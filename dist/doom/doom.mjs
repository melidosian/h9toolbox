// Doom for the TD-H9: the installer page logic (/doom/).
//
// 01 Install over USB-C in the vendor HID recovery mode (PTT + 2), from the
//    files hosted next to this page (checked against doom-manifest.json) or
//    the user's own build, with the web flasher's HID protocol: back up,
//    write the Doom code region at 0xEA000 first (inert until the firmware
//    calls it), then the firmware sectors, then verify every sector by CRC.
// 02 Game data and music over Bluetooth to the Doom firmware's upload
//    mode (hold 2 at power-on, or automatic when there is no game data).
//
// Nothing the user selects or the radio returns leaves the browser.

import {
  APP_START, SECTOR_SIZE, loadBuildPackage, loadStockSeed, scanRadio, sha256, writeSector,
} from '../firmware-update/firmware.mjs?v=20261109d';
import { createHidArchive, readAccessibleFlash } from '../firmware-update/readback.mjs?v=20261109d';
import {
  BLANK_CRC, DoomHid, FAR_END, FAR_START, checkDoomCode, checkFirmware, checkPack, checkSong,
  doomTarget, planInstall,
} from './install.mjs?v=20261109e';

function requireValue(condition, message) { if (!condition) throw new Error(message); }
const hex = value => `0x${value.toString(16)}`;

// ---- page

const $ = id => document.getElementById(id);
const files = { firmware: null, code: null, pack: null, song: null };  // { bytes, label, ... }
let device = null, hid = null, busy = false, backedUp = false, references = null, session = null;

function log(message, error = false) {
  const item = document.createElement('li');
  item.textContent = `${new Date().toLocaleTimeString()} ${message}`;
  if (error) item.className = 'error';
  $('log').prepend(item);
  $('status').textContent = message;
  (error ? console.error : console.info)('[doom]', message);
}
function progress(done, total, text) {
  const fraction = total ? Math.min(1, done / total) : 0;
  $('progress').style.width = `${(fraction * 100).toFixed(1)}%`;
  $('progressText').textContent = text ?? `${Math.floor(fraction * 100)}%`;
}
const kb = n => `${(n / 1024).toLocaleString(undefined, { maximumFractionDigits: 0 })} KB`;

function refresh() {
  const haveInstall = files.firmware && files.code;
  $('connect').disabled = busy || !!hid;
  $('disconnect').disabled = busy || !hid;
  $('backup').disabled = busy || !hid || !haveInstall;
  $('install').disabled = busy || !hid || !haveInstall || !(backedUp || $('skipBackup').checked) || !$('acknowledge').checked;
  $('btConnect').disabled = busy || !!session;
  $('btDisconnect').disabled = busy || !session;
  // Step 02 works on its own: without loaded files the upload loads Doom first.
  $('uploadPack').disabled = busy || !session;
  $('uploadSong').disabled = busy || !session;
  $('removeData').disabled = busy || !session;
  for (const [key, id] of [['firmware', 'rowFirmware'], ['code', 'rowCode'], ['pack', 'rowPack'], ['song', 'rowSong']]) {
    const row = $(id), entry = files[key];
    row.classList.toggle('ready', !!entry);
    row.querySelector('.state').textContent = entry ? `✓ ${entry.label} · ${kb(entry.bytes.length)}` : 'Not loaded';
  }
}
async function task(work) {
  if (busy) return;
  busy = true; refresh();
  try { await work(); } catch (error) { log(error.message, true); } finally { busy = false; refresh(); }
}

function setFile(key, bytes, label, extra = {}) {
  const check = { firmware: checkFirmware, code: checkDoomCode, pack: checkPack, song: checkSong }[key];
  const checked = check(bytes);
  files[key] = { bytes, checked, label, ...extra };
}

// Hosted release, as the web flasher hosts H9 Toolbox Firmware: files next to
// this page (files/, staged by doom-firmware/stage_release.py), each checked
// against doom-manifest.json. Loaded into the page only; nothing is saved.
const HOSTED = './files/';

function choose(id) {
  $('sourceGrid').classList.add('has-selection');
  for (const option of ['optHosted', 'optOwn']) $(option).classList.toggle('selected', option === id);
}

