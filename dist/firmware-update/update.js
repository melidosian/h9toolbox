import { APP_START, IMAGE_SIZE, SECTOR_SIZE, BUILD, PREVIOUS, classifyRadio, crc16, expectedCrcs, H9Hid, loadBackup, loadBuildPackage, loadPreviousImage, loadStockPackage, loadStockSeed, planUpdate, scanRadio, sha256, verifyStockBackup, writeSector } from './firmware.mjs?v=20261108c';
import { createHidArchive, knownAppMatches, loadHidArchive, readAccessibleFlash } from './readback.mjs?v=20261108c';
import { extractPinnedStockFirmware, TYPE_C_ZIP_URL } from './stock-source.mjs?v=20261108c';
import { mountLogoTool } from '../boot-logo/logo-tool.mjs?v=20261108d';

const $ = id => document.getElementById(id);
// target: the image to install (kind 'build' or 'stock'). stockSeed: exact
// stock rebuilt from the build. buildImage / previousImage: the hosted build
// and the original Brick Breaker, known so a radio running either is
// recognised, backed up quickly, and (for the original) upgraded directly.
// createdBackup: the archive made on this page ({ archive, name, source }),
// kept across firmware selection and used when no backup file is chosen.
let createdBackup = null;
// True once the connected radio has been backed up on this page. Install
// stays disabled until then; connecting a radio resets it.
let radioBackedUp = false;
let target = null, stockSeed = null, buildImage = null, previousImage = null, backup = null, device = null, hid = null, busy = false;
let cancelBackup = false, backupRunning = false;
let firmwareSelection = 0, backupSelection = 0;
const resumeKey = 'h9-hid-pending-v1';
function status(message, error = false) {
  $('status').textContent = message;
  $('status').classList.toggle('error-status', error);
  const item = document.createElement('li'); item.textContent = message; $('log').prepend(item);
}
// Each step (CRC scan, backup recovery, writing, final check) names itself
// with phase(); progress() adds the percentage and, once the step has run a
// few seconds, the time left at the rate measured so far.
let timing = { label: '', started: 0, total: 0 };
function phase(label) { timing = { label, started: performance.now(), total: 0 }; }
function formatDuration(seconds) {
  const whole = Math.max(1, Math.ceil(seconds));
  if (whole < 90) return `${whole} s`;
  return `${Math.floor(whole / 60)} min ${String(whole % 60).padStart(2, '0')} s`;
}
function finish(text) { $('progress').style.width = '100%'; $('progressText').textContent = `100% · ${text}`; }
function progress(done, total) {
  if (total !== timing.total) timing = { ...timing, started: timing.total ? performance.now() : timing.started || performance.now(), total };
  const percent = Math.round(done / total * 100);
  const elapsed = (performance.now() - timing.started) / 1000;
  const parts = [`${percent}%`];
  if (timing.label) parts.push(typeof timing.label === 'function' ? timing.label(done, total) : timing.label);
  if (done >= total) parts.push(`done in ${formatDuration(elapsed)}`);
  else if (done > 0 && elapsed >= 3) parts.push(`about ${formatDuration(elapsed * (total - done) / done)} left`);
  $('progress').style.width = `${percent}%`; $('progressText').textContent = parts.join(' · ');
}
let shownSelection = null;
function refresh() {
  $('connect').disabled = busy || !!device;
  $('disconnect').disabled = busy || !device;
  $('createBackup').disabled = busy || !hid;
  $('cancelBackup').hidden = !backupRunning;
  const ready = Boolean(hid && target && backup);
  $('acknowledge').disabled = busy || !ready;
  $('flash').disabled = busy || !ready || !radioBackedUp || !$('acknowledge').checked;
  $('restore').disabled = busy || !ready || !$('acknowledge').checked;
  $('restore').textContent = backup?.source === 'hosted-stock' ? 'Restore stock v1.0.38' : 'Restore backup';
  // The next action gets the colour: Backup until it succeeds, then Install.
  const installReady = radioBackedUp && !$('flash').disabled;
  $('createBackup').classList.toggle('primary-button', !radioBackedUp);
  $('createBackup').classList.toggle('ghost', radioBackedUp);
  $('flash').classList.toggle('primary-button', installReady);
  $('flash').classList.toggle('ghost', !installReady);
  if (busy && backupRunning) $('planState').textContent = 'Backing up the radio. Keep USB connected.';
  else if (!busy) $('planState').textContent = !hid ? 'Connect the radio in step 01.'
    : !radioBackedUp ? 'Click Backup radio. Install unlocks when the backup succeeds.'
      : !target ? 'Backup saved. Select firmware in step 02.'
        : !backup ? 'Backup saved. This radio’s firmware is not recognised, so select a stock backup as the restore point in step 02.'
          : !$('acknowledge').checked ? 'Backup saved. Tick the box above to enable Install.'
            : 'Ready. Install checks the radio again before anything is written.';
  // Highlight the chosen firmware; the animation plays when the choice changes.
  const selected = target?.kind ?? null;
  if (selected !== shownSelection) {
    shownSelection = selected;
    $('optBuild').classList.toggle('selected', selected === 'build');
    $('optStock').classList.toggle('selected', selected === 'stock');
    $('optBuild').closest('.file-grid').classList.toggle('has-selection', Boolean(selected));
    $('firmwareState').classList.remove('chosen');
    if (selected) { void $('firmwareState').offsetWidth; $('firmwareState').classList.add('chosen'); }
  }
  $('firmwareFile').disabled = busy;
  $('loadBuild').disabled = busy;
  $('loadStockGitHub').disabled = busy;
  $('backupFile').disabled = busy;
  $('flash').textContent = target?.kind === 'build' ? `Install ${BUILD.label}` : target?.kind === 'stock' ? 'Install stock v1.0.38' : 'Install firmware';
}
function fail(error) { status(error?.message || String(error), true); }
function pending() { try { return JSON.parse(localStorage.getItem(resumeKey)); } catch { return null; } }
function forgetPlan() { $('acknowledge').checked = false; refresh(); }
// Step 04 (boot logo) needs H9 Toolbox Firmware: open it when the page sees
// that build on the radio, keep it closed (with a note) when it sees another.
// Unknown radios leave it as it is; the user can always open it.
function setLogoStep(kind) {
  const gate = $('logoGate');
  if (kind === 'build') {
    gate.textContent = `This radio runs ${BUILD.label}, so the boot logo can be changed.`;
    gate.classList.add('ready');
    $('logoStep').open = true;
  } else if (kind === 'stock' || kind === 'previous') {
    gate.textContent = `This radio runs ${kind === 'stock' ? 'stock v1.0.38' : `the ${PREVIOUS.label}`}. Install ${BUILD.label} above first; the boot logo can be changed after that.`;
    gate.classList.remove('ready');
    $('logoStep').open = false;
  }
}
// Which known image the radio holds, from a full sector-CRC scan, or null.
function knownRadio(current) {
  const images = [['stock', stockSeed ?? target?.image], ['build', buildImage], ['previous', previousImage]].filter(([, image]) => image);
  try { return images.length > 1 ? classifyRadio(current, images) : null; } catch { return null; }
}
// The outcome of an install or restore, shown large under the progress bar.
function showResult(result) {
  const panel = $('writeResult');
  panel.hidden = !result;
  if (!result) return;
  panel.classList.toggle('failed', result.failed);
  panel.replaceChildren();
  const title = document.createElement('strong'); title.textContent = result.title;
  const body = document.createElement('div');
  const lead = document.createElement('p'); lead.textContent = result.text; body.append(lead);
  if (result.steps) {
    const list = document.createElement('ol');
    for (const step of result.steps) { const item = document.createElement('li'); item.textContent = step; list.append(item); }
    body.append(list);
  }
  panel.append(title, body);
  if (result.action) {
    const link = document.createElement('a'); link.className = 'button primary-button'; link.href = result.action.href; link.textContent = result.action.label;
    panel.append(link);
  }
  panel.scrollIntoView({ behavior: 'smooth', block: 'center' });
}
// Stock, H9 Toolbox Firmware, and the original Brick Breaker images, from
// the hosted files if no firmware has been selected yet. A backup needs them
// to recognise the radio, not a chosen target.
async function ensureReferences() {
  if (buildImage && previousImage && (stockSeed || target?.kind === 'stock')) return;
  const hosted = await loadHostedBuild();
  buildImage = hosted.build.image; previousImage = hosted.previous; stockSeed ??= hosted.stock;
}
function knownImages() {
  return target?.kind === 'build' && previousImage ? [previousImage] : [];
}
// Fetch the hosted build, its stock seed, and the original Brick Breaker's
// sectors; every file is checked by SHA-256.
async function loadHostedBuild() {
  const [packageResponse, seedResponse, previousResponse] = await Promise.all([
    fetch(`./${BUILD.packageFile}?v=${BUILD.packageSha256.slice(0, 8)}`, { cache: 'no-store' }),
    fetch(`./${BUILD.stockSeedFile}?v=${BUILD.stockSeedSha256.slice(0, 8)}`, { cache: 'no-store' }),
    fetch(`./${PREVIOUS.seedFile}?v=${PREVIOUS.seedSha256.slice(0, 8)}`, { cache: 'no-store' }),
  ]);
  if (!packageResponse.ok || !seedResponse.ok || !previousResponse.ok) throw new Error(`Could not download the ${BUILD.label} files.`);
  const build = await loadBuildPackage(new Uint8Array(await packageResponse.arrayBuffer()));
  const stock = await loadStockSeed(new Uint8Array(await seedResponse.arrayBuffer()), build.image);
  const previous = await loadPreviousImage(new Uint8Array(await previousResponse.arrayBuffer()), build.image);
  return { build, stock, previous };
}
async function verifyLiveGamePlan(current, changed, resumeSector) {
  if (target.kind !== 'build') return;
  if (resumeSector) {
    const index = (resumeSector - APP_START) / SECTOR_SIZE;
    const backupCrc = expectedCrcs(backup.image)[index];
    const gameCrc = expectedCrcs(target.image)[index];
    if (current[index] !== backupCrc && current[index] !== gameCrc) return;
  }
  const live = backup.image.slice();
  const targetCrcs = expectedCrcs(target.image);
  const previousCrcs = previousImage ? expectedCrcs(previousImage) : [];
  for (const physical of changed) {
    const index = (physical - APP_START) / SECTOR_SIZE;
    if (current[index] === targetCrcs[index])
      live.set(target.image.subarray(physical, physical + SECTOR_SIZE), physical);
    else if (previousImage && current[index] === previousCrcs[index])
      live.set(previousImage.subarray(physical, physical + SECTOR_SIZE), physical);
  }
  if (!await knownAppMatches(hid, current, live))
    throw new Error(`Live radio bytes differ from exact stock, ${BUILD.label}, and the ${PREVIOUS.label}. No writes allowed.`);
}

