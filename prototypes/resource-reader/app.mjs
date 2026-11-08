import { BANK_SIZE, BLOCK, BULK_HEADER, BULK_SIZES, LINK_FAST_ADDRESS, LOGO_ADDRESS, LOGO_BYTES, LOGO_HEIGHT, LOGO_WIDTH, STOCK_LOGO_SHA256, logoFromBin, logoSectorPlan, logoToRgba, rgbaToLogo, LINK_FIXED_ADDRESS, LINK_STOCK_ADDRESS, CHIP_SIZE, OP, PARTITIONS, SECTOR, assembleBulk, bulkSlotAddress, checkRange, isBulkPacket, parseBulkReply, controlFrame, crc16, dataFrame, describeJedec, hex, parseStatus, readPlan, statusAddress, testPattern, writable } from '../../dist/boot-logo/resource.mjs?build=logo-3';
import { readBlockReply } from '../../dist/protocol.mjs';

const $ = id => document.getElementById(id);
const MAGIC = Uint8Array.of(0x50, 0x56, 0x4f, 0x4a, 0x48, 0x5c, 0x14);
const SERVICE = '0000ff00-0000-1000-8000-00805f9b34fb';
const WRITE = '0000ff02-0000-1000-8000-00805f9b34fb';
const NOTIFY = '0000ff01-0000-1000-8000-00805f9b34fb';
const identityPrefix = Uint8Array.of(0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff);
const MAX_RETRIES = 5;

let device;
let writer;
let reader;
let queue = [];
let wake;
let busy = false;
let stopRequested = false;
let checksumMode = 'auto';
let serviceStatus = null;
let lastDump = null;
let totalPasses = 1;
// Boot logo: what the radio holds now (read this session, then kept in step
// with each completed sector) and the logo prepared for writing.
let currentLogo = null;
let nextLogo = null;
// Progress goes to the bar of the section that started the operation. A
// read inside a longer job (the logo write's final check) fills only its
// share of that job's bar: [start, start + span].
let progressIds = ['progress', 'progressText'];
let progressShare = { start: 0, span: 1, prefix: '' };

function useProgress(bar, label) {
  progressIds = [bar, label];
  progressShare = { start: 0, span: 1, prefix: '' };
  showProgress(0, 'Starting…');
}

function showProgress(fraction, text) {
  const [bar, label] = progressIds;
  if (fraction !== null) $(bar).value = progressShare.start + Math.min(1, Math.max(0, fraction)) * progressShare.span;
  $(label).textContent = progressShare.prefix + text;
}

function formatDuration(seconds) {
  if (!Number.isFinite(seconds)) return '…';
  const whole = Math.max(1, Math.ceil(seconds));
  if (whole < 90) return `${whole} s`;
  return `${Math.floor(whole / 60)} min ${String(whole % 60).padStart(2, '0')} s`;
}
let bulkPackets = [];
// 176-byte packets (180 with the header) fit the 182-byte link payload seen on
// macOS; the page adjusts if the first notifications show otherwise.
let bulkSizeIndex = BULK_SIZES.indexOf(176);
let bulkWait = true;
// While a fast read is running (bulkActive): the packet being reassembled (the
// stack splits notifications larger than the link MTU) and the first
// notification sizes seen, which reveal that MTU.
let bulkActive = false;
let bulkPartial = null;
let notificationSizes = [];

// Console output: [h9] prefix. debug = raw BLE traffic, info = per-request
// summaries, warn = retries and size changes, error = failures. In DevTools,
// enable the Verbose level to see debug lines. window.h9 exposes live state.
function log(message, level = 'log') {
  $('log').textContent += `${new Date().toISOString()} ${message}\n`;
  $('log').scrollTop = $('log').scrollHeight;
  console[level]('[h9]', message);
}

function status(message, error = false) {
  $('status').textContent = message;
  $('status').classList.toggle('error', error);
  log(message, error ? 'error' : 'log');
}

window.h9 = {
  get queue() { return queue.slice(); },
  get bulkPackets() { return bulkPackets.slice(); },
  get bulkPartial() { return bulkPartial; },
  get bulkSize() { return BULK_SIZES[bulkSizeIndex]; },
  get notificationSizes() { return notificationSizes.slice(); },
  get serviceStatus() { return serviceStatus; },
  get lastDump() { return lastDump?.manifest; },
};

function refresh() {
  const connected = Boolean(device?.gatt?.connected && writer && reader);
  $('connect').disabled = busy || connected;
  $('disconnect').disabled = busy || !connected;
  $('check').disabled = busy || !connected;
  $('dump').disabled = busy || !connected || !serviceStatus?.ready;
  $('stop').disabled = !busy;
  $('downloadBin').disabled = busy || !lastDump;
  $('downloadManifest').disabled = busy || !lastDump;
  const s2 = serviceStatus?.version === 2 && serviceStatus.ready;
  $('sectorTest').disabled = busy || !connected || !s2;
  $('eraseSector').disabled = busy || !connected || !s2 || !$('eraseConfirm').checked;
  $('readLogo').disabled = busy || !connected || !serviceStatus?.ready;
  $('downloadLogo').disabled = busy || !currentLogo;
  $('writeLogo').disabled = busy || !connected || !s2 || !currentLogo || !nextLogo || !$('logoConfirm').checked;
}