async function loadHosted() {
  const response = await fetch(HOSTED + 'doom-manifest.json', { cache: 'no-store' });
  requireValue(response.ok, `The Doom files are not on this site (HTTP ${response.status}). Use your own build.`);
  const manifest = await response.json();
  const wanted = [['firmware', manifest.firmware], ['code', manifest.code], ['pack', manifest.pack], ['song', manifest.music?.[0]]];
  for (const [key, entry] of wanted) {
    if (!entry) continue;
    log(`Loading ${entry.name} (${kb(entry.bytes)})…`);
    const fileResponse = await fetch(HOSTED + entry.file, { cache: 'no-store' });
    requireValue(fileResponse.ok, `Could not load ${entry.file} (HTTP ${fileResponse.status}).`);
    const bytes = new Uint8Array(await fileResponse.arrayBuffer());
    requireValue(await sha256(bytes) === entry.sha256, `${entry.file} does not match its exact hash.`);
    setFile(key, bytes, entry.name ?? entry.file);
  }
  if (manifest.firmware?.imageSha256)
    requireValue(await sha256(files.firmware.checked) === manifest.firmware.imageSha256, 'The firmware image does not match its exact hash.');
  choose('optHosted');
  log(`Loaded Doom ${manifest.version ?? ''}: every file matched its exact hash. Connect the radio next.`);
}

for (const [key, id] of [['firmware', 'ownFirmware'], ['code', 'ownCode'], ['pack', 'ownPack'], ['song', 'ownSong']]) {
  $(id).addEventListener('change', () => task(async () => {
    const file = $(id).files[0];
    if (!file) return;
    setFile(key, new Uint8Array(await file.arrayBuffer()), file.name);
    choose('optOwn');
    log(`${file.name}: checked (${kb(files[key].bytes.length)}).`);
  }));
}
$('loadHosted').addEventListener('click', () => task(loadHosted));

// USB (HID) ---------------------------------------------------------------

$('connect').addEventListener('click', () => task(async () => {
  requireValue('hid' in navigator, 'This browser has no WebHID. Use desktop Chrome or Edge.');
  const devices = await navigator.hid.requestDevice({ filters: [{ vendorId: 0x4c4a, productId: 0x4155, usagePage: 0xff00, usage: 1 }] });
  if (!devices.length) return;
  requireValue(devices.length === 1, 'Choose exactly one JieLi vendor HID interface.');
  await devices[0].open();
  const candidate = new DoomHid(devices[0]);
  await candidate.initialize();
  device = devices[0]; hid = candidate; backedUp = false;
  $('deviceState').textContent = 'Connected';
  log('Radio connected in USB recovery mode. Back it up next; nothing has been written.');
}));
$('disconnect').addEventListener('click', () => task(async () => {
  await device?.close(); device = null; hid = null;
  $('deviceState').textContent = 'Disconnected';
  log('USB disconnected.');
}));

async function knownImages() {
  if (!references) {
    const [pkg, seed] = await Promise.all([
      fetch('../firmware-update/h9-toolbox-firmware-v1.0.38.fw').then(r => r.arrayBuffer()),
      fetch('../firmware-update/h9-toolbox-firmware-stock-sectors.bin').then(r => r.arrayBuffer()),
    ]);
    const build = await loadBuildPackage(new Uint8Array(pkg));
    const stock = await loadStockSeed(new Uint8Array(seed), build.image);
    references = [stock, build.image];
  }
  return [...references, files.firmware.checked];
}

$('backup').addEventListener('click', () => task(async () => {
  log('Scanning sector CRCs (read only)…');
  const current = await scanRadio(hid, (done, total) => progress(done, total, `Scanning ${done}/${total}`));
  log('Rebuilding the firmware area from CRC windows. Seconds on stock, H9 Toolbox or this Doom build; up to an hour on other firmware.');
  const recovered = await readAccessibleFlash(hid, current, await knownImages(),
    (done, total) => progress(done, total), () => false, { settings: false });
  const archive = await createHidArchive(recovered.accessible, recovered.method);
  const name = `TD-H9-before-Doom-HID-recovery-${new Date().toISOString().slice(0, 10)}.h9hid`;
  const url = URL.createObjectURL(new Blob([archive], { type: 'application/octet-stream' }));
  const link = Object.assign(document.createElement('a'), { href: url, download: name });
  document.body.append(link); link.click(); link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
  const codeBlank = current.slice((FAR_START - APP_START) / SECTOR_SIZE, (FAR_END - APP_START) / SECTOR_SIZE).every(crc => crc === BLANK_CRC);
  backedUp = true;
  progress(1, 1, 'Backup saved');
  log(`Backup saved as ${name}. Keep it: /firmware-update/ restores from it. Doom code area: ${codeBlank ? 'empty' : 'holds earlier data (it is only used by Doom)'}.`);
}));