async function validateSelectedBackup(announceWaiting = true) {
  const selection = ++backupSelection;
  backup = null; forgetPlan();
  const file = $('backupFile').files[0];
  const name = file?.name ?? createdBackup?.name;
  if (!name) { $('backupState').textContent = 'No backup selected. You can create one after connecting.'; refresh(); return; }
  if (!target) {
    $('backupState').textContent = `${name} · ${file ? 'checked when you choose firmware' : 'saved; checked when you choose firmware'}`;
    if (announceWaiting && file) status('Backup selected. Choose firmware to check it against.');
    refresh(); return;
  }
  if (!file && createdBackup.source !== 'stock' && target.kind === 'build') {
    if (createdBackup.source === 'build' || createdBackup.source === 'previous') {
      // The radio was verified (sector and shifted-window CRCs) to run a known
      // modified image, so its stock state is exact official v1.0.38: rebuild
      // it from the hosted, hash-checked files and use it as the restore point.
      // This is what lets a radio already running a Brick Breaker build
      // reinstall or update without a separate stock backup file.
      try {
        await ensureReferences();
        const image = stockSeed;
        const hash = await sha256(image);
        if (selection !== backupSelection) return;
        backup = { image, hash, source: 'hosted-stock' };
        $('backupState').textContent = `${name} saved · restore point: official stock v1.0.38, rebuilt from verified files`;
        status('Restore point set to official stock v1.0.38, rebuilt from the verified hosted files (this radio runs a known H9 Toolbox image).');
      } catch (error) {
        if (selection !== backupSelection) return;
        $('backupState').textContent = `${name} saved · could not load the stock restore point`;
        fail(error);
      }
    } else {
      // Unrecognised firmware: only the user's own stock backup can be the restore point.
      $('backupState').textContent = `${name} · saved; select a stock backup to use as the restore point`;
    }
    refresh(); return;
  }
  try {
    const loaded = !file
      ? await loadHidArchive(createdBackup.archive, target.image)
      : file.name.toLowerCase().endsWith('.h9hid')
        ? await loadHidArchive(new Uint8Array(await file.arrayBuffer()), target.image)
        : await loadBackup(new Uint8Array(await file.arrayBuffer()), target.image);
    if (target.kind === 'build') await verifyStockBackup(loaded.image);
    if (selection !== backupSelection) return;
    backup = loaded;
    $('backupState').textContent = `${name} · ${backup.source === 'hid' ? 'HID archive verified' : 'boot region verified'}`;
    status(backup.source === 'hid' ? 'HID recovery archive checksum verified locally.' : 'Raw backup format and protected boot bytes verified locally.');
  } catch (error) {
    if (selection !== backupSelection) return;
    $('backupState').textContent = `${name} · backup rejected`;
    fail(error);
  }
  refresh();
}

