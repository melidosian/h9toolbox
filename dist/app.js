import { CHANNEL_COUNT, CTCSS_TONES, DCS_CODES, readChannel, writeChannel, clearChannel } from './channel.mjs';
import { SETTINGS, SETTING_GROUPS, readSetting, writeSetting, readFixedPosition } from './settings.mjs?v=20261126dualwatch';
import { SETTING_HELP } from './settings-help.mjs?v=20261126dualwatch';
import { exportPortable, importPortable } from './portable.mjs?v=20261126dualwatch';
import { parseMHz, standardOffset, planTransmit } from './channel-plan.mjs';
import { exportChannels, inspectChannelImport, importChannels } from './channels-file.mjs';
import { serializeChannelsCsv, parseChannelsCsv } from './channels-csv.mjs';
import { checkLongName, longNameChanges, readCloneCapabilities, readLongName, writeLongName } from './channel-labels.mjs';
import { readBlockReply } from './protocol.mjs';
import { ResourceRadio, connectUsb } from './boot-logo/radio.mjs?v=programmer-usb';
import { parseStatus, statusAddress } from './boot-logo/resource.mjs?v=20261108a';
import { mountLogoTool } from './boot-logo/logo-tool.mjs?v=20261109b';

const $ = id => document.getElementById(id);
const guide = $('programmerGuide');
const slides = [...guide.querySelectorAll('.guide-slide')];
const stepIndicators = [...guide.querySelectorAll('.guide-steps li')];
const nextLabels = ['Read & back up →', 'Edit channels →', 'Radio settings →', 'Review changes →'];
const MEM_SIZE = 0x3140;
const BLOCK = 0x20;
const IMAGE_SIZE = MEM_SIZE + 8;
const MAGIC = Uint8Array.of(0x50, 0x56, 0x4f, 0x4a, 0x48, 0x5c, 0x14);
const PROFILES = [
  { name: 'TD-H9 / FF00', service: '0000ff00-0000-1000-8000-00805f9b34fb', write: '0000ff02-0000-1000-8000-00805f9b34fb', notify: '0000ff01-0000-1000-8000-00805f9b34fb' },
  { name: 'Nordic UART', service: '6e400001-b5a3-f393-e0a9-e50e24dcca9e', write: '6e400002-b5a3-f393-e0a9-e50e24dcca9e', notify: '6e400003-b5a3-f393-e0a9-e50e24dcca9e' },
  { name: 'HM-10 / FFE0', service: '0000ffe0-0000-1000-8000-00805f9b34fb', write: '0000ffe1-0000-1000-8000-00805f9b34fb', notify: '0000ffe1-0000-1000-8000-00805f9b34fb' },
  { name: 'Feasycom / FFF0', service: '0000fff0-0000-1000-8000-00805f9b34fb', write: '0000fff2-0000-1000-8000-00805f9b34fb', notify: '0000fff1-0000-1000-8000-00805f9b34fb' },
];
const modeNames = { 0x4e: 'Normal', 0x48: 'HAM', 0x47: 'GMRS' };

let device = null;
let writeChar = null;
let notifyChar = null;
let usbSession = null;
let sparseSerialWrites = false;
let radioBackup = null;
let baseline = null;
let edited = null;
let originalBackup = null;
let baselineLongNames = null;
let editedLongNames = new Map();
let backupSaved = false;
let selectedChannel = null;
let txManuallyEdited = false;
let directionAuto = false;
let customShift = null;
let busy = false;
let incoming = [];
let incomingWake = null;
let readReplyMode = 'auto';
// H9 Toolbox Firmware: `resourceRadio` shares this connection (it gets every
// notification and clears its own queue per session), `toolbox` holds the
// service status once a read has detected it. The clone image is the start of
// the rw_freq partition at chip 0x900000, so it can be bulk-read.
const RW_FREQ_ORIGIN = 0x900000;
let resourceRadio = null;
let toolbox = null;
let logoTool = null;
let logoBusy = false;
let currentStep = 0;

function refreshNavigation(connected) {
  const onLastStep = currentStep === slides.length - 1;
  $('previousStep').disabled = busy || currentStep === 0;
  $('nextStep').hidden = onLastStep;
  $('nextStep').disabled = busy || onLastStep || (currentStep === 1 && !edited);
  $('nextStep').textContent = currentStep === 0 && !connected ? 'Open a backup →' : nextLabels[currentStep] ?? '';
  $('reconnectStep').hidden = !onLastStep || (connected && (!usbSession || sparseSerialWrites));
  $('reconnectStep').disabled = busy;
  $('reconnectStep').textContent = connected ? 'Change connection →' : 'Connect radio →';
  $('writeDisconnected').hidden = connected;
  $('usbWriteUnavailable').hidden = !(usbSession && !sparseSerialWrites);
}

function showStep(index) {
  if (index < 0 || index >= slides.length) return;
  slides[currentStep].hidden = true;
  currentStep = index;
  slides[currentStep].hidden = false;
  stepIndicators.forEach((step, i) => {
    step.classList.toggle('past', i < currentStep);
    if (i === currentStep) step.setAttribute('aria-current', 'step');
    else step.removeAttribute('aria-current');
  });
  refresh();
  $(slides[currentStep].getAttribute('aria-labelledby')).focus();
}