function onNotification(event) {
  const view = event.target.value;
  let rest = new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
  console.debug('[h9] notify', rest.length, hex(rest.subarray(0, 12)), bulkPartial ? `continues packet (${bulkPartial.filled}/${bulkPartial.bytes.length})` : '');
  if (bulkActive && notificationSizes.length < 64) notificationSizes.push(rest.length);
  while (rest.length) {
    if (bulkPartial) {
      const take = Math.min(rest.length, bulkPartial.bytes.length - bulkPartial.filled);
      bulkPartial.bytes.set(rest.subarray(0, take), bulkPartial.filled);
      bulkPartial.filled += take;
      rest = rest.subarray(take);
      if (bulkPartial.filled === bulkPartial.bytes.length) {
        bulkPackets.push(bulkPartial.bytes);
        bulkPartial = null;
      }
    } else if (bulkActive && rest.length >= BULK_HEADER && rest[0] === 0xb7 && rest[1] === 0x5a) {
      bulkPartial = { bytes: new Uint8Array(rest[3] + BULK_HEADER), filled: 0 };
    } else {
      queue.push(...rest);
      rest = rest.subarray(rest.length);
    }
  }
  if (wake) { const resolve = wake; wake = null; resolve(); }
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
  console.debug('[h9] write', bytes.length, hex(bytes.subarray(0, 16)));
  // Write without response skips the ATT write acknowledgement, so each
  // request reaches the radio sooner. Used only when the box is ticked and the
  // characteristic supports it.
  if ($('noResponse').checked && writer.properties.writeWithoutResponse) await writer.writeValueWithoutResponse(bytes);
  else if (writer.properties.write) await writer.writeValueWithResponse(bytes);
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
}

async function leaveProgramming() {
  try { await send(Uint8Array.of(0x45)); } catch { /* radio already gone */ }
  await new Promise(resolve => setTimeout(resolve, 300));
  queue = [];
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

async function selectBank(bank) {
  return parseStatus(await readBlock(statusAddress(bank)), bank);
}

async function connect() {
  busy = true;
  refresh();
  try {
    if (!navigator.bluetooth) throw new Error('Use desktop Chrome or Edge on localhost.');
    status('Choose the TD-H9 Bluetooth programming device.');
    device = await navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'TD-H9' }], optionalServices: [SERVICE] });
    device.addEventListener('gattserverdisconnected', disconnected);
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE);
    writer = await service.getCharacteristic(WRITE);
    reader = await service.getCharacteristic(NOTIFY);
    reader.addEventListener('characteristicvaluechanged', onNotification);
    await reader.startNotifications();
    status(`Connected to ${device.name || 'TD-H9'}. No radio commands sent.`);
    log(`Write characteristic: write ${writer.properties.write ? 'yes' : 'no'}, write without response ${writer.properties.writeWithoutResponse ? 'yes' : 'no'}.`, 'info');
  } catch (error) {
    status(`Connection failed: ${error.message || error}`, true);
    if (device?.gatt?.connected) device.gatt.disconnect();
    writer = reader = null;
  } finally {
    busy = false;
    refresh();
  }
}

function disconnected() {
  writer = reader = null;
  serviceStatus = null;
  status('Radio disconnected.', busy);
  refresh();
}

async function checkService() {
  busy = true;
  refresh();
  try {
    status('Checking for the resource service (one status read).');
    await enterProgramming();
    serviceStatus = await selectBank(0);
    const jedec = describeJedec(serviceStatus.jedec);
    $('serviceResult').textContent = [
      `Service version: ${serviceStatus.version}`,
      `Flash ready (0x5AA5): ${serviceStatus.ready ? 'yes' : 'no'}`,
      serviceStatus.write ? `Write service: ${serviceStatus.write.resultText}, ${serviceStatus.write.operations} operations since boot` : 'Write service: not in this image (read-only S1)',
      `JEDEC ID: ${jedec.text}`,
      jedec.matches16MiB ? 'Capacity matches the 16 MiB partition table.' : 'Capacity does NOT match 16 MiB; do not trust a dump until this is explained.',
    ].join('\n');
    status(serviceStatus.ready ? 'Resource service answered and flash is ready.' : 'Service answered, but stock has not marked the flash ready.', !serviceStatus.ready);
  } catch (error) {
    serviceStatus = null;
    $('serviceResult').textContent = error.message;
    status(`Service check failed: ${error.message}`, true);
  } finally {
    await leaveProgramming();
    busy = false;
    refresh();
  }
}