$('firmwareFile').addEventListener('change', async event => {
  const selection = ++firmwareSelection;
  ++backupSelection; target = null; stockSeed = null; buildImage = null; previousImage = null; backup = null; forgetPlan();
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
$('loadBuild').addEventListener('click', async () => {
  const selection = ++firmwareSelection;
  ++backupSelection; target = null; stockSeed = null; buildImage = null; previousImage = null; backup = null; forgetPlan();
  busy = true; refresh();
  try {
    status(`Downloading the pinned ${BUILD.label} package and recovery seeds from H9 Toolbox.`);
    const hosted = await loadHostedBuild();
    if (selection !== firmwareSelection) return;
    target = hosted.build; stockSeed = hosted.stock; buildImage = hosted.build.image; previousImage = hosted.previous;
    $('firmwareFile').value = '';
    $('firmwareState').textContent = `${BUILD.label} · exact .fw verified`;
    status(`${BUILD.label} package, exact stock v1.0.38 seed, and ${PREVIOUS.label} sectors verified locally. No radio write sent.`);
    await validateSelectedBackup(false);
  } catch (error) {
    if (selection === firmwareSelection) { $('firmwareState').textContent = `${BUILD.label} package rejected`; fail(error); }
  } finally { busy = false; refresh(); }
});
$('loadStockGitHub').addEventListener('click', async () => {
  const selection = ++firmwareSelection;
  ++backupSelection; target = null; stockSeed = null; buildImage = null; previousImage = null; backup = null; forgetPlan();
  busy = true; refresh();
  try {
    status('Downloading the pinned Type-C v1.0.38 ZIP from the TDH9 GitHub mirror.');
    const [zipResponse, hosted] = await Promise.all([fetch(TYPE_C_ZIP_URL, { cache: 'no-store' }), loadHostedBuild()]);
    if (!zipResponse.ok) throw new Error('Could not download the stock firmware source.');
    const stockBytes = await extractPinnedStockFirmware(new Uint8Array(await zipResponse.arrayBuffer()));
    const loaded = await loadStockPackage(stockBytes);
    if (selection !== firmwareSelection) return;
    target = loaded; buildImage = hosted.build.image; previousImage = hosted.previous; $('firmwareFile').value = '';
    $('firmwareState').textContent = 'Stock v1.0.38 · pinned GitHub ZIP and extracted .fw verified';
    status('Stock Type-C v1.0.38 package verified locally. The hosted builds are available as fast backup seeds. No radio write sent.');
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
    device = chosen; hid = candidate; radioBackedUp = false;
    $('deviceState').textContent = 'Connected';
    status(`Radio connected (app 0x${geometry.address.toString(16)}, 4 KiB sectors). Back it up next. No write sent.`);
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
  if (!hid || busy) return;
  busy = true; backupRunning = true; cancelBackup = false; $('cancelBackup').disabled = false; forgetPlan(); phase('Starting backup'); progress(0, 1); refresh();
  const backupBegan = performance.now();
  try {
    status('Scanning accessible flash sector CRCs. No erase or write will be sent.');
    phase('Scanning sector CRCs');
    const current = await scanRadio(hid, progress);
    await ensureReferences();
    const images = [['stock', stockSeed ?? target?.image], ['build', buildImage], ['previous', previousImage]].filter(([, image]) => image);
    const source = images.length > 1 ? classifyRadio(current, images) : 'other';
    setLogoStep(source);
    status('Recovering accessible flash bytes using shifted CRC windows. This may take several minutes; keep USB connected.');
    phase('Recovering flash bytes from shifted CRC windows');
    // Firmware area only: the settings area above 0xDA000 is never written
    // here and takes about 10 minutes to recover. A radio with unknown
    // firmware still gets the full recovery (there is no shortcut for it).
    const recovered = await readAccessibleFlash(hid, current, images.map(([, image]) => image), progress, () => cancelBackup, { settings: false });
    if (!recovered.settings) status('The radio matches a known firmware image, so the backup was quick.');
    if (cancelBackup) throw new Error('HID backup cancelled. No radio write was sent.');
    const archive = await createHidArchive(recovered.accessible, recovered.method);
    // The protected boot bytes are a placeholder from a known image (they are
    // identical in all of them) and are never written through HID.
    const loaded = await loadHidArchive(archive, target?.image ?? stockSeed);
    if (source === 'stock') await verifyStockBackup(loaded.image);
    const expected = { build: BUILD.imageSha256, previous: PREVIOUS.imageSha256 }[source];
    if (expected && await sha256(loaded.image) !== expected)
      throw new Error('Recovered image differs from the pinned build it matched. No backup accepted.');
    const sourceName = { build: 'H9-Toolbox-Firmware-', previous: 'Brick-Breaker-' }[source] ?? '';
    const sourceLabel = { build: BUILD.label, previous: PREVIOUS.label }[source];
    const date = new Date().toISOString().slice(0, 10);
    const name = `TD-H9-${sourceName}HID-recovery-${date}.h9hid`;
    const url = URL.createObjectURL(new Blob([archive], { type: 'application/octet-stream' }));
    const link = document.createElement('a'); link.href = url; link.download = name; document.body.append(link);
    link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 60000);
    createdBackup = { archive, name, source };
    radioBackedUp = true;
    status(`${sourceLabel ? `Backup of the installed ${sourceLabel}` : 'Backup'} download started (${recovered.method === 'verified-known-image-seed' ? 'matched a known image' : 'full recovery'}). Keep that file. Protected boot bytes are not in it.`);
    // Choose the restore point the same way as for a selected file.
    await validateSelectedBackup(false);
    finish(`Backup built and verified in ${formatDuration((performance.now() - backupBegan) / 1000)}`);
  } catch (error) { fail(error); }
  busy = false; backupRunning = false; cancelBackup = false; refresh();
});
// Check the radio and plan both directions: a full sector-CRC scan, the
// pinned sector set, the live shifted-window check, and any interrupted
// sector. Nothing is erased or written here.
async function inspectRadio() {
  status('Checking the radio: scanning 251 sectors (CRC only, no writes)…');
  phase('Checking the radio (CRC scan, no writes)');
  const current = await scanRadio(hid, progress);
  setLogoStep(knownRadio(current));
  const marker = pending();
  const resumeSector = marker?.packageHash === target.packageHash && marker?.backupHash === backup.hash ? marker.sector : null;
  const changed = planUpdate(current, backup.image, target.image, resumeSector, knownImages());
  if (target.kind === 'build' && (changed.length !== BUILD.sectors.length ||
    !BUILD.sectors.every(physical => changed.includes(physical))))
    throw new Error(`${BUILD.label} sector plan differs from its ${BUILD.sectors.length} pinned sectors. No writes allowed.`);
  phase('Checking the radio (shifted CRC windows)');
  await verifyLiveGamePlan(current, changed, resumeSector);
  const backupCrcs = expectedCrcs(backup.image), targetCrcs = expectedCrcs(target.image);
  const knownCrcs = knownImages().map(expectedCrcs);
  const unknown = changed.filter(physical => {
    const index = (physical - APP_START) / SECTOR_SIZE;
    return current[index] !== backupCrcs[index] && current[index] !== targetCrcs[index] &&
      !knownCrcs.some(crcs => crcs[index] === current[index]);
  });
  if (unknown.length && (unknown.length !== 1 || unknown[0] !== resumeSector)) throw new Error('Unknown sector state outside the saved interrupted update. No writes allowed.');
  const index = physical => (physical - APP_START) / SECTOR_SIZE;
  return {
    update: changed.filter(physical => current[index(physical)] !== targetCrcs[index(physical)]),
    restore: changed.filter(physical => current[index(physical)] !== backupCrcs[index(physical)]),
    interrupted: unknown.length > 0,
  };
}
async function performWrite(direction) {
  if (!backup || !target || !hid || !$('acknowledge').checked) return;
  const destination = direction === 'update' ? target.image : backup.image;
  const began = performance.now();
  showResult(null);
  busy = true; refresh();
  try {
    const checked = await inspectRadio();
    const work = checked[direction];
    $('planState').textContent = `Radio checked: ${work.length} sector(s) to ${direction === 'update' ? 'write' : 'restore'}${checked.interrupted ? ', including one interrupted sector' : ''}.`;
    if (!work.length) {
      finish('Radio checked; nothing to write');
      showResult(direction === 'update'
        ? { title: target.kind === 'build' ? `${BUILD.label} is already installed` : 'This firmware is already installed', text: 'Every firmware sector on the radio already matches this package exactly, so nothing needed rewriting. Your radio is up to date.' }
        : { title: backup.source === 'hosted-stock' ? 'The radio already runs stock v1.0.38' : 'The radio already matches your backup', text: 'Every firmware sector on the radio already matches, so nothing was written.' });
      status('Radio checked: it already matches; nothing was written.');
      return;
    }
    status(`Radio checked. ${direction === 'update' ? 'Writing' : 'Restoring'} ${work.length} sectors. Keep USB connected and the radio powered on.`);
    phase((done, total) => done >= total ? `${total} sectors written and verified` : `Writing and verifying sector ${done + 1} of ${total}`);
    progress(0, work.length);
    for (let index = 0; index < work.length; index++) {
      const physical = work[index];
      localStorage.setItem(resumeKey, JSON.stringify({ packageHash: target.packageHash, backupHash: backup.hash, sector: physical, direction }));
      await writeSector(hid, physical, destination);
      localStorage.removeItem(resumeKey);
      progress(index + 1, work.length);
      if (index % 8 === 7 || index === work.length - 1) status(`Verified ${index + 1}/${work.length} sectors.`);
    }
    phase('Final CRC check of every sector');
    const after = await scanRadio(hid, progress), desired = expectedCrcs(destination);
    for (let physical = APP_START; physical < IMAGE_SIZE; physical += SECTOR_SIZE) {
      const index = (physical - APP_START) / SECTOR_SIZE;
      if (after[index] !== desired[index]) throw new Error(`Final CRC mismatch at 0x${physical.toString(16)}.`);
    }
    $('acknowledge').checked = false;
    const seconds = formatDuration((performance.now() - began) / 1000);
    finish(`${work.length} sectors ${direction === 'update' ? 'installed' : 'restored'} and every sector CRC verified in ${seconds}`);
    setLogoStep(knownRadio(after));
    showResult(direction === 'update' && target.kind === 'build'
      ? { title: `${BUILD.label} installed and verified`,
          text: `All ${work.length} changed sectors were written and every firmware sector's CRC matches the package (${seconds}).`,
          steps: ['Disconnect USB and power-cycle the radio.', 'You should see your boot logo, then the normal channel screen.', 'Hold Back to start Brick Breaker.', 'To change the startup image, continue to step 04.'],
          action: { href: '#logo-title', label: 'Continue to step 04: boot logo' } }
      : { title: direction === 'update' ? 'Firmware written and verified' : backup.source === 'hosted-stock' ? 'Stock v1.0.38 restored and verified' : 'Backup restored and verified',
          text: `All ${work.length} changed sectors were written and every firmware sector's CRC matches ${direction === 'update' ? 'the package' : backup.source === 'hosted-stock' ? 'official stock v1.0.38' : 'your backup'} (${seconds}).`,
          steps: ['Disconnect USB and power-cycle the radio.', 'Check the version and your saved channels and settings.', 'Your boot logo is kept: it lives on the separate resource flash.'] });
    status(`${direction === 'update' ? 'Updated firmware' : 'Restored backup'} sector CRC verification passed. Disconnect USB and power-cycle the radio, then check its version and saved settings.`);
  } catch (error) {
    fail(error);
    showResult({ failed: true, title: 'Write stopped before it finished',
      text: error?.message || String(error),
      steps: ['Do not discard the selected firmware or backup files.', 'Reconnect in PTT + 2 HID mode and click the same button again.', 'The page checks the radio and resumes from the interrupted sector; Restore returns to your backup.'] });
  } finally {
    busy = false; refresh();
  }
}
$('flash').addEventListener('click', () => performWrite('update'));
$('restore').addEventListener('click', () => performWrite('restore'));

mountLogoTool($('logoTool'), { onStatus: status });
if (!('hid' in navigator)) status('WebHID is unavailable here. Use desktop Chrome or Edge over HTTPS.', true);
refresh();
