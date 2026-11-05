import { analyzeVolumeSnapshots, compareVfoSnapshots, changedBytes, hex, vfoWriteBlock, bChannelWriteBlock, volumeWriteBlock } from './core.mjs?build=rx-volume-3';
import { readBlockReply } from '../../dist/protocol.mjs';
import { CHANNEL_COUNT, readChannel } from '../../dist/channel.mjs';

const $ = id => document.getElementById(id);
const MAGIC = Uint8Array.of(0x50, 0x56, 0x4f, 0x4a, 0x48, 0x5c, 0x14);
const SERVICE = '0000ff00-0000-1000-8000-00805f9b34fb';
const WRITE = '0000ff02-0000-1000-8000-00805f9b34fb';
const NOTIFY = '0000ff01-0000-1000-8000-00805f9b34fb';
const MEM_SIZE = 0x3140;
const BLOCK = 0x20;
const identityPrefix = Uint8Array.of(0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff);

let device;
let writer;
let reader;
let queue = [];
let wake;
let busy = false;
let listening = false;
let recording = false;
let checksumMode = 'auto';
const snapshots = {};
const events = [];
let safetyBackup = null;
let aprsMemory = null;
let bBefore = null;
let bAfter = null;
let bChannelProven = false;
const volumeSnapshots = {};
let volumeCandidate = null;

function log(message) {
  const line = `${new Date().toISOString()} ${message}`;
  $('log').textContent += `${line}\n`;
  $('log').scrollTop = $('log').scrollHeight;
}

function status(message, error = false) {
  $('status').textContent = message;
  $('status').classList.toggle('error', error);
  log(message);
}

function refresh() {
  const connected = Boolean(device?.gatt?.connected && writer && reader);
  $('connect').disabled = busy || connected;
  $('disconnect').disabled = busy || !connected;
  $('listen').disabled = busy || !connected || listening;
  $('stop').disabled = busy || !listening;
  $('a1').disabled = busy || !connected || listening;
  $('b').disabled = busy || !connected || listening || !snapshots.a1;
  $('a2').disabled = busy || !connected || listening || !snapshots.b;
  $('download').disabled = busy || (!snapshots.a1 && !events.length);
  $('backup').disabled = busy || !connected || listening;
  $('selectA').disabled = busy || !connected || listening || !safetyBackup;
  $('selectB').disabled = busy || !connected || listening || !safetyBackup;
  $('bBefore').disabled = busy || !connected || listening || !safetyBackup;
  $('bAfter').disabled = busy || !connected || listening || !bBefore;
  $('bAprs').disabled = busy || !connected || listening || !bChannelProven || !aprsMemory;
  $('onePass').disabled = busy || !connected || listening;
  $('volumeLow1').disabled = busy || !connected || listening;
  $('volumeHigh').disabled = busy || !connected || listening || !volumeSnapshots.low1;
  $('volumeLow2').disabled = busy || !connected || listening || !volumeSnapshots.high;
  $('volumeDownload').disabled = busy || !volumeSnapshots.low2;
  $('volumeHigher').disabled = busy || !connected || listening || !volumeCandidate;
  $('volumeLower').disabled = busy || !connected || listening || !volumeCandidate;
}

function onNotification(event) {
  const bytes = new Uint8Array(event.target.value.buffer, event.target.value.byteOffset, event.target.value.byteLength);
  if (recording) {
    const record = { at: new Date().toISOString(), hex: hex(bytes) };
    events.push(record);
    log(`BLE notification: ${record.hex}`);
  } else if (busy) {
    queue.push(...bytes);
    if (wake) { const resolve = wake; wake = null; resolve(); }
  }
}

async function readExact(length, timeoutMs = 2500) {
  const output = new Uint8Array(length);
  const deadline = Date.now() + timeoutMs;
  let offset = 0;
  while (offset < length) {
    while (queue.length && offset < length) output[offset++] = queue.shift();
    if (offset === length) break;
    const remaining = deadline - Date.now();
    if (remaining <= 0) throw new Error(`Timed out waiting for radio (${offset}/${length} bytes).`);
    await new Promise((resolve, reject) => {
      const timer = setTimeout(() => { wake = null; reject(new Error(`Timed out waiting for radio (${offset}/${length} bytes).`)); }, remaining);
      wake = () => { clearTimeout(timer); resolve(); };
    });
  }
  return output;
}