function selectedRange() {
  const choice = $('region').value;
  if (choice === 'chip') return { label: 'chip', start: 0, length: CHIP_SIZE };
  if (choice === 'custom') {
    const start = Number.parseInt($('start').value, 16);
    const length = Number.parseInt($('length').value, 16);
    checkRange(start, length);
    return { label: `custom-${start.toString(16)}`, start, length };
  }
  const part = PARTITIONS[Number(choice)];
  return { label: part.name, start: part.origin, length: part.length };
}

async function readPass(range, pass) {
  const output = new Uint8Array(range.length);
  const steps = [...readPlan(range.start, range.length)];
  const blocks = range.length / BLOCK;
  const began = performance.now();
  let done = 0;
  let index = 0;
  let bank = -1;
  let retries = 0;
  await enterProgramming();
  while (index < steps.length) {
    if (stopRequested) throw new Error('Stopped by user.');
    const step = steps[index];
    try {
      if (step.kind === 'bank') {
        const state = await selectBank(step.bank);
        if (!state.ready) throw new Error('Flash stopped reporting ready.');
        bank = step.bank;
      } else {
        if (Math.floor(step.chip / 0x8000) !== bank) throw new Error('Bank selection lost.');
        output.set(await readBlock(step.address), step.chip - range.start);
        done++;
        if (done % 64 === 0 || done === blocks) {
          const seconds = (performance.now() - began) / 1000;
          const rate = done / seconds;
          showProgress(done / blocks, `Pass ${pass} of ${totalPasses}: ${done}/${blocks} blocks, ${(rate * BLOCK / 1024).toFixed(1)} KiB/s, ${remainingText((blocks - done) / rate, pass, range.length, rate * BLOCK)}`);
        }
      }
      index++;
      retries = 0;
    } catch (error) {
      if (stopRequested || ++retries > MAX_RETRIES) throw error;
      log(`Retry ${retries} at ${step.kind === 'bank' ? `bank ${step.bank}` : `0x${step.chip.toString(16)}`}: ${error.message}`);
      await leaveProgramming();
      await enterProgramming();
      // Reselect the bank before resuming data reads.
      while (index > 0 && steps[index].kind !== 'bank') index--;
      done = steps.slice(0, index).filter(entry => entry.kind === 'data').length;
      bank = -1;
    }
  }
  await leaveProgramming();
  return { bytes: output, seconds: (performance.now() - began) / 1000 };
}

// "about 4 min left in this pass, 12 min total" at the current rate; the
// total includes the passes still to come.
function remainingText(passSeconds, pass, passBytes, bytesPerSecond) {
  const later = (totalPasses - pass) * passBytes / bytesPerSecond;
  const format = formatDuration;
  return totalPasses > pass
    ? `about ${format(passSeconds)} left in this pass, ${format(passSeconds + later)} total`
    : `about ${format(passSeconds)} left`;
}

class BulkUnsupported extends Error {}

const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

