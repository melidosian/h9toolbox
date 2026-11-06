import { APP_START, IMAGE_SIZE, SECTOR_SIZE, BRICK_BREAKER_IMAGE_SHA256, BRICK_BREAKER_SECTORS, classifyBrickBreakerRadio, crc16, expectedCrcs, H9Hid, loadBackup, loadBrickBreakerPackage, loadBrickBreakerStockSeed, loadStockPackage, planUpdate, scanRadio, sha256, verifyBrickBreakerBackup, writeSector } from './firmware.mjs?v=20261106d';
import { createHidArchive, knownAppMatches, loadHidArchive, readAccessibleFlash } from './readback.mjs?v=20261106d';
import { extractPinnedStockFirmware, TYPE_C_ZIP_URL } from './stock-source.mjs?v=20261106d';

const $ = id => document.getElementById(id);
let target = null, stockSeed = null, gameSeed = null, backup = null, device = null, hid = null, plan = null, restorePlan = null, busy = false;
let cancelBackup = false, backupRunning = false;
let firmwareSelection = 0, backupSelection = 0;
const resumeKey = 'h9-hid-pending-v1';
function status(message, error = false) {
  $('status').textContent = message;
  $('status').classList.toggle('error-status', error);
  const item = document.createElement('li'); item.textContent = message; $('log').prepend(item);
}
function progress(done, total) {
  const percent = Math.round(done / total * 100);
  $('progress').style.width = `${percent}%`; $('progressText').textContent = `${percent}%`;
}
function refresh() {
  $('connect').disabled = busy || !!device;
  $('disconnect').disabled = busy || !device;
  $('inspect').disabled = busy || !hid || !target || !backup;
  $('createBackup').disabled = busy || !hid || !target;
  $('backupHint').textContent = busy && backupRunning ? 'Reading the radio. Keep USB connected.'
    : !hid ? 'Connect the HID radio to enable backup.'
      : !target ? 'Select a firmware target in step 01 to enable backup.'
        : 'Ready to create a local HID recovery backup.';
  $('cancelBackup').hidden = !backupRunning;
  $('acknowledge').disabled = busy || (!plan?.length && !restorePlan?.length);
  $('flash').disabled = busy || !plan?.length || !$('acknowledge').checked;
  $('restore').disabled = busy || !restorePlan?.length || !$('acknowledge').checked;
  $('firmwareFile').disabled = busy;
  $('loadBrickBreaker').disabled = busy;
  $('loadStockGitHub').disabled = busy;
  $('backupFile').disabled = busy;
  $('flash').textContent = target?.kind === 'brick-breaker' ? 'Install Brick Breaker' : 'Write verified firmware';
}
function fail(error) { status(error?.message || String(error), true); }
function pending() { try { return JSON.parse(localStorage.getItem(resumeKey)); } catch { return null; } }
function forgetPlan() { plan = null; restorePlan = null; $('acknowledge').checked = false; $('planState').textContent = 'Inspect the radio before writing.'; refresh(); }
async function verifyLiveGamePlan(current, changed, resumeSector) {
  if (target.kind !== 'brick-breaker') return;
  if (resumeSector) {
    const index = (resumeSector - APP_START) / SECTOR_SIZE;
    const backupCrc = expectedCrcs(backup.image)[index];
    const gameCrc = expectedCrcs(target.image)[index];
    if (current[index] !== backupCrc && current[index] !== gameCrc) return;
  }
  const live = backup.image.slice();
  const targetCrcs = expectedCrcs(target.image);
  for (const physical of changed) {
    const index = (physical - APP_START) / SECTOR_SIZE;
    if (current[index] === targetCrcs[index])
      live.set(target.image.subarray(physical, physical + SECTOR_SIZE), physical);
  }
  if (!await knownAppMatches(hid, current, live))
    throw new Error('Live radio bytes differ from the exact stock/Brick Breaker sector plan. No writes allowed.');
}

async function validateSelectedBackup(announceWaiting = true) {
  const selection = ++backupSelection;
  backup = null; forgetPlan();
  const file = $('backupFile').files[0];
  if (!file) { $('backupState').textContent = 'No backup selected. You can create one after connecting.'; refresh(); return; }
  if (!target) {
    $('backupState').textContent = `${file.name} · waiting for firmware package`;
    if (announceWaiting) status('Backup selected. Choose a firmware target to validate it.');
    refresh(); return;
  }
  try {
    const bytes = new Uint8Array(await file.arrayBuffer());
    const loaded = file.name.toLowerCase().endsWith('.h9hid')
      ? await loadHidArchive(bytes, target.image)
      : await loadBackup(bytes, target.image);
    if (target.kind === 'brick-breaker') await verifyBrickBreakerBackup(loaded.image);
    if (selection !== backupSelection) return;
    backup = loaded;
    $('backupState').textContent = `${file.name} · ${backup.source === 'hid' ? 'HID archive verified' : 'boot region verified'}`;
    status(backup.source === 'hid' ? 'HID recovery archive checksum verified locally.' : 'Raw backup format and protected boot bytes verified locally.');
  } catch (error) {
    if (selection !== backupSelection) return;
    $('backupState').textContent = `${file.name} · backup rejected`;
    fail(error);
  }
  refresh();
}