async function send(bytes) {
  if (!writer || !device?.gatt?.connected) throw new Error('Radio disconnected.');
  if (writer.properties.write) await writer.writeValueWithResponse(bytes);
  else await writer.writeValueWithoutResponse(bytes);
}

async function expectAck(label) {
  const reply = await readExact(1);
  if (reply[0] !== 0x06) throw new Error(`${label}: expected ACK 06, got ${hex(reply)}.`);
}

async function enterProgramming() {
  queue = [];
  checksumMode = 'auto';
  await send(MAGIC);
  await expectAck('Handshake');
  await send(Uint8Array.of(0x02));
  const ident = await readExact(8);
  if (!identityPrefix.every((byte, index) => ident[index] === byte) || ![0x4e, 0x48, 0x47].includes(ident[7])) {
    throw new Error(`Unexpected TD-H9 identity: ${hex(ident)}.`);
  }
  await send(Uint8Array.of(0x06));
  await expectAck('Enter programming');
  return ident;
}

async function readBlock(address) {
  await send(Uint8Array.of(0x52, address >> 8, address & 255, BLOCK));
  const reply = await readBlockReply(address, BLOCK, checksumMode, readExact, async () => {
    try { return (await readExact(1, 150))[0]; }
    catch (error) { if (error.message.startsWith('Timed out')) return null; throw error; }
  });
  checksumMode = reply.mode;
  return reply.data;
}

async function capture(label) {
  busy = true;
  refresh();
  let attemptedEntry = false;
  try {
    status(`Reading ${label}; keep the radio powered on.`);
    attemptedEntry = true;
    const ident = await enterProgramming();
    const bytes = new Uint8Array(MEM_SIZE);
    for (let address = 0; address < MEM_SIZE; address += BLOCK) {
      bytes.set(await readBlock(address), address);
      if (address % 0x800 === 0) status(`${label}: ${address.toString(16).padStart(4, '0')} / ${MEM_SIZE.toString(16)} bytes`);
    }
    snapshots[label] = { ident: hex(ident), bytes };
    renderResult();
    status(`${label} captured: ${MEM_SIZE.toLocaleString()} configuration bytes. Select the next VFO manually before continuing.`);
  } catch (error) {
    status(`${label} failed: ${error.message || error}`, true);
  } finally {
    if (attemptedEntry && device?.gatt?.connected) {
      try { await send(Uint8Array.of(0x45)); }
      catch (error) { log(`Exit programming failed: ${error.message}`); }
    }
    queue = [];
    busy = false;
    refresh();
  }
}

function same(a, b) {
  return a.length === b.length && a.every((byte, index) => byte === b[index]);
}