$('install').addEventListener('click', () => task(async () => {
  const whole = doomTarget(files.firmware.checked, files.code.checked);
  log('Checking every sector on the radio (CRC only)…');
  let current = await scanRadio(hid, (done, total) => progress(done, total, `Checking ${done}/${total}`));
  const plan = planInstall(current, whole, files.code.checked.length);
  if (!plan.sectors.length) { log('The radio already holds this Doom build. Nothing to write.'); progress(1, 1, 'Up to date'); return; }
  log(`Writing ${plan.code.length} Doom code sector(s) at ${hex(FAR_START)}, then ${plan.firmware.length} firmware sector(s).`);
  let done = 0;
  for (const physical of plan.sectors) {
    await writeSector(hid, physical, whole);
    done += 1;
    progress(done, plan.sectors.length, `Writing ${done}/${plan.sectors.length} (${hex(physical)})`);
  }
  log('Verifying every sector…');
  current = await scanRadio(hid, (n, total) => progress(n, total, `Verifying ${n}/${total}`));
  const left = planInstall(current, whole, files.code.checked.length);
  requireValue(!left.sectors.length, `Verification failed at ${left.sectors.map(hex).join(', ')}. Run Install again (it resumes).`);
  progress(1, 1, 'Installed');
  log('Doom installed and verified. Unplug USB and power-cycle the radio. If the game data is not on the radio yet, it opens the Bluetooth upload screen by itself (step 02); if it is, Doom starts.');
}));

for (const id of ['skipBackup', 'acknowledge']) $(id).addEventListener('change', refresh);

// Bluetooth --------------------------------------------------------------

$('btConnect').addEventListener('click', () => task(async () => {
  const { connectBluetooth } = await import('../boot-logo/radio.mjs?v=up3');
  session = await connectBluetooth({
    log: (message, level) => { if (level !== 'debug') log(message, level === 'error'); },
    onDisconnect: () => { session = null; $('btState').textContent = 'Disconnected'; log('Bluetooth disconnected.'); refresh(); },
  });
  const service = await session.radio.checkService();
  requireValue(service.version === 2, 'The radio is not in Doom upload mode (or H9 Toolbox Firmware): the write service is missing.');
  $('btState').textContent = `Connected · ${session.name}`;
  log('Bluetooth connected: upload service ready.');
}));
$('btDisconnect').addEventListener('click', () => { session?.disconnect(); session = null; $('btState').textContent = 'Disconnected'; refresh(); });

async function upload(key, slot) {
  if (!files[key]) {
    log('Loading the Doom files first (step 01, Load Doom)…');
    await loadHosted();
    requireValue(files[key], key === 'song' ? 'This release has no music.' : 'The game data did not load.');
  }
  const { SLOTS, uploadPack } = await import('./upload.mjs?v=20261109g');
  const began = performance.now();
  const entry = files[key];
  const manifest = await uploadPack(session.radio, entry.checked, {
    name: entry.label.slice(0, 32), slot: SLOTS[slot],
    onProgress: (fraction, phase, rate) => {
      const elapsed = (performance.now() - began) / 1000;
      const left = fraction > 0.03 ? ` · about ${Math.ceil(elapsed * (1 - fraction) / fraction / 60)} min left` : '';
      progress(fraction, 1, `${Math.floor(fraction * 100)}% · ${phase}${rate ? ` · ${(rate / 1024).toFixed(2)} KiB/s` : ''}${left}`);
    },
  });
  log(manifest.alreadyComplete ? `${entry.label} is already on the radio; nothing to upload.`
    : `${entry.label}: uploaded and verified (${manifest.bytes.toLocaleString()} bytes, ${(manifest.totalSeconds / 60).toFixed(1)} min).`);
}
$('removeData').addEventListener('click', () => task(async () => {
  const { SLOTS, restore } = await import('./upload.mjs?v=20261109g');
  let erased = 0;
  for (const slot of [SLOTS.music, SLOTS.game]) {
    log(`Removing the ${slot.name === 'game' ? 'game data' : 'music'}…`);
    const result = await restore(session.radio, {
      slot, onProgress: (fraction, phase) => progress(fraction, 1, `${slot.name}: ${phase}`),
    });
    erased += result.erased;
    log(result.note ?? `${slot.name === 'game' ? 'Game data' : 'Music'} removed: ${result.erased} sector(s) erased and read back as erased.`);
  }
  progress(1, 1, 'Removed');
  log(`Doom data removed (${erased} sector(s)). The Doom firmware will now open its upload screen at power-on.`);
}));
$('uploadPack').addEventListener('click', () => task(() => upload('pack', 'game')));
$('uploadSong').addEventListener('click', () => task(() => upload('song', 'music')));

refresh();