function log(message) {
  const item = document.createElement('li');
  item.textContent = `${new Date().toLocaleTimeString()} · ${message}`;
  $('log').prepend(item);
  while ($('log').children.length > 45) $('log').lastChild.remove();
}
function radioLog(message, level) {
  if (level !== 'debug') log(message);
}
function status(message, error = false) {
  $('status').hidden = false;
  $('status').textContent = message;
  $('status').classList.toggle('error-status', error);
  log(message);
}
function hex(bytes) { return [...bytes].map(b => b.toString(16).padStart(2, '0')).join(' '); }
function same(a, b) { return a.length === b.length && a.every((n, i) => n === b[i]); }
function serviceUuid(value) {
  const raw = value.trim().toLowerCase();
  if (/^[0-9a-f]{4}$/.test(raw)) return `0000${raw}-0000-1000-8000-00805f9b34fb`;
  if (/^[0-9a-f]{8}$/.test(raw)) return `${raw}-0000-1000-8000-00805f9b34fb`;
  if (/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(raw)) return raw;
  throw new Error('Enter a 16-bit, 32-bit, or full 128-bit service UUID.');
}
function selectedProfiles() {
  const choice = $('profile').value;
  return choice === 'auto' || choice === 'custom' ? PROFILES : [PROFILES[{ h9: 0, nordic: 1, hm10: 2, feasycom: 3 }[choice]]];
}
function connectedName() {
  return usbSession?.name ?? (device?.gatt?.connected && writeChar && notifyChar ? device.name || 'TD-H9' : '');
}
function refresh() {
  const name = connectedName();
  const connected = !!name;
  refreshNavigation(connected);
  $('connection').classList.toggle('connected', connected);
  $('connection').innerHTML = `<i></i> ${connected ? 'Connected to ' + escapeHtml(name) : 'Disconnected'}`;
  $('connect').disabled = busy || connected || !navigator.bluetooth;
  $('connectUsb').disabled = busy || connected || !navigator.serial;
  $('bluetoothUnavailable').hidden = !!navigator.bluetooth;
  $('usbUnavailable').hidden = !!navigator.serial;
  $('disconnect').disabled = busy || !connected;
  $('read').disabled = busy || !connected;
  $('read').hidden = !connected || busy;
  $('save').disabled = busy || !baseline;
  $('connectedBackup').hidden = !radioBackup;
  $('useConnectedImage').disabled = busy;
  $('saveConnectedBackup').disabled = busy;
  $('openFile').disabled = busy;
  $('exportJson').disabled = busy || !edited;
  $('exportArchive').disabled = busy || !edited;
  $('exportImg').disabled = busy || !edited;
  $('search').disabled = !edited;
  $('showAll').disabled = !edited;
  $('createChannel').disabled = busy || !edited;
  $('newChannelNumber').disabled = busy || !edited;
  $('exportChannels').disabled = busy || !edited;
  $('openChannels').disabled = busy || !edited;
  $('applySettings').disabled = busy || !edited;
  $('riskAcknowledged').disabled = busy || !edited;
  const count = edited && baseline ? changedBlocks().length : 0;
  const longNames = pendingLongNames().filter(change => !change.implicit).length;
  const summaries = [count ? `${count} changed block${count === 1 ? '' : 's'}` : '',
    longNames ? `${longNames} long name${longNames === 1 ? '' : 's'}` : ''].filter(Boolean);
  $('changeCount').textContent = summaries.join(' · ') || 'No changes';
  $('write').disabled = busy || !connected || (usbSession && !sparseSerialWrites) ||
    !$('riskAcknowledged').checked || (!count && !longNames);
  $('radioMode').textContent = edited ? `${modeName(edited)} image` : 'No image';
  $('logoCard').hidden = !(toolbox && connected);
  $('toolboxNote').hidden = !toolbox;
  if (toolbox) $('toolboxNote').innerHTML = '<strong>H9 Toolbox Firmware detected.</strong> The final step includes the boot logo tool.';
  logoTool?.refresh();
  $('imageInfo').textContent = edited ? `${IMAGE_SIZE.toLocaleString()} bytes · ${modeName(edited)} · ${backupSaved ? 'Original backup saved' : 'Original backup available to save'}` : 'No radio image loaded.';
}
function escapeHtml(str) { return str.replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]); }
function modeName(image) { return modeNames[image[7]] || 'Unknown'; }
function validateImage(bytes) {
  if (bytes.length !== IMAGE_SIZE) throw new Error(`Expected ${IMAGE_SIZE} bytes, got ${bytes.length}.`);
  if (!same(bytes.slice(0, 7), Uint8Array.of(0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff)) || !modeNames[bytes[7]]) throw new Error('This is not a TD-H9 Normal, HAM, or GMRS image.');
}
function onNotification(event) {
  if (event.target !== notifyChar) return;
  const bytes = new Uint8Array(event.target.value.buffer, event.target.value.byteOffset, event.target.value.byteLength);
  resourceRadio?.receive(bytes.slice());
  receiveCloneBytes(bytes);
}
function receiveCloneBytes(bytes) {
  incoming.push(...bytes);
  if (incomingWake) { incomingWake(); incomingWake = null; }
}
async function readExact(length, timeoutMs = 2500) {
  const out = new Uint8Array(length);
  let at = 0;
  const deadline = Date.now() + timeoutMs;
  while (at < length) {
    if (!connectedName()) throw new Error('Radio disconnected.');
    while (incoming.length && at < length) out[at++] = incoming.shift();
    if (at === length) break;
    const remaining = deadline - Date.now();
    if (remaining <= 0) throw new Error(`Radio timed out (${at}/${length} bytes). Check the read-reply option if it stops at 36/37 bytes.`);
    await new Promise((resolve, reject) => {
      const timer = setTimeout(() => { incomingWake = null; reject(new Error(`Radio timed out (${at}/${length} bytes). Check the read-reply option if it stops at 36/37 bytes.`)); }, remaining);
      incomingWake = () => { clearTimeout(timer); resolve(); };
    });
  }
  return out;
}
async function send(bytes) {
  if (usbSession) return usbSession.radio.write(bytes);
  if (!writeChar) throw new Error('Radio disconnected.');
  if (writeChar.properties.write) await writeChar.writeValueWithResponse(bytes);
  else await writeChar.writeValueWithoutResponse(bytes);
}
async function expectAck(what) {
  const ack = await readExact(1);
  if (ack[0] !== 0x06) throw new Error(`${what}: expected ACK (06), received ${hex(ack)}.`);
}
async function enterProgramming() {
  incoming = [];
  readReplyMode = $('checksum').value;
  await send(MAGIC);
  await expectAck('Handshake');
  try {
    await send(Uint8Array.of(0x02));
    // Skip a late acknowledgement of a previous exit (0x45): it can arrive
    // ahead of the identity, which always starts with 'T' (0x54).
    let first = (await readExact(1))[0];
    for (let skipped = 0; first === 0x06 && skipped < 4; skipped++) first = (await readExact(1))[0];
    const ident = Uint8Array.of(first, ...(await readExact(7)));
    if (!same(ident.slice(0, 7), Uint8Array.of(0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff)) || !modeNames[ident[7]]) throw new Error(`Unexpected radio identity: ${hex(ident)}.`);
    await send(Uint8Array.of(0x06));
    await expectAck('Programming mode');
    log(`Radio identified: TD-H9 ${modeNames[ident[7]]}.`);
    return ident;
  } catch (error) {
    await exitProgramming();
    throw error;
  }
}
async function exitProgramming() {
  try { if (connectedName()) await send(Uint8Array.of(0x45)); }
  catch (error) { log(`Could not send exit command: ${error.message}`); }
}
async function readBlock(address) {
  const cmd = Uint8Array.of(0x52, (address >> 8) & 255, address & 255, BLOCK);
  await send(cmd);
  const previousMode = readReplyMode;
  const reply = await readBlockReply(address, BLOCK, readReplyMode, readExact, async () => {
    try { return (await readExact(1, 150))[0]; }
    catch (error) { if (error.message.startsWith('Radio timed out')) return null; throw error; }
  });
  readReplyMode = reply.mode;
  if (previousMode !== readReplyMode) log(`Read reply: ${readReplyMode === 'yes' ? '37 bytes (checksum)' : '36 bytes (no checksum)'}.`);
  return reply.data;
}
async function writeBlock(address, bytes) {
  const checksum = bytes.reduce((sum, byte) => (sum + byte) & 255, 0);
  await send(Uint8Array.of(0x57, (address >> 8) & 255, address & 255, BLOCK, ...bytes, checksum));
  await expectAck(`Write at 0x${address.toString(16)}`);
}
const labelRadio = {readBlock, writeBlock};

