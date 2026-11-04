import { APP_START, IMAGE_SIZE, SECTOR_SIZE, crc16, expectedCrcs, H9Hid, loadBackup, loadStockPackage, planUpdate, scanRadio, writeSector } from './firmware.mjs?v=20261104b';
import { createHidArchive, loadHidArchive, readAccessibleFlash } from './readback.mjs?v=20261104b';

const $ = id => document.getElementById(id);
let target = null, backup = null, device = null, hid = null, plan = null, restorePlan = null, busy = false;
let cancelBackup = false, backupRunning = false;
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
      : !target ? 'Select TD-H9-V1.0.38.fw from the Type-C download in step 01 to enable backup.'
        : 'Ready to create a local HID recovery backup.';
  $('cancelBackup').hidden = !backupRunning;
  $('acknowledge').disabled = busy || (!plan?.length && !restorePlan?.length);
  $('flash').disabled = busy || !plan?.length || !$('acknowledge').checked;
  $('restore').disabled = busy || !restorePlan?.length || !$('acknowledge').checked;
  $('firmwareFile').disabled = busy;
  $('backupFile').disabled = busy;
}
function fail(error) { status(error?.message || String(error), true); }
function pending() { try { return JSON.parse(localStorage.getItem(resumeKey)); } catch { return null; } }
function forgetPlan() { plan = null; restorePlan = null; $('acknowledge').checked = false; $('planState').textContent = 'Inspect the radio before writing.'; refresh(); }

$('firmwareFile').addEventListener('change', async event => {
  target = null; forgetPlan();
  const file = event.target.files[0]; if (!file) return;
  try {
    const loaded = await loadStockPackage(new Uint8Array(await file.arrayBuffer()));
    target = loaded; $('firmwareState').textContent = `${file.name} · official v1.0.38 verified`;
    status('Official v1.0.38 firmware package verified locally.');
  } catch (error) { $('firmwareState').textContent = 'Package rejected'; fail(error); }
  refresh();
});
$('backupFile').addEventListener('change', async event => {
  backup = null; forgetPlan();
  const file = event.target.files[0]; if (!file) return;
  try {
    if (!target) throw new Error('Select the official firmware package first.');
    const bytes = new Uint8Array(await file.arrayBuffer());
    backup = file.name.toLowerCase().endsWith('.h9hid')
      ? await loadHidArchive(bytes, target.image)
      : await loadBackup(bytes, target.image);
    $('backupState').textContent = `${file.name} · ${backup.source === 'hid' ? 'HID archive verified' : 'boot region verified'}`;
    status(backup.source === 'hid' ? 'HID recovery archive checksum verified locally.' : 'Raw backup format and protected boot bytes verified locally.');
  } catch (error) { $('backupState').textContent = 'Backup rejected'; fail(error); }
  refresh();
});
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
    status('Recovering accessible flash bytes using shifted CRC windows. This may take several minutes; keep USB connected.');
    const recovered = await readAccessibleFlash(hid, current, target.image, progress, () => cancelBackup);
    if (cancelBackup) throw new Error('HID backup cancelled. No radio write was sent.');
    const archive = await createHidArchive(recovered.accessible, recovered.method);
    const loaded = await loadHidArchive(archive, target.image);
    const date = new Date().toISOString().slice(0, 10);
    const name = `TD-H9-HID-recovery-${date}.h9hid`;
    const url = URL.createObjectURL(new Blob([archive], { type: 'application/octet-stream' }));
    const link = document.createElement('a'); link.href = url; link.download = name; document.body.append(link);
    link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 60000);
    backup = loaded;
    $('backupState').textContent = `${name} · download started · ${recovered.method === 'verified-official-image-seed' ? 'official image seed' : 'erased sector seed'}`;
    status('HID recovery archive built and download started. Confirm it is saved before writing. Protected boot bytes are not in this archive.');
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