function downloadJson(data, filename) {
  const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function findAprsMemory(ident, bytes) {
  const image = new Uint8Array(8 + MEM_SIZE);
  image.set(ident);
  image.set(bytes, 8);
  const matches = Array.from({ length: CHANNEL_COUNT }, (_, index) => readChannel(image, index + 1))
    .filter(channel => channel.valid && channel.name.toUpperCase() === 'APRS'
      && channel.rx === '144.39000' && channel.tx === '144.39000');
  if (matches.length !== 1) throw new Error(`Expected one APRS memory at 144.390 MHz RX/TX; found ${matches.length}. No write sent.`);
  return matches[0].n;
}

function downloadSafetyBackup(ident, bytes, kind, tag = 'vfo') {
  downloadJson({
    warning: 'Private TD-H9 configuration; may contain callsign and relay password.',
    type: kind,
    identityHex: hex(ident).replaceAll(' ', ''),
    memoryHex: hex(bytes).replaceAll(' ', ''),
  }, `td-h9-before-${tag}-test-${new Date().toISOString().slice(0, 10)}.json`);
}

async function readFullMemory(label) {
  const bytes = new Uint8Array(MEM_SIZE);
  for (let address = 0; address < MEM_SIZE; address += BLOCK) {
    bytes.set(await readBlock(address), address);
    if (address % 0x800 === 0) status(`${label}: ${address.toString(16).padStart(4, '0')} / ${MEM_SIZE.toString(16)} bytes`);
  }
  return bytes;
}

async function captureVolume(which) {
  busy = true;
  delete volumeSnapshots[which];
  if (which === 'low1') {
    delete volumeSnapshots.high;
    delete volumeSnapshots.low2;
  } else if (which === 'high') {
    delete volumeSnapshots.low2;
  }
  volumeCandidate = null;
  $('volumeResult').textContent = `Reading ${which} snapshot…`;
  refresh();
  let attemptedEntry = false;
  try {
    status(`Reading RX volume ${which} snapshot.`);
    attemptedEntry = true;
    const ident = await enterProgramming();
    const bytes = await readFullMemory(`RX volume ${which}`);
    if (which !== 'low1' && !same(ident, volumeSnapshots.low1.ident)) {
      throw new Error('Radio identity changed between volume snapshots. Start again at low 1.');
    }
    volumeSnapshots[which] = { ident, bytes };
    if (which === 'low2') {
      const result = analyzeVolumeSnapshots(volumeSnapshots.low1.bytes, volumeSnapshots.high.bytes, bytes);
      volumeCandidate = result.candidate;
      const format = item => `0x${item.offset.toString(16).padStart(4, '0')}: ${item.from} → ${item.to}`;
      $('volumeResult').textContent = [
        `Changed bytes at higher volume: ${result.changes.length}`,
        ...result.changes.slice(0, 30).map(format),
        result.changes.length > 30 ? `... ${result.changes.length - 30} more in downloaded results` : '',
        `First low → returned low: ${result.drift.length} changed byte(s)`,
        result.changes.length === 0
          ? 'The clone image did not change at the higher volume. An imperfect return to low cannot explain this; there is no clone byte to test.'
          : result.candidate
            ? `One-step candidate at radio-memory 0x${result.candidate.offset.toString(16).padStart(4, '0')}: returned low ${result.candidate.low} → high ${result.candidate.high}. This is not yet proof of live RX volume control.`
            : result.volumeField
              ? `Possible volume byte at 0x${result.volumeField.offset.toString(16).padStart(4, '0')}, but high is more than one stored step above returned low. Remote writes remain disabled.`
              : 'No isolated 0–30 volume candidate. Remote volume writes remain disabled.',
      ].filter(Boolean).join('\n');
      status(result.candidate
        ? 'RX volume candidate isolated. The guarded one-step test is available.'
        : 'RX volume comparison complete; no safe one-step write candidate found.');
    } else {
      $('volumeResult').textContent = which === 'low1'
        ? 'Low 1 saved. Manually raise the radio volume, then capture high.'
        : 'High saved. Manually return near the original volume, then capture low 2. An exact match is not required.';
      status(`RX volume ${which} snapshot saved (${MEM_SIZE.toLocaleString()} bytes).`);
    }
  } catch (error) {
    $('volumeResult').textContent = `RX volume ${which} capture failed. Repeat this step before continuing.`;
    status(`RX volume ${which} failed: ${error.message || error}`, true);
  } finally {
    if (attemptedEntry && device?.gatt?.connected) {
      try { await send(Uint8Array.of(0x45)); }
      catch (error) { log(`Exit programming failed: ${error.message}`); }
    }
    queue = [];
    busy = false;
    refresh();
  }
}

function downloadVolumeResults() {
  const { low1, high, low2 } = volumeSnapshots;
  if (!low1 || !high || !low2) return;
  downloadJson({
    warning: 'Private TD-H9 configuration; may contain callsign and relay password.',
    experiment: 'RX volume low/high/low clone comparison',
    snapshots: Object.fromEntries(Object.entries(volumeSnapshots).map(([label, snapshot]) => [label, {
      identityHex: hex(snapshot.ident).replaceAll(' ', ''),
      memoryHex: hex(snapshot.bytes).replaceAll(' ', ''),
    }])),
    comparison: analyzeVolumeSnapshots(low1.bytes, high.bytes, low2.bytes),
  }, `td-h9-rx-volume-probe-${new Date().toISOString().slice(0, 10)}.json`);
}

async function tryVolume(which) {
  if (!volumeCandidate || !volumeSnapshots.low2) return;
  busy = true;
  refresh();
  let attemptedEntry = false;
  let writeIssued = false;
  try {
    status(`Checking fresh radio state before RX volume ${which} test.`);
    attemptedEntry = true;
    const ident = await enterProgramming();
    if (!same(ident, volumeSnapshots.low2.ident)) throw new Error('Radio identity differs from volume snapshots. No write sent.');
    const bytes = await readFullMemory('Pre-volume backup');
    downloadSafetyBackup(ident, bytes, 'TD-H9 clone image before RX volume experiment', 'rx-volume');
    const { offset, low, high } = volumeCandidate;
    if (!bytes.every((byte, index) => index === offset || byte === volumeSnapshots.low2.bytes[index])) {
      throw new Error('Other configuration bytes changed since the low snapshot. Backup saved; no write sent. Repeat the volume comparison.');
    }
    if (![low, high].includes(bytes[offset])) {
      throw new Error('Candidate byte is outside the observed low/high values. Backup saved; no write sent.');
    }
    const value = which === 'higher' ? high : low;
    if (bytes[offset] === value) {
      status(`Candidate already stores ${value}. Backup saved; no write sent. Check the radio's actual RX audio.`);
      return;
    }
    const address = offset & ~0x1f;
    const current = bytes.slice(address, address + BLOCK);
    writeIssued = true;
    await send(volumeWriteBlock(current, address, offset - address, value));
    await expectAck(`RX volume ${which}`);
    const verify = await readBlock(address);
    const expected = current.slice();
    expected[offset - address] = value;
    if (!same(verify, expected)) throw new Error('Write ACK received, but the 32-byte readback differs. Stop testing and keep the backup.');
    status(`Candidate 0x${offset.toString(16).padStart(4, '0')} stored ${value} and read back. Check whether the radio's received audio actually changed after programming exits.`);
  } catch (error) {
    status(`RX volume ${which} stopped: ${error.message || error}.${writeIssued ? ' A write was issued; keep the downloaded backup.' : ' No write was issued.'}`, true);
  } finally {
    if (attemptedEntry && device?.gatt?.connected) {
      try { await send(Uint8Array.of(0x45)); }
      catch (error) { log(`Exit programming failed: ${error.message}`); }
    }
    queue = [];
    busy = false;
    refresh();
  }
}

async function captureSafetyBackup() {
  busy = true;
  refresh();
  let attemptedEntry = false;
  try {
    status('Reading fresh safety backup before any VFO write.');
    attemptedEntry = true;
    const ident = await enterProgramming();
    const bytes = new Uint8Array(MEM_SIZE);
    for (let address = 0; address < MEM_SIZE; address += BLOCK) {
      bytes.set(await readBlock(address), address);
      if (address % 0x800 === 0) status(`Backup: ${address.toString(16).padStart(4, '0')} / ${MEM_SIZE.toString(16)} bytes`);
    }
    const value = bytes[0x3004];
    if (![0, 1].includes(value)) throw new Error(`Selector byte 0x3004 is ${value.toString(16)}, expected 00 or 01. No write enabled.`);
    safetyBackup = { ident, bytes };
    try { aprsMemory = findAprsMemory(ident, bytes); }
    catch (error) { aprsMemory = null; log(error.message); }
    bBefore = bAfter = null;
    bChannelProven = false;
    $('aprsMemory').textContent = aprsMemory
      ? `Found APRS memory ${aprsMemory}: RX/TX 144.390 MHz. Stored B channel: ${bytes[0x0ca5]}. B display mode: ${bytes[0x0ca3] & 3}.`
      : `Could not uniquely identify an APRS memory with RX/TX 144.390 MHz; B channel write stays disabled.`;
    $('channelStatus').textContent = 'No B channel comparison yet.';
    downloadSafetyBackup(ident, bytes, 'TD-H9 clone image before VFO experiment');
    $('writeStatus').textContent = `Safety backup downloaded; original selector is ${value === 0 ? 'A' : 'B'}. Check the download before trying a write.`;
    status(`Safety backup captured. Selector is ${value === 0 ? 'A (00)' : 'B (01)'}.`);
  } catch (error) {
    safetyBackup = null;
    aprsMemory = null;
    status(`Backup failed: ${error.message || error}`, true);
  } finally {
    if (attemptedEntry && device?.gatt?.connected) {
      try { await send(Uint8Array.of(0x45)); }
      catch (error) { log(`Exit programming failed: ${error.message}`); }
    }
    queue = [];
    busy = false;
    refresh();
  }
}

async function onePassSelectBAndAprs() {
  busy = true;
  refresh();
  let attemptedEntry = false;
  const issued = [];
  try {
    status('One-pass: reading the complete radio configuration and preparing a safety backup.');
    attemptedEntry = true;
    const ident = await enterProgramming();
    const bytes = new Uint8Array(MEM_SIZE);
    for (let address = 0; address < MEM_SIZE; address += BLOCK) {
      bytes.set(await readBlock(address), address);
      if (address % 0x800 === 0) status(`One-pass read: ${address.toString(16).padStart(4, '0')} / ${MEM_SIZE.toString(16)} bytes`);
    }
    downloadSafetyBackup(ident, bytes, 'TD-H9 clone image before one-pass B/APRS selection');
    safetyBackup = { ident, bytes };
    aprsMemory = findAprsMemory(ident, bytes);
    bBefore = bAfter = null;
    bChannelProven = false;
    const mode = bytes[0x0ca3] & 3;
    if (![1, 2].includes(mode)) throw new Error(`B display mode ${mode} is not a memory-channel mode. Backup saved; no write sent.`);
    if (![0, 1].includes(bytes[0x3004])) throw new Error('VFO selector is not 00 or 01. Backup saved; no write sent.');
    $('aprsMemory').textContent = `Found APRS memory ${aprsMemory}: RX/TX 144.390 MHz. Original B channel: ${bytes[0x0ca5]}.`;
    status(`Safety backup downloaded. APRS is memory ${aprsMemory}; B currently stores ${bytes[0x0ca5]}.`);

    const bBlock = bytes.slice(0x0ca0, 0x0cc0);
    if (bBlock[5] !== aprsMemory) {
      issued.push('B channel');
      await send(bChannelWriteBlock(bBlock, aprsMemory));
      await expectAck('One-pass B channel');
      const verify = await readBlock(0x0ca0);
      const expected = bBlock.slice();
      expected[5] = aprsMemory;
      if (!same(verify, expected)) throw new Error('B channel readback differs after write.');
      status(`B channel ${aprsMemory} written and read back.`);
    }

    // Send B selection even when B is already selected: test whether an explicit
    // selector write makes the radio reload the newly stored channel reliably.
    const selectorBlock = bytes.slice(0x3000, 0x3020);
    issued.push('VFO B selector');
    await send(vfoWriteBlock(selectorBlock, 1));
    await expectAck('One-pass VFO B selector');
    const verify = await readBlock(0x3000);
    const expected = selectorBlock.slice();
    expected[4] = 1;
    if (!same(verify, expected)) throw new Error('VFO B selector readback differs after write.');
    $('onePassStatus').textContent = `Stored B memory ${aprsMemory} and selected VFO B; both written blocks read back correctly. Check the physical display after programming exits.`;
    status(`One-pass complete: B → APRS memory ${aprsMemory} at 144.390 MHz, selector B verified. Check the radio display.`);
  } catch (error) {
    const detail = issued.length ? ` Write commands issued: ${issued.join(', ')}.` : ' No write command sent.';
    $('onePassStatus').textContent = `Stopped: ${error.message || error}.${detail} Keep the downloaded backup.`;
    status(`One-pass stopped: ${error.message || error}.${detail}`, true);
  } finally {
    if (attemptedEntry && device?.gatt?.connected) {
      try { await send(Uint8Array.of(0x45)); }
      catch (error) { log(`Exit programming failed: ${error.message}`); }
    }
    queue = [];
    busy = false;
    refresh();
  }
}

async function readBChannelState(which) {
  busy = true;
  if (which === 'after') { bAfter = null; bChannelProven = false; }
  refresh();
  let attemptedEntry = false;
  try {
    status(`Reading B channel ${which} state.`);
    attemptedEntry = true;
    const ident = await enterProgramming();
    if (!same(ident, safetyBackup.ident)) throw new Error('Radio identity differs from safety backup.');
    const selectorBlock = await readBlock(0x3000);
    if (selectorBlock[4] !== 1) throw new Error('Select VFO B first, then retry. No write sent.');
    const block = await readBlock(0x0ca0);
    const state = { block, channel: block[5], mode: block[3] & 3 };
    if (which === 'before') {
      bBefore = state;
      bAfter = null;
      bChannelProven = false;
      $('channelStatus').textContent = `Before: B memory channel ${state.channel}, display mode ${state.mode}. Now choose a different stored channel on the radio, then read after.`;
    } else {
      bAfter = state;
      const changes = changedBytes(bBefore.block, bAfter.block);
      bChannelProven = changes.length === 1 && changes[0].offset === 5
        && bBefore.channel >= 1 && bBefore.channel <= CHANNEL_COUNT
        && bAfter.channel >= 1 && bAfter.channel <= CHANNEL_COUNT;
      $('channelStatus').textContent = `B channel ${bBefore.channel} → ${bAfter.channel}; block changes: ${changes.map(change => `0x${(0x0ca0 + change.offset).toString(16)}`).join(', ') || 'none'}. ${bChannelProven ? 'The B channel byte is isolated; set-to-APRS test unlocked.' : 'The channel byte was not isolated; no write will be offered.'}`;
    }
    status(`B ${which}: memory channel ${state.channel}, mode ${state.mode}.`);
  } catch (error) {
    status(`B ${which} read failed: ${error.message || error}`, true);
  } finally {
    if (attemptedEntry && device?.gatt?.connected) {
      try { await send(Uint8Array.of(0x45)); }
      catch (error) { log(`Exit programming failed: ${error.message}`); }
    }
    queue = [];
    busy = false;
    refresh();
  }
}

async function setBToAprs() {
  if (!safetyBackup || !bChannelProven || !aprsMemory) return;
  busy = true;
  refresh();
  let attemptedEntry = false;
  try {
    status(`Checking B before setting APRS memory ${aprsMemory}.`);
    attemptedEntry = true;
    const ident = await enterProgramming();
    if (!same(ident, safetyBackup.ident)) throw new Error('Radio identity differs from safety backup. No write sent.');
    const selectorBlock = await readBlock(0x3000);
    if (![0, 1].includes(selectorBlock[4])) throw new Error('VFO selector is not 00 or 01. No write sent.');
    const originalSelector = safetyBackup.bytes.slice(0x3000, 0x3020);
    if (!selectorBlock.every((byte, index) => index === 4 || byte === originalSelector[index])) {
      throw new Error('Other settings in block 0x3000 changed since backup. No write sent. Capture a new safety backup.');
    }
    const current = await readBlock(0x0ca0);
    if (!same(current, bAfter.block)) throw new Error('B channel settings changed since the manual test. No write sent. Repeat the before/after reads.');
    const original = safetyBackup.bytes.slice(0x0ca0, 0x0cc0);
    if (!current.every((byte, index) => index === 5 || byte === original[index])) {
      throw new Error('Other settings in block 0x0CA0 changed since backup. No write sent. Capture a new safety backup.');
    }
    let channelWritten = false;
    if (current[5] !== aprsMemory) {
      await send(bChannelWriteBlock(current, aprsMemory));
      await expectAck('Set B APRS channel');
      const verify = await readBlock(0x0ca0);
      const expected = current.slice();
      expected[5] = aprsMemory;
      if (!same(verify, expected)) throw new Error('Write was acknowledged, but B channel readback differs. Stop testing and keep the safety backup.');
      channelWritten = true;
    }
    if (selectorBlock[4] !== 1) {
      await send(vfoWriteBlock(selectorBlock, 1));
      await expectAck('Select VFO B');
      const verify = await readBlock(0x3000);
      const expected = selectorBlock.slice();
      expected[4] = 1;
      if (!same(verify, expected)) throw new Error('VFO B selection readback differs. Stop testing and keep the safety backup.');
    }
    const detail = channelWritten ? `B memory channel ${aprsMemory} written and verified` : `B already stored memory channel ${aprsMemory}`;
    bChannelProven = false;
    $('channelStatus').textContent = `${detail}; VFO B selected. Confirm the display after programming exits.`;
    status(`${detail}; VFO B selection checked. Look for APRS at 144.390 MHz on B.`);
  } catch (error) {
    status(`B APRS test stopped: ${error.message || error}`, true);
  } finally {
    if (attemptedEntry && device?.gatt?.connected) {
      try { await send(Uint8Array.of(0x45)); }
      catch (error) { log(`Exit programming failed: ${error.message}`); }
    }
    queue = [];
    busy = false;
    refresh();
  }
}

async function trySelect(value) {
  if (!safetyBackup) return;
  busy = true;
  refresh();
  let attemptedEntry = false;
  const target = value === 0 ? 'A' : 'B';
  try {
    status(`Testing selector ${target}; checking radio identity and current block first.`);
    attemptedEntry = true;
    const ident = await enterProgramming();
    if (!same(ident, safetyBackup.ident)) throw new Error('Radio identity differs from safety backup. No write sent.');
    const current = await readBlock(0x3000);
    const original = safetyBackup.bytes.slice(0x3000, 0x3020);
    if (![0, 1].includes(current[4])) throw new Error('Current selector is not 00 or 01. No write sent.');
    if (!current.every((byte, index) => index === 4 || byte === original[index])) {
      throw new Error('Other settings in block 0x3000 changed since backup. No write sent. Capture a new safety backup.');
    }
    if (current[4] === value) {
      status(`Radio already stores ${target} at 0x3004. No write sent.`);
      return;
    }
    await send(vfoWriteBlock(current, value));
    await expectAck(`Select ${target}`);
    const verify = await readBlock(0x3000);
    const expected = current.slice();
    expected[4] = value;
    if (!same(verify, expected)) throw new Error('Write was acknowledged, but block readback differs. Stop testing and keep the safety backup.');
    $('writeStatus').textContent = `Selector ${target} stored and read back. Check whether the radio display actually switched after leaving programming mode.`;
    status(`Selector ${target} write/readback verified. Check the radio display; live VFO behavior is not yet proven.`);
  } catch (error) {
    status(`Selector ${target} test stopped: ${error.message || error}`, true);
  } finally {
    if (attemptedEntry && device?.gatt?.connected) {
      try { await send(Uint8Array.of(0x45)); }
      catch (error) { log(`Exit programming failed: ${error.message}`); }
    }
    queue = [];
    busy = false;
    refresh();
  }
}

function renderResult() {
  const present = ['a1', 'b', 'a2'].filter(label => snapshots[label]);
  const lines = [`Snapshots: ${present.join(', ') || 'none'}`];
  if (present.length === 3) {
    const ids = new Set(present.map(label => snapshots[label].ident));
    if (ids.size !== 1) lines.push('Radio identity changed between snapshots; comparison is invalid.');
    else {
      const { stable, unstable } = compareVfoSnapshots(snapshots.a1.bytes, snapshots.b.bytes, snapshots.a2.bytes);
      lines.push(`Repeatable A/B differences: ${stable.length}`);
      lines.push(`A1/A2 differences (unreliable or changing settings): ${unstable.length}`);
      for (const item of stable.slice(0, 100)) {
        lines.push(`0x${item.offset.toString(16).padStart(4, '0')}: A=${item.a.toString(16).padStart(2, '0')} B=${item.b.toString(16).padStart(2, '0')}`);
      }
      if (stable.length > 100) lines.push(`... ${stable.length - 100} more in downloaded JSON`);
      if (!stable.length) lines.push('No stable clone-image byte tracked the selected VFO. A live-control command would need a separate protocol investigation.');
    }
  }
  $('result').textContent = lines.join('\n');
}

function disconnect() {
  if (device?.gatt?.connected) device.gatt.disconnect();
  writer = null;
  reader = null;
  queue = [];
  listening = recording = false;
  status('Disconnected.');
  refresh();
}

async function connect() {
  busy = true;
  refresh();
  try {
    if (!navigator.bluetooth) throw new Error('Use desktop Chrome or Edge on localhost.');
    status('Choose the TD-H9 Bluetooth programming device.');
    device = await navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'TD-H9' }], optionalServices: [SERVICE] });
    device.addEventListener('gattserverdisconnected', disconnect);
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE);
    writer = await service.getCharacteristic(WRITE);
    reader = await service.getCharacteristic(NOTIFY);
    reader.addEventListener('characteristicvaluechanged', onNotification);
    await reader.startNotifications();
    status(`Connected to ${device.name || 'TD-H9'}. No radio commands sent.`);
  } catch (error) {
    status(`Connection failed: ${error.message || error}`, true);
    if (device?.gatt?.connected) device.gatt.disconnect();
    writer = reader = null;
  } finally {
    busy = false;
    refresh();
  }
}