// Bulk read: the radio streams up to 2 KiB per request from a cursor set by
// the bank-select status read. Every byte is matched to the radio's report.
async function readFastPass(range, pass) {
  if (range.start % BANK_SIZE) throw new BulkUnsupported('Fast read needs a 32 KiB aligned start.');
  const output = new Uint8Array(range.length);
  const end = range.start + range.length;
  const stats = { requests: 0, queue_full_waits: 0, size_reductions: 0, capacities: [], wait_recoveries: 0, bytes: 0 };
  notificationSizes = [];
  let sizesLogged = false;
  const began = performance.now();
  let pos = range.start;
  let retries = 0;
  await enterProgramming();
  await selectBank(pos / BANK_SIZE);
  let linkRequested = false;
  const linkMode = $('fastLink').value;
  if (linkMode !== 'off') {
    const linkLabel = linkMode === 'fixed' ? 'fixed 15 ms' : '15-30 ms';
    const link = parseBulkReply(await readBlock(linkMode === 'fixed' ? LINK_FIXED_ADDRESS : LINK_FAST_ADDRESS));
    if (!link) {
      log('This image has no link-parameter slot; continuing with the current link.', 'warn');
    } else {
      linkRequested = link.status === 0;
      log(`Requested a ${linkLabel}, latency-0 link: ${link.statusText}${link.status === 8 ? ` (stack code ${link.count})` : ''}. Waiting 1.5 s for macOS to apply it.`, link.status === 0 ? 'info' : 'warn');
      stats.link_request = `${linkLabel}: ${link.statusText}`;
      await sleep(1500);
    }
  }
  const timing = { started: performance.now(), requests: 0 };
  while (pos < end) {
    if (stopRequested) throw new Error('Stopped by user.');
    try {
      bulkPackets = [];
      bulkPartial = null;
      bulkActive = true;
      const reply = parseBulkReply(await readBlock(bulkSlotAddress(bulkSizeIndex, bulkWait)));
      if (!sizesLogged && reply) {
        sizesLogged = true;
        log(`First bulk request (${BULK_SIZES[bulkSizeIndex]}-byte packets, ${bulkWait ? 'waiting for TX room' : 'no waiting'}): ${reply.count} bytes, TX capacity ${reply.capacity}, polls that found room ${reply.waitRecoveries}, notification sizes ${notificationSizes.slice(0, 24).join(' ')}`, 'info');
        if (bulkWait && reply.waitRecoveries === 0 && reply.count < 2048) {
          bulkWait = false;
          log('Waiting inside the radio did not free TX room; switching to no-wait requests.', 'warn');
        }
        // The first notification of a split packet is one full link payload.
        // Use the largest size that fits it, so packets are never split
        // (on macOS the payload was 182: 176-byte data packets, two per request).
        const linkPayload = notificationSizes[0];
        if (linkPayload && linkPayload < BULK_SIZES[bulkSizeIndex] + BULK_HEADER) {
          const fitting = BULK_SIZES.findLastIndex(size => size + BULK_HEADER <= linkPayload);
          if (fitting >= 0) {
            log(`Link payload is ${linkPayload} bytes; switching to ${BULK_SIZES[fitting]}-byte packets for the next requests.`, 'warn');
            bulkSizeIndex = fitting;
            stats.size_reductions++;
          }
        }
      }
      if (!reply) throw new BulkUnsupported('This image has no bulk read (reply was not H9BK).');
      stats.requests++;
      stats.capacities.push(reply.capacity);
      console.info('[h9] bulk reply', { size: BULK_SIZES[bulkSizeIndex], ...reply, packets: bulkPackets.length, partial: bulkPartial?.filled ?? null, position: `0x${pos.toString(16)}` });
      if (reply.count === 0 && reply.status === 6 && bulkSizeIndex > 0) {
        bulkSizeIndex--;
        stats.size_reductions++;
        log(`Notification of ${BULK_SIZES[bulkSizeIndex + 1]} bytes refused; trying ${BULK_SIZES[bulkSizeIndex]}.`, 'warn');
        continue;
      }
      if (reply.count === 0 && reply.status === 3) {
        stats.queue_full_waits++;
        await sleep(30);
        continue;
      }
      if (reply.count === 0) throw new Error(`Bulk read: ${reply.statusText}`);
      if (reply.start !== pos) throw new Error(`Radio cursor at 0x${reply.start.toString(16)}, expected 0x${pos.toString(16)}.`);
      let data;
      try {
        data = assembleBulk(bulkPackets, reply, BULK_SIZES[bulkSizeIndex]);
      } catch (error) {
        if (/expected \d+\.$/.test(error.message) && bulkSizeIndex > 0) {
          bulkSizeIndex--;
          stats.size_reductions++;
        }
        throw error;
      }
      stats.wait_recoveries += reply.waitRecoveries;
      stats.bytes += data.length;
      timing.requests++;
      if (timing.requests === 50 || timing.requests % 500 === 0) {
        const roundTrip = (performance.now() - timing.started) / timing.requests;
        stats.round_trip_ms = Math.round(roundTrip);
        log(`Average round trip over ${timing.requests} requests: ${roundTrip.toFixed(0)} ms (${(stats.bytes / stats.requests / roundTrip * 1000 / 1024).toFixed(1)} KiB/s)`, 'info');
      }
      output.set(data.subarray(0, Math.min(data.length, end - pos)), pos - range.start);
      pos += data.length;
      if (!reply.cursorValid || reply.cursor !== (pos & 0xffffff)) throw new Error('Radio cursor disagrees with the bytes received.');
      retries = 0;
      const seconds = (performance.now() - began) / 1000;
      const doneBytes = Math.min(pos, end) - range.start;
      const rate = doneBytes / seconds;
      showProgress(doneBytes / range.length, `Pass ${pass} of ${totalPasses} (fast, ${BULK_SIZES[bulkSizeIndex]}-byte packets, ${bulkWait ? 'waiting' : 'no wait'}, ${Math.round(stats.bytes / stats.requests)} B/request): ${(rate / 1024).toFixed(1)} KiB/s, ${remainingText(Math.max(0, end - pos) / rate, pass, range.length, rate)}`);
    } catch (error) {
      if (bulkPartial && bulkSizeIndex > 0) {
        // A packet never completed: the stack truncated rather than split.
        log(`Bulk packet incomplete (${bulkPartial.filled}/${bulkPartial.bytes.length} bytes); notification sizes ${notificationSizes.slice(0, 24).join(' ')}`, 'warn');
        bulkSizeIndex--;
        stats.size_reductions++;
      }
      bulkPartial = null;
      if (error instanceof BulkUnsupported || stopRequested || ++retries > MAX_RETRIES) { bulkActive = false; throw error; }
      log(`Fast retry ${retries} at 0x${pos.toString(16)}: ${error.message}`, 'warn');
      console.warn('[h9] fast retry detail', error, { queue: queue.slice(0, 40), packets: bulkPackets.length, sizes: notificationSizes.slice(0, 24) });
      await leaveProgramming();
      await enterProgramming();
      pos = Math.floor(pos / BANK_SIZE) * BANK_SIZE;
      await selectBank(pos / BANK_SIZE);
    }
  }
  bulkActive = false;
  if (linkRequested) {
    const restored = parseBulkReply(await readBlock(LINK_STOCK_ADDRESS));
    log(`Restored stock link parameters: ${restored?.statusText ?? 'no reply'}.`, 'info');
  }
  await leaveProgramming();
  return { bytes: output, seconds: (performance.now() - began) / 1000, method: 'fast', stats: { ...stats, packet_bytes: BULK_SIZES[bulkSizeIndex], waiting: bulkWait, bytes_per_request: Math.round(stats.bytes / Math.max(1, stats.requests)), notification_sizes: notificationSizes.slice(0, 24) } };
}