async function probeCloneCapabilities() {
  try {
    return {...await readCloneCapabilities(labelRadio), resync: false};
  } catch (error) {
    log(`Channel long-name probe did not respond (${error.message}).`);
    return {longNames: false, sparseSerialWrites: false, resync: true};
  }
}

async function readChannelLongNames(image) {
  const names = new Map();
  const channels = [];
  for (let n = 1; n <= CHANNEL_COUNT; n++) if (readChannel(image, n).valid) channels.push(n);
  if (channels.length) startProgress('Reading channel long names');
  for (const [i, n] of channels.entries()) {
    names.set(n, await readLongName(labelRadio, n));
    showProgress(i + 1, channels.length);
  }
  return names;
}
// One status read at the H9 Toolbox service address, as the first read of a
// session. Stock firmware returns ordinary clone bytes there (or nothing).
async function probeToolbox() {
  try {
    const state = parseStatus(await readBlock(statusAddress(0)), 0);
    if (state.version !== 2 || !state.ready) return { status: null, resync: false };
    log('H9 Toolbox Firmware detected: boot-logo service ready.');
    return { status: state, resync: false };
  } catch (error) {
    return { status: null, resync: error.message.startsWith('Radio timed out') };
  }
}
// Bulk-read the clone image from rw_freq, then compare four blocks with the
// normal clone read. Returns null (after logging why) to fall back.
async function fastImage(ident) {
  try {
    startProgress('Reading the radio (fast)');
    const result = await resourceRadio.readRange(RW_FREQ_ORIGIN, MEM_SIZE, fraction => showProgress(Math.round(fraction * MEM_SIZE), MEM_SIZE));
    const samples = [0, MEM_SIZE - BLOCK, ...[0, 1].map(() => Math.floor(Math.random() * (MEM_SIZE / BLOCK)) * BLOCK)];
    await new Promise(resolve => setTimeout(resolve, 300));
    await enterProgramming();
    try {
      for (const address of samples) {
        const normal = await readBlock(address);
        if (!same(normal, result.bytes.subarray(address, address + BLOCK))) throw new Error(`fast and normal reads differ at 0x${address.toString(16)}`);
      }
    } finally { await exitProgramming(); }
    const image = new Uint8Array(IMAGE_SIZE);
    image.set(ident);
    image.set(result.bytes, 8);
    log(`Fast read: ${MEM_SIZE.toLocaleString()} bytes in ${result.seconds.toFixed(1)} s; ${samples.length} blocks match the normal read.`);
    return image;
  } catch (error) {
    log(`Fast read not used (${error.message}); using the standard read.`);
    await exitProgramming();
    return null;
  }
}
// startProgress(label) begins a timed step; showProgress adds the
// percentage and, after a couple of seconds, the time left at the measured
// rate (as on the firmware update page).
let progressStep = { label: '', began: 0 };
function startProgress(label) {
  progressStep = { label, began: performance.now() };
  showProgress(0, 1);
}
function formatDuration(seconds) {
  const whole = Math.max(1, Math.ceil(seconds));
  return whole < 90 ? `${whole} s` : `${Math.floor(whole / 60)} min ${String(whole % 60).padStart(2, '0')} s`;
}
function showProgress(done, total) {
  $('progressWrap').hidden = false;
  const percent = Math.round(done * 100 / total);
  const elapsed = (performance.now() - progressStep.began) / 1000;
  const parts = [`${percent}%`];
  if (progressStep.label) parts.push(progressStep.label);
  if (done >= total && done > 0) parts.push(`done in ${formatDuration(elapsed)}`);
  else if (done > 0 && elapsed >= 2) parts.push(`about ${formatDuration(elapsed * (total - done) / done)} left`);
  $('progressBar').style.width = `${percent}%`;
  $('progressLabel').textContent = parts.join(' · ');
}
async function work(fn) {
  if (busy) return;
  busy = true; refresh();
  try { await fn(); }
  catch (error) { status(error.message || String(error), true); }
  finally { busy = false; refresh(); }
}
async function connectBluetooth() {
  if (!navigator.bluetooth) throw new Error('Web Bluetooth is unavailable here. Open this page in Chrome or Edge over HTTPS.');
  let profiles = selectedProfiles();
  const extra = $('customUuid').value.trim();
  const extraUuid = extra ? serviceUuid(extra) : null;
  if ($('profile').value === 'custom' && !extraUuid) throw new Error('Enter the custom service UUID first.');
  const optionalServices = [...new Set([...profiles.map(p => p.service), ...(extraUuid ? [extraUuid] : [])])];
  status('Choose your TD-H9 in the Bluetooth device picker.');
  device = await navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'TD-H9' }], optionalServices });
  device.addEventListener('gattserverdisconnected', bluetoothDisconnected);
  const server = await device.gatt.connect();
  let services;
  try { services = await server.getPrimaryServices(); }
  catch (error) { server.disconnect(); throw new Error(`Could not discover approved BLE services: ${error.message}`); }
  log(`Visible services: ${services.map(s => s.uuid).join(', ') || 'none'}.`);
  let pair = null;
  for (const profile of profiles) {
    const service = services.find(s => s.uuid.toLowerCase() === profile.service);
    if (!service) continue;
    const chars = await service.getCharacteristics();
    const tx = chars.find(c => c.uuid.toLowerCase() === profile.write && (c.properties.write || c.properties.writeWithoutResponse));
    const rx = chars.find(c => c.uuid.toLowerCase() === profile.notify && (c.properties.notify || c.properties.indicate));
    if (tx && rx) { pair = { tx, rx, name: profile.name }; break; }
  }
  if (!pair && extraUuid) {
    const service = services.find(s => s.uuid.toLowerCase() === extraUuid);
    if (service) {
      const chars = await service.getCharacteristics();
      const writers = chars.filter(c => c.properties.write || c.properties.writeWithoutResponse);
      const readers = chars.filter(c => c.properties.notify || c.properties.indicate);
      log(`Custom service characteristics: ${chars.map(c => `${c.uuid} [${c.properties.write ? 'W' : ''}${c.properties.writeWithoutResponse ? 'w' : ''}${c.properties.notify ? 'N' : ''}${c.properties.indicate ? 'I' : ''}]`).join(', ')}.`);
      if (writers.length === 1 && readers.length === 1) pair = { tx: writers[0], rx: readers[0], name: 'Custom UART' };
    }
  }
  if (!pair) {
    server.disconnect();
    throw new Error('No known UART write/notify pair found. Try another service UUID in Bluetooth options.');
  }
  writeChar = pair.tx; notifyChar = pair.rx;
  incoming = [];
  resourceRadio = new ResourceRadio({ write: send, log: radioLog });
  notifyChar.addEventListener('characteristicvaluechanged', onNotification);
  await notifyChar.startNotifications();
  status(`Connected to ${device.name || 'TD-H9'} via ${pair.name}.`);
}
async function connectSerial() {
  status('Choose the TD-H9 serial port.');
  let session;
  session = await connectUsb({
    log: radioLog,
    onReceive: receiveCloneBytes,
    onDisconnect: () => { if (session && usbSession === session) finishDisconnect(); },
  });
  usbSession = session;
  resourceRadio = session.radio;
}
function clearConnection() {
  device?.removeEventListener('gattserverdisconnected', bluetoothDisconnected);
  notifyChar?.removeEventListener('characteristicvaluechanged', onNotification);
  device = null;
  writeChar = null;
  notifyChar = null;
  usbSession = null;
  sparseSerialWrites = false;
  resourceRadio = null;
  toolbox = null;
  incoming = [];
  if (incomingWake) { incomingWake(); incomingWake = null; }
  logoTool?.reset();
}
function finishDisconnect() {
  clearConnection();
  status('Radio disconnected.');
  refresh();
}
function bluetoothDisconnected(event) {
  if (event.target === device) finishDisconnect();
}
async function closeConnection() {
  const port = usbSession;
  const gatt = device?.gatt;
  clearConnection();
  if (port) await port.disconnect();
  else if (gatt?.connected) gatt.disconnect();
}
function startConnection(open) {
  return work(async () => {
    try { await open(); }
    catch (error) { await closeConnection(); throw error; }
    radioBackup = null;
    showStep(1);
    await readRadio();
  });
}
async function readRadio() {
  let entered = false;
  try {
    status(`Reading backup from ${connectedName()}…`);
    startProgress('Reading the radio');
    let ident = await enterProgramming(); entered = true;
    const began = performance.now();
    let image = null;
    const probe = await probeToolbox();
    if (probe.resync) {
      // A stock radio may ignore the probe; start a clean session.
      await exitProgramming(); entered = false;
      await new Promise(resolve => setTimeout(resolve, 300));
      ident = await enterProgramming(); entered = true;
    }
    toolbox = probe.status;
    refresh();
    if (toolbox && !usbSession) {
      await exitProgramming(); entered = false;
      // Let the radio's late acknowledgement of the exit arrive before the
      // fast session starts (it otherwise precedes the next identity).
      await new Promise(resolve => setTimeout(resolve, 300));
      image = await fastImage(ident);
      if (!image) { await new Promise(resolve => setTimeout(resolve, 300)); ident = await enterProgramming(); entered = true; }
    }
    if (!image) {
      startProgress('Reading the radio');
      image = new Uint8Array(IMAGE_SIZE);
      image.set(ident);
      for (let address = 0; address < MEM_SIZE; address += BLOCK) {
        image.set(await readBlock(address), address + 8);
        if (address % (BLOCK * 8) === 0 || address + BLOCK === MEM_SIZE) {
          showProgress(address + BLOCK, MEM_SIZE);
          await new Promise(resolve => setTimeout(resolve, 0));
        }
      }
    }
    let names = null;
    if (toolbox) {
      try {
        if (!entered) {
          await new Promise(resolve => setTimeout(resolve, 300));
          const nameIdent = await enterProgramming();
          entered = true;
          if (!same(nameIdent, ident)) throw new Error('Radio changed before reading channel long names.');
        }
        const capabilities = await probeCloneCapabilities();
        sparseSerialWrites = capabilities.sparseSerialWrites;
        if (capabilities.longNames) names = await readChannelLongNames(image);
      } catch (error) {
        log(`Channel long names could not be read (${error.message}). The radio image is available.`);
      }
    }
    log(`Read ${IMAGE_SIZE.toLocaleString()} bytes in ${((performance.now() - began) / 1000).toFixed(1)} s.`);
    $('progressBar').style.width = '100%';
    $('progressLabel').textContent = `100% · Read complete in ${formatDuration((performance.now() - began) / 1000)}`;
    validateImage(image);
    if (edited) {
      radioBackup = {image, names};
      status('Read complete. Your working image is still open. The connected radio backup is ready to save or use.');
    } else {
      useRadioImage({image, names});
      status(`Read complete: ${modeName(image)} image. ${names ? 'Channel long names loaded. ' : ''}Save the original backup before editing.`);
    }
  } finally { if (entered) await exitProgramming(); }
}
function useRadioImage({image, names}) {
  baseline = image;
  edited = image.slice();
  originalBackup = image.slice();
  baselineLongNames = names;
  editedLongNames = new Map([...(names ?? [])].filter(([, name]) => name !== ''));
  backupSaved = false;
  radioBackup = null;
  selectedChannel = null;
  $('editPanel').hidden = true;
  $('riskAcknowledged').checked = false;
  renderChannels();
  renderSettings();
}
function useConnectedImage() {
  if (!radioBackup) return;
  if ((changedBlocks().length || pendingLongNames().length) &&
      !window.confirm('Replace your working image and its staged edits with the backup just read from the connected radio?')) return;
  useRadioImage(radioBackup);
  status('The backup from the connected radio is now your working image.');
}
function saveConnectedBackup() {
  if (!radioBackup) return;
  saveFile(radioBackup.image, `TD-H9-${modeName(radioBackup.image)}-radio-backup.img`, 'application/octet-stream');
  status('Connected radio backup download started.');
}
function saveBackup() {
  if (!originalBackup) return;
  saveFile(originalBackup, `TD-H9-${modeName(originalBackup)}-original-${new Date().toISOString().slice(0, 10)}.img`, 'application/octet-stream');
  backupSaved = true;
  status('Original backup download started. Keep this file unchanged.'); refresh();
}
function saveFile(bytes, name, type) {
  const blob = new Blob([bytes], { type });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = name;
  document.body.append(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 30000);
}
function downloadJson(archive = false) {
  if (!baseline || !edited) return;
  try {
    const document = exportPortable(baseline, edited, {archive, longNames: editedLongNames, baseLongNames: baselineLongNames});
    const bytes = JSON.stringify(document, null, 2) + '\n';
    saveFile(bytes, `TD-H9-${modeName(edited)}-${archive ? 'archive' : 'config'}-${new Date().toISOString().slice(0, 10)}.json`, 'application/json');
    status(archive ? 'Lossless JSON archive downloaded with complete original image bytes.' : 'Readable configuration JSON downloaded. Load an .img backup or read the destination radio before importing it.');
  } catch (error) { status(error.message, true); }
}
function exportImg() {
  if (!edited) return;
  saveFile(edited, `TD-H9-${modeName(edited)}-working-${new Date().toISOString().slice(0, 10)}.img`, 'application/octet-stream');
  status(editedLongNames.size ? 'Working .img downloaded. Use CSV or JSON to save channel long names.' :
    'Working .img downloaded. Keep the separate original backup unchanged.');
}
function downloadChannels() {
  if (!edited) return;
  try {
    const document = exportChannels(edited, editedLongNames);
    saveFile(serializeChannelsCsv(document), `TD-H9-${modeName(edited)}-channels-${new Date().toISOString().slice(0, 10)}.csv`, 'text/csv;charset=utf-8');
    status(`${document.channels.length} channel${document.channels.length === 1 ? '' : 's'} exported as CSV. Radio settings and raw memory are not included.`);
  } catch (error) { status(error.message, true); }
}
async function openChannels(event) {
  const file = event.target.files?.[0]; if (!file) return;
  await work(async () => {
    if (!edited) throw new Error('Read the radio or open a backup before importing channels.');
    if (file.size > 2_000_000) throw new Error('Channels file is too large.');
    let document;
    if (file.name.toLowerCase().endsWith('.json')) {
      try { document = JSON.parse(await file.text()); }
      catch { throw new Error('Could not parse the older channels JSON file.'); }
    } else document = parseChannelsCsv(await file.text(), edited);
    document = {...document, replaceAll: document?.replaceAll === true || $('replaceChannels').checked};
    const plan = inspectChannelImport(edited, document, editedLongNames);
    if ((plan.conflicts.length || plan.removals.length) && !window.confirm(`Import will replace ${plan.conflicts.length} occupied channel${plan.conflicts.length === 1 ? '' : 's'} and remove ${plan.removals.length} channel${plan.removals.length === 1 ? '' : 's'}${document.replaceAll === true ? ' because Replace all channels is selected' : ''}. Continue?`)) return;
    const result = importChannels(edited, document, editedLongNames);
    edited = result.image;
    editedLongNames = result.longNames;
    $('replaceChannels').checked = false;
    selectedChannel = null;
    $('editPanel').hidden = true;
    $('riskAcknowledged').checked = false;
    $('search').value = '';
    renderChannels();
    status(`Imported ${document.channels.length} channel entr${document.channels.length === 1 ? 'y' : 'ies'}: ${result.additions.length} added, ${result.conflicts.length} replaced, ${result.removals.length} removed, ${result.unchanged.length} unchanged. Radio has not been changed.`);
  });
  event.target.value = '';
}
async function openJson(event) {
  const file = event.target.files?.[0]; if (!file) return;
  await work(async () => {
    if (file.size > 2_000_000) throw new Error('JSON file is too large for a TD-H9 image.');
    let document;
    try { document = JSON.parse(await file.text()); }
    catch { throw new Error('Could not parse the JSON file.'); }
    const configuration = document.formatVersion === 2;
    const parsed = importPortable(document, configuration ? edited : null, editedLongNames);
    if (!configuration) {
      baseline = parsed.original;
      originalBackup = parsed.original.slice();
      baselineLongNames = parsed.baselineLongNames;
      backupSaved = true;
    }
    edited = parsed.edited;
    editedLongNames = parsed.longNames;
    $('riskAcknowledged').checked = false;
    renderChannels();
    renderSettings();
    status(`Opened ${file.name}: ${changedBlocks().length} changed block${changedBlocks().length === 1 ? '' : 's'}. Connect the same radio before writing.`);
  });
  event.target.value = '';
}
async function openBackup(event) {
  const file = event.target.files?.[0]; if (!file) return;
  await work(async () => {
    const bytes = new Uint8Array(await file.arrayBuffer());
    validateImage(bytes);
    baseline = bytes;
    edited = bytes.slice();
    originalBackup = bytes.slice();
    baselineLongNames = null;
    editedLongNames = new Map();
    backupSaved = true;
    $('riskAcknowledged').checked = false;
    renderChannels();
    renderSettings();
    status(`Opened ${file.name}: TD-H9 ${modeName(bytes)}. Connect the same radio before writing.`);
  });
  event.target.value = '';
}
function openFile(event) {
  const name = event.target.files?.[0]?.name.toLowerCase();
  if (!name) return;
  if (name.endsWith('.json')) return openJson(event);
  if (name.endsWith('.img')) return openBackup(event);
  status('Choose a TD-H9 .img backup or H9 Toolbox .json file.', true);
  event.target.value = '';
}
function toneLabel(tone) {
  if (tone.type === 'off') return '—';
  if (tone.type === 'ctcss') return `${tone.value} Hz`;
  if (tone.type === 'dcs') return `D${tone.value}${tone.polarity}`;
  return `Raw ${tone.value}`;
}
function toneSummary(ch) {
  const tx = toneLabel(ch.txTone), rx = toneLabel(ch.rxTone);
  if (tx === '—' && rx === '—') return '—';
  return tx === rx ? tx : `TX ${tx} / RX ${rx}`;
}
function renderChannels() {
  const rows = $('channelRows'); rows.replaceChildren();
  if (!edited) { rows.innerHTML = '<tr><td colspan="9" class="empty">Read from the radio or open a backup to see channels.</td></tr>'; refresh(); return; }
  const search = $('search').value.trim().toLowerCase();
  const showAll = $('showAll').checked;
  let count = 0;
  const fragment = document.createDocumentFragment();
  for (let n = 1; n <= CHANNEL_COUNT; n++) {
    const ch = readChannel(edited, n);
    if (ch.valid) count++;
    if (!ch.valid && !showAll) continue;
    const longName = ch.valid ? editedLongNames.get(n) ?? '' : '';
    if (search && !`${n} ${ch.name} ${longName} ${ch.rx || ''} ${ch.tx || ''} ${ch.mode} ${toneSummary(ch)}`.toLowerCase().includes(search)) continue;
    const row = document.createElement('tr');
    row.innerHTML = `<td>${n.toString().padStart(3, '0')}</td><td>${escapeHtml(longName || ch.name || '—')}</td><td>${ch.rx || '—'}</td><td>${ch.tx || '—'}</td><td>${ch.valid ? ch.mode : '—'}</td><td>${ch.valid ? escapeHtml(toneSummary(ch)) : '—'}</td><td>${ch.valid ? ['Low', 'Mid', 'High', '?'][ch.power] : '—'}</td><td>${ch.valid ? (ch.scan ? 'Yes' : 'No') : '—'}</td><td class="channel-actions"><button class="text-button" data-channel="${n}" aria-label="${ch.valid ? 'Edit' : 'Add'} channel ${n}">${ch.valid ? 'Edit' : 'Add'}</button>${ch.valid ? `<button class="text-button danger-text" data-remove="${n}" aria-label="Remove channel ${n}">Remove</button>` : ''}</td>`;
    fragment.append(row);
  }
  rows.append(fragment);
  if (!rows.children.length) rows.innerHTML = '<tr><td colspan="9" class="empty">No channels match.</td></tr>';
  $('channelCount').textContent = `${count} populated`;
  refresh();
}
function setToneForm(side, tone) {
  const prefix = `edit${side}`;
  const raw = $(`${prefix}ToneType`).querySelector('[value="raw"]');
  raw.hidden = tone.type !== 'raw';
  raw.disabled = tone.type !== 'raw';
  raw.textContent = tone.type === 'raw' ? `Unknown (${tone.value}) · preserve` : 'Unknown · preserve';
  $(`${prefix}ToneType`).value = tone.type;
  selectPreservingUnknown($(`${prefix}Ctcss`), tone.type === 'ctcss' ? tone.value : '88.5');
  selectPreservingUnknown($(`${prefix}Dcs`), tone.type === 'dcs' ? tone.value : '023');
  $(`${prefix}Polarity`).value = tone.polarity || 'N';
  updateToneControls(side);
}
function selectPreservingUnknown(select, value) {
  select.querySelector('[data-unknown]')?.remove();
  if (![...select.options].some(option => option.value === value)) {
    const option = new Option(`${value} · current value`, value);
    option.dataset.unknown = 'true';
    select.add(option);
  }
  select.value = value;
}
function updateToneControls(side) {
  const prefix = `edit${side}`;
  const type = $(`${prefix}ToneType`).value;
  $(`${prefix}CtcssWrap`).hidden = type !== 'ctcss';
  $(`${prefix}DcsWrap`).hidden = type !== 'dcs';
  $(`${prefix}PolarityWrap`).hidden = type !== 'dcs';
}
function toneForm(side, original) {
  const prefix = `edit${side}`, type = $(`${prefix}ToneType`).value;
  if (type === 'raw') return original;
  return { type, value: type === 'ctcss' ? $(`${prefix}Ctcss`).value : type === 'dcs' ? $(`${prefix}Dcs`).value : '', polarity: $(`${prefix}Polarity`).value };
}
function startEdit(n) {
  const ch = readChannel(edited, n);
  selectedChannel = n;
  const rx = parseMHz(ch.rx ?? ''), tx = parseMHz(ch.tx ?? '');
  const actualShift = rx !== null && tx !== null ? Number((tx - rx).toFixed(5)) : null;
  const band = rx !== null ? standardOffset(rx) : null;
  const standardShift = band && (ch.offsetDirection === 1 || ch.offsetDirection === 2) ? (ch.offsetDirection === 1 ? -band.offset : band.offset) : null;
  customShift = actualShift !== null && actualShift !== 0 && (ch.offsetDirection === 0 || standardShift === null || Math.abs(actualShift - standardShift) > 0.00001) ? actualShift : null;
  directionAuto = !ch.valid || (ch.offsetDirection === 0 && customShift === null);
  txManuallyEdited = ch.valid && !ch.tx;
  $('editHeading').textContent = `${ch.valid ? 'Edit' : 'Add'} channel ${n}`;
  $('editName').value = ch.valid ? ch.name : '';
  $('editLongName').value = ch.valid ? editedLongNames.get(n) ?? '' : '';
  $('editRx').value = ch.valid ? ch.rx : '';
  $('editTx').value = ch.valid ? ch.tx || '' : '';
  $('editDirection').value = String(ch.valid ? ch.offsetDirection : 0);
  $('editMode').value = ch.valid ? ch.mode : 'FM';
  $('editPower').value = String(ch.valid ? ch.power : 2);
  $('editPttId').value = String(ch.valid ? ch.pttId : 0);
  $('editScramble').querySelector('[data-unknown]')?.remove();
  if (ch.valid && ch.scramble > 16) {
    const option = new Option(`Unknown (${ch.scramble}) · preserve`, String(ch.scramble));
    option.dataset.unknown = 'true';
    $('editScramble').add(option);
  }
  $('editScramble').value = String(ch.valid ? ch.scramble : 0);
  $('editUsed').checked = ch.valid ? ch.used : true;
  $('editScan').checked = ch.valid ? ch.scan : true;
  $('editBusyLock').checked = ch.valid && ch.busyLock;
  $('editHopping').checked = ch.valid && ch.hopping;
  setToneForm('Tx', ch.valid ? ch.txTone : { type: 'off', value: '', polarity: 'N' });
  setToneForm('Rx', ch.valid ? ch.rxTone : { type: 'off', value: '', polarity: 'N' });
  $('clearEdit').hidden = !ch.valid;
  $('editError').textContent = '';
  $('txHint').textContent = txManuallyEdited ? 'Receive-only channel: TX stays blank until you choose a suggested TX.' : customShift !== null ? `Existing custom split (${customShift >= 0 ? '+' : ''}${customShift.toFixed(5)} MHz) will follow RX changes.` : 'Changing RX updates TX. Repeater suggestions use US band-plan defaults; verify the repeater listing.';
  $('editPanel').hidden = false;
  $('editPanel').scrollIntoView({ block: 'nearest', behavior: 'smooth' });
}
function updateSuggestedTx() {
  if (!selectedChannel) return;
  if (txManuallyEdited) { $('txHint').textContent = 'Manual TX is preserved. Choose “Use suggested TX” to resume automatic updates.'; return; }
  const plan = planTransmit($('editRx').value, {direction: Number($('editDirection').value), autoDirection: directionAuto, customShift});
  if (!plan) { $('txHint').textContent = 'Enter a valid RX frequency to calculate TX.'; return; }
  $('editDirection').value = String(plan.direction);
  $('editTx').value = plan.tx ?? '';
  $('txHint').textContent = plan.message;
}
function createChannel() {
  if (!edited) return;
  const entry = $('newChannelNumber').value.trim();
  const n = entry ? Number(entry) : Array.from({length: CHANNEL_COUNT}, (_, i) => i + 1).find(number => !readChannel(edited, number).valid);
  if (!Number.isInteger(n) || n < 1 || n > CHANNEL_COUNT) { status('Choose a channel number from 1 to 199.', true); return; }
  if (readChannel(edited, n).valid) { status(`Channel ${n} is occupied. Choose an empty channel.`, true); return; }
  $('search').value = '';
  $('newChannelNumber').value = '';
  startEdit(n);
}
function applyEdit() {
  if (!edited || !selectedChannel) return;
  try {
    if (parseMHz($('editRx').value) === null) throw new Error('Enter a valid RX frequency in MHz.');
    if (['1','2'].includes($('editDirection').value) && !$('editTx').value.trim()) throw new Error('This repeater needs a TX frequency. Check the direction or enter TX manually.');
    const original = readChannel(edited, selectedChannel);
    const form = {
      name: $('editName').value, rx: $('editRx').value.trim(), tx: $('editTx').value.trim(),
      offsetDirection: $('editDirection').value, mode: $('editMode').value, power: $('editPower').value,
      pttId: $('editPttId').value, scramble: $('editScramble').value,
      used: $('editUsed').checked, scan: $('editScan').checked,
      busyLock: $('editBusyLock').checked, hopping: $('editHopping').checked,
      txTone: toneForm('Tx', original.txTone), rxTone: toneForm('Rx', original.rxTone),
    };
    const longName = checkLongName($('editLongName').value);
    edited = writeChannel(edited, selectedChannel, form);
    if (longName !== (editedLongNames.get(selectedChannel) ?? '')) {
      editedLongNames.set(selectedChannel, longName);
    }
    $('riskAcknowledged').checked = false;
    $('editPanel').hidden = true;
    status(`Channel ${selectedChannel} changes staged.`);
    selectedChannel = null;
    renderChannels();
  } catch (error) { $('editError').textContent = error.message; }
}
function removeChannel(n) {
  if (!edited || !readChannel(edited, n).valid) return;
  const name = readChannel(edited, n).name;
  if (!window.confirm(`Remove channel ${n}${name ? ` (${name})` : ''} from the working image? The radio is not changed until you write.`)) return;
  edited = clearChannel(edited, n);
  editedLongNames.set(n, '');
  $('riskAcknowledged').checked = false;
  status(`Channel ${n} removed from the image. Radio has not been changed.`);
  selectedChannel = null;
  $('editPanel').hidden = true;
  renderChannels();
}
function renderSettings() {
  const root = $('settingsGroups');
  const open = new Set([...root.querySelectorAll('details[open]')].map(node => node.dataset.group));
  root.replaceChildren();
  if (!edited) { root.textContent = 'Read from the radio or open a backup to inspect its settings.'; return; }
  for (const group of SETTING_GROUPS) {
    const details = document.createElement('details');
    details.className = 'setting-group';
    details.dataset.group = group;
    details.open = open.has(group) || (!open.size && (group === 'Radio' || group === 'APRS identity'));
    const summary = document.createElement('summary');
    summary.textContent = `${group} · ${SETTINGS.filter(s => s.section === group).length}`;
    details.append(summary);
    const grid = document.createElement('div');
    grid.className = 'settings-grid';
    for (const setting of SETTINGS.filter(s => s.section === group)) {
      const value = readSetting(edited, setting);
      const field = document.createElement('div');
      field.className = 'setting-field';
      const title = document.createElement('div');
      title.className = 'option-help setting-title';
      const label = document.createElement('label');
      label.textContent = setting.label;
      label.htmlFor = `setting-${setting.id}`;
      const helpId = `setting-help-${setting.id}`;
      const info = document.createElement('button');
      info.className = 'info-button';
      info.type = 'button';
      info.setAttribute('aria-label', `About ${setting.label}`);
      info.setAttribute('aria-describedby', helpId);
      info.setAttribute('aria-expanded', 'false');
      const tip = document.createElement('span');
      tip.className = 'tooltip-text';
      tip.id = helpId;
      tip.setAttribute('role', 'tooltip');
      tip.textContent = SETTING_HELP[setting.id];
      title.append(label, info, tip);
      let control;
      if (setting.type === 'select') {
        control = document.createElement('select');
        for (const [n, caption] of setting.options) control.add(new Option(caption, String(n)));
        if (![...control.options].some(o => o.value === value)) control.add(new Option(`Unknown (${value}) · preserve`, value));
      } else {
        control = document.createElement('input');
        control.type = setting.type === 'text' ? (setting.sensitive ? 'password' : 'text') : 'number';
        if (setting.type === 'text') control.maxLength = setting.length;
        else { control.min = setting.min; control.max = setting.max; control.step = 1; }
        control.autocomplete = 'off';
        control.spellcheck = false;
      }
      control.id = `setting-${setting.id}`;
      control.setAttribute('aria-describedby', helpId);
      control.value = value;
      if (value === 'Uninitialized') {
        control.disabled = true;
      }
      control.dataset.setting = setting.id;
      control.dataset.original = value;
      field.append(title, control);
      grid.append(field);
    }
    details.append(grid);
    if (group === 'GNSS') {
      const fixed = document.createElement('p');
      fixed.className = 'fine setting-note';
      fixed.textContent = `Fixed position: ${readFixedPosition(edited)}. This value is shown for inspection and is not editable here.`;
      details.append(fixed);
    }
    root.append(details);
  }
  $('settingsCount').textContent = `${SETTINGS.length} mapped`;
}
function applySettings() {
  if (!edited) return;
  const controls = [...$('settingsGroups').querySelectorAll('[data-setting]')];
  const changed = controls.filter(c => c.value !== c.dataset.original);
  if (!changed.length) { status('Settings unchanged.'); return; }
  try {
    let next = edited;
    for (const control of changed) {
      const setting = SETTINGS.find(s => s.id === control.dataset.setting);
      next = writeSetting(next, setting, control.value);
    }
    edited = next;
    $('riskAcknowledged').checked = false;
    $('settingsError').textContent = '';
    renderSettings();
    refresh();
    status(`${changed.length} setting${changed.length === 1 ? '' : 's'} updated in the image. Radio has not been changed.`);
  } catch (error) { $('settingsError').textContent = error.message; }
}
function changedBlocks() {
  if (!baseline || !edited) return [];
  const changed = [];
  for (let address = 0; address < MEM_SIZE; address += BLOCK) {
    if (!same(baseline.slice(address + 8, address + 8 + BLOCK), edited.slice(address + 8, address + 8 + BLOCK))) changed.push(address);
  }
  return changed;
}
function pendingLongNames() {
  return baseline && edited ? longNameChanges(baselineLongNames, editedLongNames, baseline, edited) : [];
}