function download() {
  const data = {
    experiment: 'TD-H9 A/B VFO state comparison',
    warning: 'Configuration snapshots may include callsign and relay password. Keep this file private.',
    notifications: events,
    snapshots: Object.fromEntries(Object.entries(snapshots).map(([label, snapshot]) => [label, {
      identity: snapshot.ident,
      memoryHex: hex(snapshot.bytes).replaceAll(' ', ''),
    }])),
  };
  if (snapshots.a1 && snapshots.b && snapshots.a2) {
    const { stable, unstable } = compareVfoSnapshots(snapshots.a1.bytes, snapshots.b.bytes, snapshots.a2.bytes);
    data.comparison = { stable, unstable };
  }
  downloadJson(data, `td-h9-vfo-probe-${new Date().toISOString().slice(0, 10)}.json`);
}

$('connect').addEventListener('click', connect);
$('disconnect').addEventListener('click', disconnect);
$('listen').addEventListener('click', () => {
  listening = recording = true;
  status('Listening. Manually change A/B or RX volume on the radio, then Stop listening.');
  refresh();
});
$('stop').addEventListener('click', () => {
  listening = recording = false;
  status(`Stopped listening. ${events.length} notification(s) captured.`);
  refresh();
});
for (const label of ['a1', 'b', 'a2']) $(label).addEventListener('click', () => capture(label));
$('download').addEventListener('click', download);
$('backup').addEventListener('click', captureSafetyBackup);
$('selectA').addEventListener('click', () => trySelect(0));
$('selectB').addEventListener('click', () => trySelect(1));
$('bBefore').addEventListener('click', () => readBChannelState('before'));
$('bAfter').addEventListener('click', () => readBChannelState('after'));
$('bAprs').addEventListener('click', setBToAprs);
$('onePass').addEventListener('click', onePassSelectBAndAprs);
$('volumeLow1').addEventListener('click', () => captureVolume('low1'));
$('volumeHigh').addEventListener('click', () => captureVolume('high'));
$('volumeLow2').addEventListener('click', () => captureVolume('low2'));
$('volumeDownload').addEventListener('click', downloadVolumeResults);
$('volumeHigher').addEventListener('click', () => tryVolume('higher'));
$('volumeLower').addEventListener('click', () => tryVolume('lower'));
$('volumeResult').textContent = 'RX volume test loaded. Connect the radio, set a midrange volume, then capture low 1.';
refresh();