async function readAnyPass(range, pass) {
  if ($('fast').checked) {
    try {
      return await readFastPass(range, pass);
    } catch (error) {
      if (!(error instanceof BulkUnsupported)) throw error;
      log(`${error.message} Using 32-byte reads.`);
      await leaveProgramming();
    }
  }
  return { ...(await readPass(range, pass)), method: '32-byte' };
}

async function sha256(bytes) {
  return hex(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes))).replaceAll(' ', '');
}

async function dump() {
  busy = true;
  stopRequested = false;
  refresh();
  useProgress('progress', 'progressText');
  try {
    const range = selectedRange();
    const passes = $('verify').checked ? 2 : 1;
    totalPasses = passes;
    status(`Reading ${range.label} (0x${range.start.toString(16)}, ${range.length} bytes), ${passes} pass(es). Keep the radio on and nearby.`);
    const first = await readAnyPass(range, 1);
    const hashes = [await sha256(first.bytes)];
    let differingBlocks = null;
    if (passes === 2) {
      const second = await readAnyPass(range, 2);
      hashes.push(await sha256(second.bytes));
      differingBlocks = 0;
      for (let offset = 0; offset < range.length; offset += BLOCK) {
        if (first.bytes.subarray(offset, offset + BLOCK).some((byte, i) => byte !== second.bytes[offset + i])) differingBlocks++;
      }
    }
    lastDump = {
      bytes: first.bytes,
      manifest: {
        kind: 'td-h9-resource-flash-dump',
        service_version: serviceStatus?.version,
        jedec: hex(serviceStatus?.jedec || []),
        region: range.label,
        start: `0x${range.start.toString(16)}`,
        length: range.length,
        sha256: hashes[0],
        pass_sha256: hashes,
        passes_match: passes === 2 ? hashes[0] === hashes[1] : null,
        differing_blocks: differingBlocks,
        erased_blocks: countErased(first.bytes),
        seconds_first_pass: Math.round(first.seconds),
        method: first.method,
        write_without_response: $('noResponse').checked && Boolean(writer?.properties.writeWithoutResponse),
        bulk: first.stats ? { ...first.stats, capacities: undefined, capacity_min: Math.min(...first.stats.capacities), capacity_max: Math.max(...first.stats.capacities) } : null,
        captured: new Date().toISOString(),
        warning: 'sys_data and rw_freq contain personal radio data (callsign, channels, messages). Keep dumps private.',
      },
    };
    $('dumpResult').textContent = JSON.stringify(lastDump.manifest, null, 2);
    suggestErasedSector(range, first.bytes);
    const consistent = passes === 1 || hashes[0] === hashes[1];
    status(consistent ? `Read ${range.label}: SHA-256 ${hashes[0]}.` : `Passes DIFFER in ${differingBlocks} blocks; do not use this dump as a backup.`, !consistent);
  } catch (error) {
    console.error('[h9] dump failed', error);
    status(`Dump failed: ${error.message}`, true);
    await leaveProgramming();
  } finally {
    busy = false;
    refresh();
  }
}

// Offer the last fully erased sector of a writable dump for the S2 test.
function suggestErasedSector(range, bytes) {
  const erased = [];
  for (let offset = 0; offset + SECTOR <= bytes.length; offset += SECTOR) {
    const address = range.start + offset;
    if (address % SECTOR === 0 && writable(address) && bytes.subarray(offset, offset + SECTOR).every(byte => byte === 0xff)) erased.push(address);
  }
  $('erasedSectors').textContent = erased.length
    ? `${erased.length} erased writable sectors in this dump; last is 0x${erased.at(-1).toString(16)}.`
    : 'No fully erased writable sector in this dump.';
  if (erased.length && !$('sector').value) $('sector').value = erased.at(-1).toString(16);
}

function sectorInput() {
  const address = Number.parseInt($('sector').value, 16);
  if (!Number.isInteger(address) || address % SECTOR || address < 0 || address >= CHIP_SIZE) throw new Error('Enter a 4 KiB aligned chip address in hex.');
  if (!writable(address)) throw new Error('Only voice, font, and resource sectors are writable.');
  return address;
}

async function sendWrite(frame) {
  await send(frame);
  await expectAck('Write');
}