async function writeChanges() {
  await work(async () => {
    if (!$('riskAcknowledged').checked || !baseline || !edited) throw new Error('Review your channels and settings and confirm you are ready to write first.');
    const addresses = changedBlocks();
    const names = pendingLongNames();
    if (!addresses.length && !names.length) return;
    if (addresses.some(a => !(a < 0x1f80 || (a >= 0x3000 && a < 0x3140)))) throw new Error('An edit touches an unsupported radio memory region.');
    let entered = false;
    let writes = 0;
    try {
      status('Checking radio before writing changes…');
      let ident = await enterProgramming(); entered = true;
      if (!same(ident, baseline.slice(0, 8))) throw new Error(`Radio mode differs from backup (${modeNames[ident[7]]} vs ${modeName(baseline)}). No data written.`);
      const verifiedNames = [];
      if (usbSession || names.length) {
        const capability = await probeCloneCapabilities();
        if (usbSession && !capability.sparseSerialWrites) {
          sparseSerialWrites = false;
          throw new Error('This firmware supports USB-C reads only. Connect over Bluetooth to write changes.');
        }
        if (!capability.longNames && names.some(change => !change.implicit)) {
          throw new Error('Upgrade to H9 Toolbox Firmware with channel long-name support before writing these changes.');
        }
        if (capability.resync) {
          await exitProgramming(); entered = false;
          await new Promise(resolve => setTimeout(resolve, 300));
          ident = await enterProgramming(); entered = true;
          if (!same(ident, baseline.slice(0, 8))) throw new Error('Radio mode differs from backup. No data written.');
        }
        if (capability.longNames) {
          for (const change of names) {
            const current = await readLongName(labelRadio, change.channel);
            if (!change.implicit && current !== change.previous && current !== change.next) {
              throw new Error(`Channel ${change.channel} long name changed since the backup. Read the radio again before writing. No data written.`);
            }
            verifiedNames.push({...change, current});
          }
          baselineLongNames ??= new Map();
          for (const change of verifiedNames) {
            baselineLongNames.set(change.channel, change.current);
            if (change.implicit) editedLongNames.set(change.channel, '');
          }
        }
      }
      const nameWrites = verifiedNames.filter(change => change.current !== change.next);
      const total = addresses.length + nameWrites.length;
      for (const address of addresses) {
        const original = baseline.slice(address + 8, address + 8 + BLOCK);
        const current = await readBlock(address);
        if (!same(current, original)) throw new Error(`Radio memory changed at 0x${address.toString(16)} since the backup. No data written.`);
      }
      if (total) startProgress('Writing verified changes');
      for (const address of addresses) {
        const next = edited.slice(address + 8, address + 8 + BLOCK);
        await writeBlock(address, next);
        const verify = await readBlock(address);
        if (!same(verify, next)) throw new Error(`Readback differs at 0x${address.toString(16)}. Stopped after ${writes} verified writes.`);
        baseline.set(next, address + 8);
        showProgress(++writes, total);
      }
      for (const change of nameWrites) {
        await writeLongName(labelRadio, change.channel, change.next);
        baselineLongNames.set(change.channel, change.next);
        showProgress(++writes, total);
      }
      $('riskAcknowledged').checked = false;
      status(total ? `${addresses.length} image block${addresses.length === 1 ? '' : 's'} and ${nameWrites.length} long name${nameWrites.length === 1 ? '' : 's'} written and read back.` :
        'The radio already matches these changes.');
      renderChannels();
      renderSettings();
    } finally { if (entered) await exitProgramming(); }
  });
}