$('firmwareFile').addEventListener('change', async event => {
  const selection = ++firmwareSelection;
  ++backupSelection; target = null; stockSeed = null; gameSeed = null; backup = null; forgetPlan();
  const file = event.target.files[0];
  if (!file) { $('firmwareState').textContent = 'No package selected'; await validateSelectedBackup(false); return; }
  try {
    const loaded = await loadStockPackage(new Uint8Array(await file.arrayBuffer()));
    if (selection !== firmwareSelection) return;
    target = loaded; $('firmwareState').textContent = `${file.name} · official v1.0.38 verified`;
    status('Official v1.0.38 firmware package verified locally.');
  } catch (error) {
    if (selection !== firmwareSelection) return;
    $('firmwareState').textContent = 'Package rejected'; fail(error);
  }
  await validateSelectedBackup(false);
});
$('loadBrickBreaker').addEventListener('click', async () => {
  const selection = ++firmwareSelection;
  ++backupSelection; target = null; stockSeed = null; gameSeed = null; backup = null; forgetPlan();
  busy = true; refresh();
  try {
    status('Downloading the pinned Brick Breaker package and stock recovery seed from H9 Toolbox.');
    const [packageResponse, seedResponse] = await Promise.all([
      fetch('./brick-breaker-v1.0.38.fw?v=208409d8', { cache: 'no-store' }),
      fetch('./brick-breaker-stock-sectors.bin?v=223c3b5c', { cache: 'no-store' }),
    ]);
    if (!packageResponse.ok || !seedResponse.ok) throw new Error('Could not download the Brick Breaker assets.');
    const loaded = await loadBrickBreakerPackage(new Uint8Array(await packageResponse.arrayBuffer()));
    const seed = await loadBrickBreakerStockSeed(new Uint8Array(await seedResponse.arrayBuffer()), loaded.image);
    if (selection !== firmwareSelection) return;
    target = loaded; stockSeed = seed; $('firmwareFile').value = '';
    $('firmwareState').textContent = 'Brick Breaker · exact hardware-tested .fw verified';
    status('Brick Breaker package and exact stock v1.0.38 recovery seed verified locally. No radio write sent.');
    await validateSelectedBackup(false);
  } catch (error) {
    if (selection === firmwareSelection) { $('firmwareState').textContent = 'Brick Breaker package rejected'; fail(error); }
  } finally { busy = false; refresh(); }
});
$('loadStockGitHub').addEventListener('click', async () => {
  const selection = ++firmwareSelection;
  ++backupSelection; target = null; stockSeed = null; gameSeed = null; backup = null; forgetPlan();
  busy = true; refresh();
  try {
    status('Downloading the pinned Type-C v1.0.38 ZIP from the TDH9 GitHub mirror.');
    const [zipResponse, gameResponse] = await Promise.all([
      fetch(TYPE_C_ZIP_URL, { cache: 'no-store' }),
      fetch('./brick-breaker-v1.0.38.fw?v=208409d8', { cache: 'no-store' }),
    ]);
    if (!zipResponse.ok || !gameResponse.ok) throw new Error('Could not download the stock or game firmware source.');
    const stockBytes = await extractPinnedStockFirmware(new Uint8Array(await zipResponse.arrayBuffer()));
    const loaded = await loadStockPackage(stockBytes);
    const game = await loadBrickBreakerPackage(new Uint8Array(await gameResponse.arrayBuffer()));
    if (selection !== firmwareSelection) return;
    target = loaded; gameSeed = game.image; $('firmwareFile').value = '';
    $('firmwareState').textContent = 'Stock v1.0.38 · pinned GitHub ZIP and extracted .fw verified';
    status('Stock Type-C v1.0.38 package verified locally. The game image is available as a fast backup seed. No radio write sent.');
    await validateSelectedBackup(false);
  } catch (error) {
    if (selection === firmwareSelection) { $('firmwareState').textContent = 'GitHub stock package rejected'; fail(error); }
  } finally { busy = false; refresh(); }
});
$('backupFile').addEventListener('change', () => validateSelectedBackup());
$('connect').addEventListener('click', async () => {
  if (!('hid' in navigator)) return fail(new Error('WebHID requires desktop Chrome or Edge on HTTPS.'));
  try {
    const devices = await navigator.hid.requestDevice({ filters: [{ vendorId: 0x4c4a, productId: 0x4155, usagePage: 0xff00, usage: 1 }] });
    if (devices.length !== 1) { if (devices.length) throw new Error('Choose exactly one JieLi vendor HID interface.'); return; }
    const chosen = devices[0];
    if (!chosen.collections.some(collection => collection.usagePage === 0xff00 && collection.usage === 1 && collection.inputReports.some(report => report.reportId === 0) && collection.outputReports.some(report => report.reportId === 0))) throw new Error('Unexpected HID report layout.');
    await chosen.open();
    const candidate = new H9Hid(chosen), geometry = await candidate.initialize();
    device = chosen; hid = candidate;
    $('deviceState').textContent = 'Connected';
    status(`HID radio identified: app 0x${geometry.address.toString(16)}, 4 KiB sectors. No write sent.`);
  } catch (error) { fail(error); }
  refresh();
});
$('disconnect').addEventListener('click', async () => {
  await device?.close(); device = null; hid = null; forgetPlan();
  $('deviceState').textContent = 'Disconnected'; status('Disconnected.'); refresh();
});
$('acknowledge').addEventListener('change', refresh);
$('cancelBackup').addEventListener('click', () => { cancelBackup = true; $('cancelBackup').disabled = true; status('Stopping after the current HID reply…'); });
$('createBackup').addEventListener('click', async () => {
  if (!hid || !target || busy) return;
  busy = true; backupRunning = true; cancelBackup = false; $('cancelBackup').disabled = false; forgetPlan(); progress(0, 1); refresh();
  try {
    status('Scanning accessible flash sector CRCs. No erase or write will be sent.');
    const current = await scanRadio(hid, progress);
    const source = target.kind === 'brick-breaker'
      ? classifyBrickBreakerRadio(current, stockSeed, target.image)
      : gameSeed ? classifyBrickBreakerRadio(current, target.image, gameSeed) : 'other';
    status('Recovering accessible flash bytes using shifted CRC windows. This may take several minutes; keep USB connected.');
    const knownImages = stockSeed ? [stockSeed, target.image] : gameSeed ? [target.image, gameSeed] : target.image;
    const recovered = await readAccessibleFlash(hid, current, knownImages, progress, () => cancelBackup);
    if (cancelBackup) throw new Error('HID backup cancelled. No radio write was sent.');
    const archive = await createHidArchive(recovered.accessible, recovered.method);
    const loaded = await loadHidArchive(archive, target.image);
    if (source === 'stock') await verifyBrickBreakerBackup(loaded.image);
    if (source === 'brick-breaker' && await sha256(loaded.image) !== BRICK_BREAKER_IMAGE_SHA256)
      throw new Error('Recovered Brick Breaker image differs from the pinned build. No backup accepted.');
    const date = new Date().toISOString().slice(0, 10);
    const name = `TD-H9-${source === 'brick-breaker' ? 'Brick-Breaker-' : ''}HID-recovery-${date}.h9hid`;
    const url = URL.createObjectURL(new Blob([archive], { type: 'application/octet-stream' }));
    const link = document.createElement('a'); link.href = url; link.download = name; document.body.append(link);
    link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 60000);
    if (source === 'brick-breaker' && target.kind === 'brick-breaker') {
      if (!backup) $('backupState').textContent = `${name} · game backup download started; select a stock backup to restore`;
      status(`Current Brick Breaker backup download started. ${backup ? 'The selected stock backup is still available for Restore.' : 'Select a prior stock backup before Restore.'} Confirm the download is saved.`);
    } else {
      backup = loaded;
      $('backupState').textContent = `${name} · download started · ${recovered.method === 'verified-known-image-seed' ? 'verified image seed' : 'erased sector seed'}`;
      status(`${source === 'brick-breaker' ? 'Current Brick Breaker' : 'HID recovery'} archive built and download started. Confirm it is saved before writing. Protected boot bytes are not in this archive.`);
    }
    progress(1, 1);
  } catch (error) { fail(error); }
  busy = false; backupRunning = false; cancelBackup = false; refresh();
});
$('inspect').addEventListener('click', async () => {
  busy = true; forgetPlan(); refresh();
  try {
    status('Scanning 251 radio sectors (CRC only)…');
    const current = await scanRadio(hid, progress);
    const marker = pending();
    const resumeSector = marker?.packageHash === target.packageHash && marker?.backupHash === backup.hash ? marker.sector : null;
    const changed = planUpdate(current, backup.image, target.image, resumeSector);
    if (target.kind === 'brick-breaker' && (changed.length !== BRICK_BREAKER_SECTORS.length ||
      !BRICK_BREAKER_SECTORS.every(physical => changed.includes(physical))))
      throw new Error('Brick Breaker sector plan differs from the five tested sectors. No writes allowed.');
    await verifyLiveGamePlan(current, changed, resumeSector);
    const backupCrcs = expectedCrcs(backup.image), targetCrcs = expectedCrcs(target.image);
    const unknown = changed.filter(physical => {
      const index = (physical - APP_START) / SECTOR_SIZE;
      return current[index] !== backupCrcs[index] && current[index] !== targetCrcs[index];
    });
    if (unknown.length && (unknown.length !== 1 || unknown[0] !== resumeSector)) throw new Error('Unknown sector state outside the saved interrupted update. No writes allowed.');
    const remaining = changed.filter(physical => current[(physical - APP_START) / SECTOR_SIZE] !== targetCrcs[(physical - APP_START) / SECTOR_SIZE]);
    plan = remaining;
    restorePlan = changed.filter(physical => current[(physical - APP_START) / SECTOR_SIZE] !== backupCrcs[(physical - APP_START) / SECTOR_SIZE]);
    $('planState').textContent = `${plan.length} sector(s) to update; ${restorePlan.length} to restore${unknown.length ? ' (one interrupted sector)' : ''}.`;
    status(`Inspection complete. ${plan.length} sector(s) to update, ${restorePlan.length} to restore; no writes sent.`);
  } catch (error) { fail(error); }
  busy = false; refresh();
});
async function performWrite(direction) {
  const work = [...((direction === 'update' ? plan : restorePlan) || [])];
  if (!work.length || !backup || !target || !hid || !$('acknowledge').checked) return;
  const destination = direction === 'update' ? target.image : backup.image;
  busy = true; refresh();
  try {
    const fresh = await scanRadio(hid);
    const marker = pending();
    const resumeSector = marker?.packageHash === target.packageHash && marker?.backupHash === backup.hash ? marker.sector : null;
    const changed = planUpdate(fresh, backup.image, target.image, resumeSector);
    await verifyLiveGamePlan(fresh, changed, resumeSector);
    const destinationCrcs = expectedCrcs(destination);
    const freshWork = changed.filter(physical => fresh[(physical - APP_START) / SECTOR_SIZE] !== destinationCrcs[(physical - APP_START) / SECTOR_SIZE]);
    if (freshWork.length !== work.length || freshWork.some((physical, index) => physical !== work[index]))
      throw new Error('Radio sector state changed since inspection. Inspect again before writing.');
    status(`${direction === 'update' ? 'Updating' : 'Restoring'} ${work.length} sectors. Keep USB connected and the radio powered on.`);
    for (let index = 0; index < work.length; index++) {
      const physical = work[index];
      localStorage.setItem(resumeKey, JSON.stringify({ packageHash: target.packageHash, backupHash: backup.hash, sector: physical, direction }));
      await writeSector(hid, physical, destination);
      localStorage.removeItem(resumeKey);
      progress(index + 1, work.length);
      if (index % 8 === 7 || index === work.length - 1) status(`Verified ${index + 1}/${work.length} sectors.`);
    }
    const after = await scanRadio(hid, progress), desired = expectedCrcs(destination);
    for (let physical = APP_START; physical < IMAGE_SIZE; physical += SECTOR_SIZE) {
      const index = (physical - APP_START) / SECTOR_SIZE;
      if (after[index] !== desired[index]) throw new Error(`Final CRC mismatch at 0x${physical.toString(16)}.`);
    }
    plan = null; restorePlan = null; $('acknowledge').checked = false;
    $('planState').textContent = 'All main-firmware sectors verified.';
    status(`${direction === 'update' ? 'Updated firmware' : 'Restored backup'} sector CRC verification passed. Disconnect USB and power-cycle the radio, then check its version and saved settings.`);
  } catch (error) { fail(error); status('Leave the backup and firmware files available. Reconnect in PTT + 2 mode and run Inspect to resume; do not discard the files.', true); }
  busy = false; refresh();
}
$('flash').addEventListener('click', () => performWrite('update'));
$('restore').addEventListener('click', () => performWrite('restore'));

if (!('hid' in navigator)) status('WebHID is unavailable here. Use desktop Chrome or Edge over HTTPS.', true);
refresh();