// Status read through the sector's own bank, so later data reads need no reselect.
async function writeStatus(address) {
  const state = await selectBank(Math.floor(address / 0x8000));
  if (state.version !== 2) throw new Error('This radio runs the read-only S1 image.');
  if (!state.ready) throw new Error('Flash is not ready.');
  return state.write;
}

async function expectWrite(address, label, check) {
  const state = await writeStatus(address);
  if (state.result !== 0 || (check && !check(state))) {
    throw new Error(`${label}: radio reports ${state.resultText} (pointer 0x${state.pointer.toString(16)}, CRC 0x${state.crc.toString(16)}).`);
  }
  return state;
}

async function readSector(address) {
  await writeStatus(address);
  const output = new Uint8Array(SECTOR);
  for (let offset = 0; offset < SECTOR; offset += BLOCK) {
    output.set(await readBlock(0x8000 + ((address + offset) % 0x8000)), offset);
  }
  return output;
}

function sectorLog(message) {
  $('sectorResult').textContent += `${message}\n`;
  log(message);
}

async function sectorTest() {
  busy = true;
  refresh();
  $('sectorResult').textContent = '';
  let armed = false;
  try {
    const address = sectorInput();
    const pattern = testPattern(address);
    useProgress('sectorProgress', 'sectorProgressText');
    await enterProgramming();
    showProgress(0, 'Checking the sector is erased');
    const before = await readSector(address);
    if (!before.every(byte => byte === 0xff)) throw new Error(`Sector 0x${address.toString(16)} is not erased; the S2 test only uses an erased sector.`);
    sectorLog(`Sector 0x${address.toString(16)} is erased (CRC 0x${crc16(before).toString(16)}).`);

    await sendWrite(controlFrame(OP.program, address));
    armed = true;
    await expectWrite(address, 'Begin programming', state => state.programming && state.pointer === address);
    for (let offset = 0; offset < SECTOR; offset += BLOCK) {
      await sendWrite(dataFrame(pattern.subarray(offset, offset + BLOCK)));
      await expectWrite(address, `Program 0x${(address + offset).toString(16)}`, state => state.pointer === address + offset + BLOCK && state.programming === (offset + BLOCK < SECTOR));
      showProgress((offset + BLOCK) / SECTOR, `Programmed ${offset + BLOCK} of ${SECTOR} bytes`);
    }
    armed = false;
    showProgress(1, 'Reading the pattern back');
    const written = await readSector(address);
    const mismatches = written.reduce((count, byte, index) => count + (byte !== pattern[index]), 0);
    if (mismatches) throw new Error(`Read-back differs from the pattern in ${mismatches} bytes.`);
    sectorLog(`Programmed and read back 4096 bytes (CRC 0x${crc16(pattern).toString(16)}).`);

    showProgress(1, 'Erasing and checking the sector is blank again');
    await sendWrite(controlFrame(OP.erase, address, crc16(pattern)));
    await expectWrite(address, 'Erase', state => state.crc === 0xe03e);
    const after = await readSector(address);
    if (!after.every(byte => byte === 0xff)) throw new Error('Sector is not erased after the erase command.');
    sectorLog('Erased the sector and confirmed every byte is 0xFF again. S2 test passed.');
    showProgress(1, 'S2 test passed.');
    status(`S2 sector test passed at 0x${address.toString(16)}.`);
  } catch (error) {
    if (armed) { try { await sendWrite(controlFrame(OP.disarm)); } catch { /* reported below */ } }
    sectorLog(`Stopped: ${error.message}`);
    status(`S2 sector test stopped: ${error.message}`, true);
  } finally {
    await leaveProgramming();
    busy = false;
    refresh();
  }
}

// Recovery: erase one writable sector after confirming its current CRC.
async function eraseSector() {
  busy = true;
  refresh();
  try {
    const address = sectorInput();
    await enterProgramming();
    const current = await readSector(address);
    const crc = crc16(current);
    await sendWrite(controlFrame(OP.erase, address, crc));
    await expectWrite(address, 'Erase', state => state.crc === 0xe03e);
    const after = await readSector(address);
    if (!after.every(byte => byte === 0xff)) throw new Error('Sector is not erased after the erase command.');
    $('sectorResult').textContent = `Erased 0x${address.toString(16)} (previous CRC 0x${crc.toString(16)}).`;
    status(`Erased sector 0x${address.toString(16)}.`);
  } catch (error) {
    status(`Erase stopped: ${error.message}`, true);
  } finally {
    $('eraseConfirm').checked = false;
    await leaveProgramming();
    busy = false;
    refresh();
  }
}

function drawLogo(canvasId, bytes) {
  const context = $(canvasId).getContext('2d');
  context.putImageData(new ImageData(logoToRgba(bytes), LOGO_WIDTH, LOGO_HEIGHT), 0, 0);
}