$('previousStep').addEventListener('click', () => showStep(currentStep - 1));
$('nextStep').addEventListener('click', () => showStep(currentStep + 1));
$('reconnectStep').addEventListener('click', () => showStep(0));
$('connect').addEventListener('click', () => startConnection(connectBluetooth));
$('connectUsb').addEventListener('click', () => startConnection(connectSerial));
$('disconnect').addEventListener('click', () => work(async () => {
  await closeConnection();
  status('Radio disconnected.');
}));
$('read').addEventListener('click', () => work(readRadio));
$('save').addEventListener('click', saveBackup);
$('saveConnectedBackup').addEventListener('click', saveConnectedBackup);
$('useConnectedImage').addEventListener('click', useConnectedImage);
$('exportJson').addEventListener('click', () => downloadJson(false));
$('exportArchive').addEventListener('click', () => downloadJson(true));
$('exportImg').addEventListener('click', exportImg);
$('exportChannels').addEventListener('click', downloadChannels);
$('openFile').addEventListener('change', openFile);
$('openChannels').addEventListener('change', openChannels);
function closeInfoTips() {
  for (const button of document.querySelectorAll('.info-button[aria-expanded="true"]')) button.setAttribute('aria-expanded', 'false');
}
document.addEventListener('click', event => {
  const button = event.target.closest('.info-button');
  if (!button) { closeInfoTips(); return; }
  const wasOpen = button.getAttribute('aria-expanded') === 'true';
  closeInfoTips();
  button.setAttribute('aria-expanded', String(!wasOpen));
});
document.addEventListener('keydown', event => {
  if (event.key === 'Escape') {
    closeInfoTips();
    if (document.activeElement?.matches?.('.info-button')) document.activeElement.blur();
  }
});
$('search').addEventListener('input', renderChannels);
$('showAll').addEventListener('change', renderChannels);
$('channelRows').addEventListener('click', event => {
  const remove = event.target.closest('[data-remove]')?.dataset.remove;
  if (remove) { removeChannel(Number(remove)); return; }
  const n = event.target.closest('[data-channel]')?.dataset.channel;
  if (n) startEdit(Number(n));
});
$('createChannel').addEventListener('click', createChannel);
$('newChannelNumber').addEventListener('keydown', event => { if (event.key === 'Enter') createChannel(); });
$('editRx').addEventListener('input', updateSuggestedTx);
$('editTx').addEventListener('input', () => { txManuallyEdited = true; $('txHint').textContent = $('editTx').value.trim() ? 'Manual TX is preserved when RX changes.' : 'Receive-only: TX stays blank when RX changes.'; });
$('editDirection').addEventListener('change', () => { directionAuto = false; txManuallyEdited = false; customShift = null; updateSuggestedTx(); });
$('suggestTx').addEventListener('click', () => { directionAuto = true; txManuallyEdited = false; customShift = null; updateSuggestedTx(); });
$('cancelEdit').addEventListener('click', () => { selectedChannel = null; $('editPanel').hidden = true; });
$('applyEdit').addEventListener('click', applyEdit);
$('clearEdit').addEventListener('click', () => { if (selectedChannel) removeChannel(selectedChannel); });
$('applySettings').addEventListener('click', applySettings);
for (const side of ['Tx', 'Rx']) {
  const prefix = `edit${side}`;
  $(`${prefix}Ctcss`).replaceChildren(...CTCSS_TONES.map(value => new Option(value.toFixed(1), value.toFixed(1))));
  $(`${prefix}Dcs`).replaceChildren(...DCS_CODES.map(value => new Option(value, value)));
  $(`${prefix}ToneType`).addEventListener('change', () => updateToneControls(side));
}
$('editScramble').replaceChildren(new Option('Off', '0'), ...Array.from({ length: 16 }, (_, i) => new Option(`Code ${i + 1}`, String(i + 1))));
$('riskAcknowledged').addEventListener('change', refresh);
$('write').addEventListener('click', writeChanges);
refresh();

logoTool = mountLogoTool($('logoTool'), {
  onStatus: (message, error) => status(message, error),
  connection: {
    session: () => (toolbox && resourceRadio && connectedName() ? { radio: resourceRadio, name: connectedName() } : null),
    onBusy: flag => { logoBusy = flag; busy = flag; refresh(); },
    blocked: () => busy && !logoBusy,
  },
});
refresh();
