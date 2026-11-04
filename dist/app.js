import { CHANNEL_COUNT, CTCSS_TONES, DCS_CODES, readChannel, writeChannel, clearChannel } from './channel.mjs';
import { SETTINGS, SETTING_GROUPS, readSetting, writeSetting, readFixedPosition } from './settings.mjs';
import { SETTING_HELP } from './settings-help.mjs';
import { exportPortable, importPortable } from './portable.mjs';
import { parseMHz, standardOffset, planTransmit } from './channel-plan.mjs';
import { exportChannels, inspectChannelImport, importChannels } from './channels-file.mjs';
import { readBlockReply } from './protocol.mjs';

const $ = id => document.getElementById(id);
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
let baseline = null;
let edited = null;
let originalBackup = null;
let backupSaved = false;
let selectedChannel = null;
let txManuallyEdited = false;
let directionAuto = false;
let customShift = null;
let busy = false;
let incoming = [];
let incomingWake = null;
let readReplyMode = 'auto';

function log(message) {
  const item = document.createElement('li');
  item.textContent = `${new Date().toLocaleTimeString()} · ${message}`;
  $('log').prepend(item);
  while ($('log').children.length > 45) $('log').lastChild.remove();
}
function status(message, error = false) {
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
function refresh() {
  const connected = !!device?.gatt?.connected && !!writeChar && !!notifyChar;
  $('connection').classList.toggle('connected', connected);
  $('connection').innerHTML = `<i></i> ${connected ? 'Connected to ' + escapeHtml(device.name || 'TD-H9') : 'Disconnected'}`;
  $('connect').disabled = busy || connected;
  $('disconnect').disabled = busy || !device;
  $('read').disabled = busy || !connected;
  $('save').disabled = busy || !baseline;
  $('exportJson').disabled = busy || !edited;
  $('exportArchive').disabled = busy || !edited;
  $('exportImg').disabled = busy || !edited;
  $('search').disabled = !edited;
  $('showAll').disabled = !edited;
  $('createChannel').disabled = busy || !edited;
  $('newChannelNumber').disabled = busy || !edited;
  $('exportChannels').disabled = busy || !edited;
  $('applySettings').disabled = busy || !edited;
  $('backupConfirmed').disabled = !edited || !backupSaved;
  const count = edited && baseline ? changedBlocks().length : 0;
  $('changeCount').textContent = count ? `${count} changed block${count === 1 ? '' : 's'}` : 'No changes';
  $('write').disabled = busy || !connected || !backupSaved || !$('backupConfirmed').checked || !count;
  $('radioMode').textContent = edited ? `${modeName(edited)} image` : 'No image';
  $('imageInfo').textContent = edited ? `${IMAGE_SIZE.toLocaleString()} bytes · ${modeName(edited)} · ${backupSaved ? 'Original backup saved' : 'Save the original backup before writing'}` : 'No radio image loaded.';
}
function escapeHtml(str) { return str.replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]); }
function modeName(image) { return modeNames[image[7]] || 'Unknown'; }
function validateImage(bytes) {
  if (bytes.length !== IMAGE_SIZE) throw new Error(`Expected ${IMAGE_SIZE} bytes, got ${bytes.length}.`);
  if (!same(bytes.slice(0, 7), Uint8Array.of(0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff)) || !modeNames[bytes[7]]) throw new Error('This is not a TD-H9 Normal, HAM, or GMRS image.');
}
function onNotification(event) {
  incoming.push(...new Uint8Array(event.target.value.buffer, event.target.value.byteOffset, event.target.value.byteLength));
  if (incomingWake) { incomingWake(); incomingWake = null; }
}
async function readExact(length, timeoutMs = 2500) {
  const out = new Uint8Array(length);
  let at = 0;
  const deadline = Date.now() + timeoutMs;
  while (at < length) {
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
  if (!writeChar) throw new Error('Bluetooth is disconnected.');
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
    const ident = await readExact(8);
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
  try { if (device?.gatt?.connected) await send(Uint8Array.of(0x45)); }
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
function showProgress(done, total) {
  $('progressWrap').hidden = false;
  const percent = Math.round(done * 100 / total);
  $('progressBar').style.width = `${percent}%`;
  $('progressLabel').textContent = `${percent}%`;
}
async function work(fn) {
  if (busy) return;
  busy = true; refresh();
  try { await fn(); }
  catch (error) { status(error.message || String(error), true); }
  finally { busy = false; refresh(); }
}
async function connect() {
  await work(async () => {
    if (!navigator.bluetooth) throw new Error('Web Bluetooth is unavailable here. Open this page in Chrome or Edge over HTTPS.');
    let profiles = selectedProfiles();
    const extra = $('customUuid').value.trim();
    const extraUuid = extra ? serviceUuid(extra) : null;
    if ($('profile').value === 'custom' && !extraUuid) throw new Error('Enter the custom service UUID first.');
    const optionalServices = [...new Set([...profiles.map(p => p.service), ...(extraUuid ? [extraUuid] : [])])];
    status('Choose your TD-H9 in the Bluetooth device picker.');
    device = await navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'TD-H9' }], optionalServices });
    device.addEventListener('gattserverdisconnected', () => { writeChar = null; notifyChar = null; status('Radio disconnected.'); refresh(); });
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
    notifyChar.addEventListener('characteristicvaluechanged', onNotification);
    await notifyChar.startNotifications();
    status(`Connected to ${device.name || 'TD-H9'} via ${pair.name}.`);
  });
}
async function disconnect() {
  if (device?.gatt?.connected) device.gatt.disconnect();
  writeChar = null; notifyChar = null; incoming = [];
  status('Radio disconnected.'); refresh();
}
async function download() {
  await work(async () => {
    let entered = false;
    try {
      status('Starting TD-H9 backup…');
      const ident = await enterProgramming(); entered = true;
      const image = new Uint8Array(IMAGE_SIZE);
      image.set(ident);
      for (let address = 0; address < MEM_SIZE; address += BLOCK) {
        image.set(await readBlock(address), address + 8);
        if (address % (BLOCK * 8) === 0 || address + BLOCK === MEM_SIZE) {
          showProgress(address + BLOCK, MEM_SIZE);
          await new Promise(resolve => setTimeout(resolve, 0));
        }
      }
      validateImage(image);
      baseline = image;
      edited = image.slice();
      originalBackup = image.slice();
      backupSaved = false;
      $('backupConfirmed').checked = false;
      renderChannels();
      renderSettings();
      status(`Read complete: ${modeName(image)} image, ${IMAGE_SIZE.toLocaleString()} bytes. Save the untouched backup now.`);
    } finally { if (entered) await exitProgramming(); }
  });
}
function saveBackup() {
  if (!originalBackup) return;
  const blob = new Blob([originalBackup], { type: 'application/octet-stream' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url; a.download = `TD-H9-${modeName(originalBackup)}-original-${new Date().toISOString().slice(0, 10)}.img`;
  document.body.append(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 30000);
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
    const document = exportPortable(baseline, edited, {archive});
    const bytes = JSON.stringify(document, null, 2) + '\n';
    saveFile(bytes, `TD-H9-${modeName(edited)}-${archive ? 'archive' : 'config'}-${new Date().toISOString().slice(0, 10)}.json`, 'application/json');
    status(archive ? 'Lossless JSON archive downloaded with complete original image bytes.' : 'Readable configuration JSON downloaded. Load an .img backup or read the destination radio before importing it.');
  } catch (error) { status(error.message, true); }
}
function exportImg() {
  if (!edited) return;
  saveFile(edited, `TD-H9-${modeName(edited)}-working-${new Date().toISOString().slice(0, 10)}.img`, 'application/octet-stream');
  status('Working .img downloaded. Keep the separate original backup unchanged.');
}
function downloadChannels() {
  if (!edited) return;
  try {
    const document = exportChannels(edited);
    saveFile(JSON.stringify(document, null, 2) + '\n', `TD-H9-${modeName(edited)}-channels-${new Date().toISOString().slice(0, 10)}.json`, 'application/json');
    status(`${document.channels.length} channels exported as JSON. Radio settings and raw memory are not included.`);
  } catch (error) { status(error.message, true); }
}
async function openChannels(event) {
  const file = event.target.files?.[0]; if (!file) return;
  await work(async () => {
    if (!edited) throw new Error('Read the radio or open a backup before importing channels.');
    if (file.size > 2_000_000) throw new Error('Channels file is too large.');
    let document;
    try { document = JSON.parse(await file.text()); }
    catch { throw new Error('Could not parse the channels JSON file.'); }
    const plan = inspectChannelImport(edited, document);
    if ((plan.conflicts.length || plan.removals.length) && !window.confirm(`Import will replace ${plan.conflicts.length} occupied channel${plan.conflicts.length === 1 ? '' : 's'} and remove ${plan.removals.length} channel${plan.removals.length === 1 ? '' : 's'}${document.replaceAll === true ? ' because replaceAll is enabled' : ''}. Continue?`)) return;
    const result = importChannels(edited, document);
    edited = result.image;
    selectedChannel = null;
    $('editPanel').hidden = true;
    $('backupConfirmed').checked = false;
    $('search').value = '';
    renderChannels();
    status(`Imported ${document.channels.length} channel entries: ${result.additions.length} added, ${result.conflicts.length} replaced, ${result.removals.length} removed, ${result.unchanged.length} unchanged. Radio has not been changed.`);
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
    const parsed = importPortable(document, configuration ? edited : null);
    if (!configuration) {
      baseline = parsed.original;
      originalBackup = parsed.original.slice();
      backupSaved = true;
    }
    edited = parsed.edited;
    $('backupConfirmed').checked = false;
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
    backupSaved = true;
    $('backupConfirmed').checked = false;
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
    if (search && !`${n} ${ch.name} ${ch.rx || ''} ${ch.tx || ''} ${ch.mode} ${toneSummary(ch)}`.toLowerCase().includes(search)) continue;
    const row = document.createElement('tr');
    row.innerHTML = `<td>${n.toString().padStart(3, '0')}</td><td>${escapeHtml(ch.name || '—')}</td><td>${ch.rx || '—'}</td><td>${ch.tx || '—'}</td><td>${ch.valid ? ch.mode : '—'}</td><td>${ch.valid ? escapeHtml(toneSummary(ch)) : '—'}</td><td>${ch.valid ? ['Low', 'Mid', 'High', '?'][ch.power] : '—'}</td><td>${ch.valid ? (ch.scan ? 'Yes' : 'No') : '—'}</td><td class="channel-actions"><button class="text-button" data-channel="${n}" aria-label="${ch.valid ? 'Edit' : 'Add'} channel ${n}">${ch.valid ? 'Edit' : 'Add'}</button>${ch.valid ? `<button class="text-button danger-text" data-remove="${n}" aria-label="Remove channel ${n}">Remove</button>` : ''}</td>`;
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
    edited = writeChannel(edited, selectedChannel, form);
    $('editPanel').hidden = true;
    status(`Channel ${selectedChannel} updated in the image. Radio has not been changed.`);
    selectedChannel = null;
    renderChannels();
  } catch (error) { $('editError').textContent = error.message; }
}
function removeChannel(n) {
  if (!edited || !readChannel(edited, n).valid) return;
  const name = readChannel(edited, n).name;
  if (!window.confirm(`Remove channel ${n}${name ? ` (${name})` : ''} from the working image? The radio is not changed until you write.`)) return;
  edited = clearChannel(edited, n);
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
async function writeChanges() {
  await work(async () => {
    if (!backupSaved || !$('backupConfirmed').checked || !baseline || !edited) throw new Error('Save and confirm the original backup first.');
    const addresses = changedBlocks();
    if (!addresses.length) return;
    if (addresses.some(a => !(a < 0x1f80 || (a >= 0x3000 && a < 0x3140)))) throw new Error('An edit touches an unsupported radio memory region.');
    let entered = false;
    let writes = 0;
    try {
      status(`Checking radio before ${addresses.length} block write${addresses.length === 1 ? '' : 's'}…`);
      const ident = await enterProgramming(); entered = true;
      if (!same(ident, baseline.slice(0, 8))) throw new Error(`Radio mode differs from backup (${modeNames[ident[7]]} vs ${modeName(baseline)}). No data written.`);
      for (let i = 0; i < addresses.length; i++) {
        const address = addresses[i];
        const original = baseline.slice(address + 8, address + 8 + BLOCK);
        const next = edited.slice(address + 8, address + 8 + BLOCK);
        const current = await readBlock(address);
        if (!same(current, original)) throw new Error(`Radio memory changed at 0x${address.toString(16)} since the backup. Stopped after ${writes} writes.`);
        const checksum = next.reduce((sum, b) => (sum + b) & 255, 0);
        await send(Uint8Array.of(0x57, (address >> 8) & 255, address & 255, BLOCK, ...next, checksum));
        await expectAck(`Write at 0x${address.toString(16)}`);
        writes++;
        const verify = await readBlock(address);
        if (!same(verify, next)) throw new Error(`Readback differs at 0x${address.toString(16)}. Stopped after ${writes} writes.`);
        showProgress(i + 1, addresses.length);
      }
      baseline = edited.slice();
      status(`${writes} changed block${writes === 1 ? '' : 's'} written and read back. Check the changed settings on the radio after it restarts.`);
      renderChannels();
      renderSettings();
    } finally { if (entered) await exitProgramming(); }
  });
}

$('connect').addEventListener('click', connect);
$('disconnect').addEventListener('click', disconnect);
$('read').addEventListener('click', download);
$('save').addEventListener('click', saveBackup);
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
$('backupConfirmed').addEventListener('change', refresh);
$('write').addEventListener('click', writeChanges);
refresh();