async function readLogo() {
  busy = true;
  stopRequested = false;
  totalPasses = 1;
  refresh();
  useProgress('logoProgress', 'logoProgressText');
  try {
    status('Reading the boot logo (40 KiB at 0xA00000).');
    const result = await readAnyPass({ label: 'logo', start: LOGO_ADDRESS, length: LOGO_BYTES }, 1);
    showProgress(1, `Read the current logo (40 KiB) in ${Math.round(result.seconds)} s.`);
    currentLogo = result.bytes;
    drawLogo('currentLogo', currentLogo);
    const digest = await sha256(currentLogo);
    $('currentLogoInfo').textContent = `SHA-256 ${digest.slice(0, 16)}…${digest === STOCK_LOGO_SHA256 ? ' (stock TIDRADIO logo)' : ''}`;
    status(`Read the current logo in ${Math.round(result.seconds)} s.`);
  } catch (error) {
    currentLogo = null;
    showProgress(null, `Read stopped: ${error.message}`);
    status(`Logo read failed: ${error.message}`, true);
  } finally {
    busy = false;
    refresh();
  }
}

// Fit an image onto a black 160 x 128 canvas, then convert it to the radio's format.
async function chooseLogo() {
  const file = $('logoFile').files[0];
  if (!file) return;
  try {
    if (/\.bin$/i.test(file.name)) {
      nextLogo = logoFromBin(new Uint8Array(await file.arrayBuffer()));
    } else {
      const image = await createImageBitmap(file);
      const canvas = new OffscreenCanvas(LOGO_WIDTH, LOGO_HEIGHT);
      const context = canvas.getContext('2d');
      context.fillStyle = '#000';
      context.fillRect(0, 0, LOGO_WIDTH, LOGO_HEIGHT);
      const fit = $('logoFit').value;
      let width = LOGO_WIDTH, height = LOGO_HEIGHT;
      if (fit !== 'stretch') {
        const scale = (fit === 'cover' ? Math.max : Math.min)(LOGO_WIDTH / image.width, LOGO_HEIGHT / image.height);
        width = image.width * scale;
        height = image.height * scale;
      }
      context.imageSmoothingQuality = 'high';
      context.drawImage(image, (LOGO_WIDTH - width) / 2, (LOGO_HEIGHT - height) / 2, width, height);
      nextLogo = rgbaToLogo(context.getImageData(0, 0, LOGO_WIDTH, LOGO_HEIGHT).data);
    }
    drawLogo('nextLogo', nextLogo);
    const digest = await sha256(nextLogo);
    const plan = currentLogo ? logoSectorPlan(currentLogo, nextLogo) : null;
    $('nextLogoInfo').textContent = `SHA-256 ${digest.slice(0, 16)}…${digest === STOCK_LOGO_SHA256 ? ' (stock TIDRADIO logo)' : ''}${plan ? `, ${plan.length} of 10 sectors differ` : ''}`;
    log(`Prepared logo from ${file.name} (${$('logoFit').value}); SHA-256 ${digest}.`, 'info');
  } catch (error) {
    nextLogo = null;
    status(`Could not use ${file.name}: ${error.message}`, true);
  } finally {
    refresh();
  }
}

// Rewrite only the logo sectors that differ. Each erase proves the current
// contents with their CRC; each 32-byte block is verified by the radio; the
// whole logo is read back at the end.
async function writeLogo() {
  busy = true;
  stopRequested = false;
  refresh();
  $('logoResult').textContent = '';
  const report = message => { $('logoResult').textContent += `${message}\n`; log(message, 'info'); };
  let armed = false;
  try {
    const plan = logoSectorPlan(currentLogo, nextLogo);
    if (!plan.length) {
      report('The radio already holds this logo; nothing to write.');
      return;
    }
    report(`Writing ${plan.length} of 10 logo sectors.`);
    // One bar for the whole job, in work units weighted by measured time
    // (2026-11-08: about 1 s per erase, 70 ms per verified 32-byte block,
    // about 9 s for the 40 KiB read-back).
    const ERASE_UNITS = 14, VERIFY_UNITS = 128;
    const writeUnits = plan.reduce((sum, entry) => sum + ERASE_UNITS + (entry.erasedTarget ? 0 : SECTOR / BLOCK), 0);
    const totalUnits = writeUnits + VERIFY_UNITS;
    const began = performance.now();
    let doneUnits = 0;
    const advance = (units, text) => {
      doneUnits += units;
      const elapsed = (performance.now() - began) / 1000;
      const left = doneUnits >= 8 ? elapsed * (totalUnits - doneUnits) / doneUnits : NaN;
      showProgress(doneUnits / totalUnits, `${text} · ${Math.floor(doneUnits / totalUnits * 100)}% · about ${formatDuration(left)} left`);
    };
    useProgress('logoProgress', 'logoProgressText');
    await enterProgramming();
    for (const [index, entry] of plan.entries()) {
      if (stopRequested) throw new Error('Stopped by user between sectors.');
      const label = `0x${entry.address.toString(16)}`;
      const where = `Sector ${index + 1} of ${plan.length} (${label})`;
      advance(0, `${where}: erasing`);
      await sendWrite(controlFrame(OP.erase, entry.address, entry.oldCrc));
      await expectWrite(entry.address, `Erase ${label}`, state => state.crc === 0xe03e);
      advance(ERASE_UNITS, `${where}: erased`);
      const data = nextLogo.subarray(entry.offset, entry.offset + SECTOR);
      currentLogo.fill(0xff, entry.offset, entry.offset + SECTOR);
      if (!entry.erasedTarget) {
        await sendWrite(controlFrame(OP.program, entry.address));
        armed = true;
        await expectWrite(entry.address, `Begin ${label}`, state => state.programming && state.pointer === entry.address);
        // A status check after every block, as in the sector test proven on hardware.
        for (let offset = 0; offset < SECTOR; offset += BLOCK) {
          await sendWrite(dataFrame(data.subarray(offset, offset + BLOCK)));
          await expectWrite(entry.address, `Program ${label}+0x${offset.toString(16)}`,
            state => state.pointer === entry.address + offset + BLOCK && state.programming === (offset + BLOCK < SECTOR));
          advance(1, `${where}: written ${offset + BLOCK} of ${SECTOR} bytes`);
        }
        armed = false;
        currentLogo.set(data, entry.offset);
      }
      report(`Sector ${label} written (${index + 1}/${plan.length}).`);
    }
    await leaveProgramming();
    report('Reading the logo back to verify.');
    totalPasses = 1;
    progressShare = { start: writeUnits / totalUnits, span: VERIFY_UNITS / totalUnits, prefix: 'Verifying the whole logo: ' };
    const check = await readAnyPass({ label: 'logo', start: LOGO_ADDRESS, length: LOGO_BYTES }, 1);
    progressShare = { start: 0, span: 1, prefix: '' };
    currentLogo = check.bytes;
    drawLogo('currentLogo', currentLogo);
    const differing = check.bytes.reduce((count, byte, index) => count + (byte !== nextLogo[index]), 0);
    if (differing) throw new Error(`Read-back differs from the new logo in ${differing} bytes.`);
    showProgress(1, `Done: ${plan.length} sectors written and the whole logo verified in ${formatDuration((performance.now() - began) / 1000)}.`);
    report('Logo written and verified. Power-cycle the radio to see it.');
    status('Boot logo written and verified.');
  } catch (error) {
    if (armed) { try { await sendWrite(controlFrame(OP.disarm)); } catch { /* reported below */ } }
    currentLogo = null;
    progressShare = { start: 0, span: 1, prefix: '' };
    showProgress(null, `Stopped: ${error.message}`);
    report(`Stopped: ${error.message}. Read the current logo again before retrying; restore from your backup .bin if needed.`);
    status(`Logo write stopped: ${error.message}`, true);
    try { await leaveProgramming(); } catch { /* already out */ }
  } finally {
    busy = false;
    $('logoConfirm').checked = false;
    refresh();
  }
}

function countErased(bytes) {
  let erased = 0;
  for (let offset = 0; offset < bytes.length; offset += BLOCK) {
    if (bytes.subarray(offset, offset + BLOCK).every(byte => byte === 0xff)) erased++;
  }
  return erased;
}

function save(blob, name) {
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = name;
  link.click();
  setTimeout(() => URL.revokeObjectURL(link.href), 1000);
}

function fileStem() {
  const { region, start } = lastDump.manifest;
  return `td-h9-resource-${region}-${start}-${new Date().toISOString().slice(0, 10)}`;
}

$('connect').addEventListener('click', connect);
$('disconnect').addEventListener('click', () => device?.gatt?.disconnect());
$('check').addEventListener('click', checkService);
$('dump').addEventListener('click', dump);
$('stop').addEventListener('click', () => { stopRequested = true; });
$('downloadBin').addEventListener('click', () => save(new Blob([lastDump.bytes]), `${fileStem()}.bin`));
$('downloadManifest').addEventListener('click', () => save(new Blob([JSON.stringify(lastDump.manifest, null, 2) + '\n'], { type: 'application/json' }), `${fileStem()}.json`));
$('sectorTest').addEventListener('click', sectorTest);
$('readLogo').addEventListener('click', readLogo);
$('downloadLogo').addEventListener('click', () => save(new Blob([currentLogo]), `td-h9-logo-${new Date().toISOString().slice(0, 10)}.bin`));
$('logoFile').addEventListener('change', chooseLogo);
$('logoFit').addEventListener('change', chooseLogo);
$('logoConfirm').addEventListener('change', refresh);
$('writeLogo').addEventListener('click', writeLogo);
$('eraseSector').addEventListener('click', eraseSector);
$('eraseConfirm').addEventListener('change', refresh);
$('region').addEventListener('change', () => { $('customRange').hidden = $('region').value !== 'custom'; });
for (const [index, part] of PARTITIONS.entries()) {
  const option = document.createElement('option');
  option.value = index;
  option.textContent = `${part.name} (0x${part.origin.toString(16).padStart(6, '0')}, ${part.length / 0x100000} MiB)`;
  $('region').insertBefore(option, $('region').querySelector('[value=chip]'));
}
$('region').value = '5';
refresh();
