import { APP_START, IMAGE_SIZE, SECTOR_SIZE, RESIDENT_START, RESIDENT_END, BUILD, PREVIOUS, classifyRadio, crc16, decodePackage, expectedCrcs, H9Hid, loadBackup, loadBuildPackage, loadPreviousImage, loadStockPackage, loadStockSeed, planUpdate, scanRadio, sha256, verifyStockBackup, verifyStockCompatible, looksLikeH9, stockCompatible, stockDifferences, writeSector } from './firmware.mjs?v=20261119h';
import { createHidArchive, knownAppMatches, loadHidArchive, readAccessibleFlash } from './readback.mjs?v=20261110a';
import { extractPinnedStockFirmware, TYPE_C_ZIP_URL } from './stock-source.mjs?v=20261110a';
import { stageAprs, stageRegion } from './aprs-stage.mjs?v=20261119h';

const $ = id => document.getElementById(id);
const slides = [...document.querySelectorAll('.guide-slide')];
const stepIndicators = [...document.querySelectorAll('.guide-steps li')];
const browserReady = window.isSecureContext && Boolean(navigator.hid?.requestDevice);
const smartBeacon = {
  label: 'H9 Toolbox Firmware with SmartBeacon',
  file: 'smartbeacon/h9-smartbeacon-bench.fw',
  residentFile: 'smartbeacon/h9-smartbeacon-resident.bin',
  residentSha256: 'aa6f231787913d7787ba282ea0bf503733b5dfbb19b9bdb1760bded90364959e',
  sectors: [...BUILD.sectors, 0x61000, 0x63000, 0x6d000, 0x74000, 0x75000, 0x7b000, 0x80000, 0x89000, 0x8a000],
  packageSha256: 'ee06b1f2ac4d224341ce25048b00ea2db999c16b2fe076251ff30a102ae7fc04',
  imageSha256: 'c230c64720957d7d8a021c9038c53399c5fd406c40577919fd1ae291cd70dd3d',
};
const devSmartBeacon = $('optBuild').hasAttribute('data-smartbeacon');
const release = devSmartBeacon ? smartBeacon : BUILD;
class BackupRequiredError extends Error {}
let currentStep = 0;
// target: the image to install (kind 'build' or 'stock'). stockSeed: exact
// stock rebuilt from the build. buildImage / previousImages: the hosted build
// and earlier builds ([{ info: PREVIOUS[i], image }]), known so a radio
// running any of them is recognised, backed up quickly and upgraded directly.
// createdBackup: the archive made on this page ({ archive, name, source }),
// kept across firmware selection and used when no backup file is chosen.
let createdBackup = null;
// True once the connected radio has been backed up on this page.
let radioBackedUp = false;
let target = null, stockSeed = null, buildImage = null, benchImage = null, previousImages = [], backup = null, device = null, hid = null, busy = false;
let cancelBackup = false, backupRunning = false;
let firmwareSelection = 0, backupSelection = 0;
const resumeKey = 'h9-hid-pending-v1';
function showStep(index) {
  slides[currentStep].hidden = true;
  stepIndicators[currentStep].removeAttribute('aria-current');
  currentStep = index;
  $('status').hidden = true;
  slides[currentStep].hidden = false;
  stepIndicators[currentStep].setAttribute('aria-current', 'step');
  stepIndicators.forEach((indicator, step) => indicator.classList.toggle('past', step < currentStep));
  refreshGuide();
  const heading = $(slides[currentStep].getAttribute('aria-labelledby'));
  heading.focus({ preventScroll: true });
  heading.scrollIntoView({ block: 'nearest' });
}

function refreshGuide() {
  const next = $('nextStep');
  const steps = [
    { label: 'Start setup', ready: browserReady },
    { label: 'Choose firmware', ready: Boolean(hid) },
    { label: 'Continue to install', ready: Boolean(hid && target) },
  ];
  const action = steps[currentStep];
  $('previousStep').disabled = busy || currentStep === 0;
  const disconnected = !hid && currentStep >= 2 && currentStep < slides.length - 1;
  const reconnect = $('reconnectStep');
  reconnect.hidden = !disconnected;
  reconnect.disabled = busy;
  next.hidden = !action || disconnected;
  next.disabled = busy || !action?.ready;
  if (action) next.textContent = `${action.label} →`;
}

$('previousStep').addEventListener('click', () => {
  if (!busy && currentStep > 0) showStep(currentStep - 1);
});
$('nextStep').addEventListener('click', () => {
  if (!$('nextStep').disabled) showStep(currentStep + 1);
});
$('reconnectStep').addEventListener('click', () => {
  if (!busy) showStep(1);
});

function status(message, error = false) {
  $('status').hidden = false;
  $('status').textContent = message;
  if (error) {
    const slide = slides[currentStep];
    const help = backupRunning ? $('backupHelp') : slide.querySelector('#writeHelp, .guide-help:not([hidden])');
    if (help) {
      help.hidden = false;
      help.open = true;
    }
  }
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
function finish(text) {
  if (backupRunning) $('backupProgress').textContent = text;
  else {
    $('progress').style.width = '100%';
    $('progressText').textContent = `100% · ${text}`;
  }
}
function progress(done, total) {
  if (total !== timing.total) timing = { ...timing, started: timing.total ? performance.now() : timing.started || performance.now(), total };
  const percent = Math.round(done / total * 100);
  const elapsed = (performance.now() - timing.started) / 1000;
  const parts = [`${percent}%`];
  if (timing.label) parts.push(typeof timing.label === 'function' ? timing.label(done, total) : timing.label);
  if (done >= total) parts.push(`done in ${formatDuration(elapsed)}`);
  else if (done > 0 && elapsed >= 3) parts.push(`about ${formatDuration(elapsed * (total - done) / done)} left`);
  if (backupRunning) {
    $('backupProgress').hidden = false;
    $('backupProgress').textContent = parts.join(' · ');
  } else {
    $('progress').style.width = `${percent}%`;
    $('progressText').textContent = parts.join(' · ');
  }
}
let shownSelection = null;
function refresh() {
  if (!busy && device && !device.opened) clearConnection();
  $('connect').disabled = busy || !browserReady || !!device;
  $('disconnect').disabled = busy || !device;
  $('cancelBackup').hidden = !backupRunning;
  const ready = Boolean(hid && target);
  const skipping = $('skipBackup').checked;
  const canRestore = ready && Boolean(backup);
  $('skipBackup').disabled = busy || !ready;
  $('acknowledge').disabled = busy || !ready;
  $('flash').disabled = busy || !ready || !$('acknowledge').checked;
  $('restore').disabled = busy || !canRestore || !$('acknowledge').checked;
  $('restore').textContent = backup?.source === 'hosted-stock' ? 'Restore stock v1.0.38' : 'Restore backup';
  if (!busy) $('planState').textContent = !hid ? 'Return to step 2 to connect your radio.'
    : !target ? 'Return to step 3 to select firmware.'
      : skipping ? 'The updater will verify the radio before writing. No new backup will be downloaded.'
        : '';
  // Highlight the chosen firmware; the animation plays when the choice changes.
  const selected = target?.kind ?? null;
  if (selected !== shownSelection) {
    shownSelection = selected;
    $('optBuild').classList.toggle('selected', selected === 'build');
    $('optStock').classList.toggle('selected', selected === 'stock');
    $('optBuild').querySelector('.selected-badge').hidden = selected !== 'build';
    $('optStock').querySelector('.selected-badge').hidden = selected !== 'stock';
    $('firmwareState').classList.toggle('chosen', Boolean(selected));
  }
  $('firmwareFile').disabled = busy;
  $('loadBuild').disabled = busy;
  $('loadStockGitHub').disabled = busy;
  $('backupFile').disabled = busy;
  refreshGuide();
  $('flash').textContent = target?.kind === 'build' ? `Install ${release.label}`
    : target?.kind === 'stock' ? 'Install stock v1.0.38' : 'Install selected firmware';
}
function requireH9(image) {
  if (!looksLikeH9(image)) throw new Error('This radio\'s firmware does not look like a TD-H9 application (no "TD-H9" model name). Installing is blocked.');
}
function fail(error) { status(error?.message || String(error), true); }
function pending() { try { return JSON.parse(localStorage.getItem(resumeKey)); } catch { return null; } }
function forgetPlan() { $('acknowledge').checked = false; refresh(); }
function resetBackupOverride() {
  $('skipBackup').checked = false;
}
const previousInfo = kind => PREVIOUS.find(item => item.id === kind);
// [name, image] pairs of every known image, for classifyRadio.
function namedImages() {
  return [['stock', stockSeed ?? target?.image], ['build', buildImage], [smartBeacon.label, benchImage], ...previousImages.map(p => [p.info.id, p.image])]
    .filter(([, image]) => image);
}
// Show write errors in place and verified outcomes on the restart slide.
function showResult(result) {
  const panel = result?.failed ? $('writeResult') : $('completedResult');
  $('writeResult').hidden = !result?.failed;
  if (!result) return;
  panel.classList.toggle('failed', Boolean(result.failed));
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
  if (!result.failed) showStep(slides.length - 1);
}
// Stock, H9 Toolbox Firmware, and the earlier builds' images, from
// the hosted files if no firmware has been selected yet. A backup needs them
// to recognise the radio, not a chosen target.
async function ensureReferences() {
  if (buildImage && previousImages.length && (stockSeed || target?.kind === 'stock')) return;
  const hosted = await loadHostedBuild();
  buildImage = hosted.build.image; benchImage = hosted.bench; previousImages = hosted.previous; stockSeed ??= hosted.stock;
}
function knownImages() {
  if (target?.kind !== 'build') return [];
  const known = previousImages.map(item => item.image);
  return devSmartBeacon ? [buildImage, ...known] : known;
}

async function loadSmartBeaconFile(file, expectedHash) {
  const response = await fetch(`./${file}?v=${expectedHash.slice(0, 8)}`);
  if (!response.ok) throw new Error('The SmartBeacon release file could not be downloaded.');
  const bytes = new Uint8Array(await response.arrayBuffer());
  if (await sha256(bytes) !== expectedHash)
    throw new Error('The SmartBeacon release file did not match the published checksum.');
  return bytes;
}

async function loadSmartBeaconImage() {
  const bytes = await loadSmartBeaconFile(smartBeacon.file, smartBeacon.packageSha256);
  const image = decodePackage(bytes);
  if (await sha256(image) !== smartBeacon.imageSha256)
    throw new Error('The SmartBeacon main image did not match the published checksum.');
  return image;
}

async function stageSmartBeaconResident() {
  status('Checking and verifying the SmartBeacon scheduler. Keep USB connected.');
  phase('Verifying the SmartBeacon scheduler');
  const bytes = await loadSmartBeaconFile(smartBeacon.residentFile, smartBeacon.residentSha256);
  const image = new Uint8Array(RESIDENT_END - RESIDENT_START).fill(0xff);
  image.set(bytes);
  const result = await stageRegion(hid, image, RESIDENT_START, 'SmartBeacon scheduler area',
    hid.writeSmartBeaconResident.bind(hid), progress);
  if (result !== 'ready')
    throw new Error('The SmartBeacon scheduler area contains different data. The updater left it untouched and did not install the main firmware.');
}

// Fetch the hosted build, its stock seed, and the earlier builds' sectors;
// every file is checked by SHA-256.
async function loadHostedBuild() {
  const get = (file, hash) => fetch(`./${file}?v=${hash.slice(0, 8)}`, { cache: 'no-store' });
  const [packageResponse, seedResponse, ...previousResponses] = await Promise.all([
    get(BUILD.packageFile, BUILD.packageSha256),
    get(BUILD.stockSeedFile, BUILD.stockSeedSha256),
    ...PREVIOUS.map(item => get(item.seedFile, item.seedSha256)),
  ]);
  if (![packageResponse, seedResponse, ...previousResponses].every(response => response.ok)) throw new Error(`Could not download the ${BUILD.label} files.`);
  const build = await loadBuildPackage(new Uint8Array(await packageResponse.arrayBuffer()));
  const stock = await loadStockSeed(new Uint8Array(await seedResponse.arrayBuffer()), build.image);
  const previous = [];
  for (const [index, info] of PREVIOUS.entries())
    previous.push({ info, image: await loadPreviousImage(new Uint8Array(await previousResponses[index].arrayBuffer()), build.image, info) });
  const bench = devSmartBeacon ? await loadSmartBeaconImage() : null;
  return { build, stock, previous, bench };
}
async function verifyLivePlan(current, changed, resumeSector, reference, known) {
  const targetCrcs = expectedCrcs(target.image);
  if (resumeSector) {
    const index = (resumeSector - APP_START) / SECTOR_SIZE;
    const referenceCrc = expectedCrcs(reference.image)[index];
    if (current[index] !== referenceCrc && current[index] !== targetCrcs[index]) return;
  }
  const live = reference.image.slice();
  const knownCrcs = known.map(expectedCrcs);
  for (const physical of changed) {
    const index = (physical - APP_START) / SECTOR_SIZE;
    if (current[index] === targetCrcs[index])
      live.set(target.image.subarray(physical, physical + SECTOR_SIZE), physical);
    else {
      const match = knownCrcs.findIndex(crcs => current[index] === crcs[index]);
      if (match >= 0) live.set(known[match].subarray(physical, physical + SECTOR_SIZE), physical);
    }
  }
  if (!await knownAppMatches(hid, current, live))
    throw new Error('The radio does not match the firmware images used for verification. No writes allowed.');
}

async function validateSelectedBackup(announceWaiting = true) {
  const selection = ++backupSelection;
  backup = null;
  if (!backupRunning) forgetPlan();
  const file = $('backupFile').files[0];
  const name = file?.name ?? createdBackup?.name;
  if (!name) { $('backupState').textContent = 'No recovery backup selected.'; refresh(); return; }
  if (!target) {
    $('backupState').textContent = `${name} · ${file ? 'checked when you choose firmware' : 'saved; checked when you choose firmware'}`;
    if (announceWaiting && file) status('Backup selected. Choose firmware to check it against.');
    refresh(); return;
  }
  if (!file && !['stock', 'stock-like', 'other', smartBeacon.label].includes(createdBackup.source) && target.kind === 'build') {
    // `source` is what the backup scan matched: 'build', or an earlier
    // build's PREVIOUS id (e.g. 'release1').
    if (createdBackup.source === 'build' || previousInfo(createdBackup.source)) {
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
    // Stock outside the pinned sectors: the quick install. Anything else that is
    // still a TD-H9 application (a backup of an unrecognised radio, taken here or
    // loaded from the saved .h9hid after a refresh) unlocks a full-image install of
    // every differing sector. Either way the radio is checked against this backup
    // right before writing, so a file that does not match the radio is refused then.
    if (target.kind === 'build') {
      await ensureReferences();
      if (!stockCompatible(expectedCrcs(loaded.image), stockSeed)) { requireH9(loaded.image); loaded.fullInstall = true; }
    }
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

function beginFirmwareSelection() {
  const selection = ++firmwareSelection;
  resetBackupOverride();
  $('firmwareHelp').hidden = true;
  $('firmwareHelp').open = false;
  $('status').hidden = true;
  ++backupSelection; target = null; stockSeed = null; buildImage = null; benchImage = null; previousImages = []; backup = null; forgetPlan();
  return selection;
}

function rejectFirmware(message, error) {
  $('firmwareState').textContent = message;
  $('firmwareHelp').hidden = false;
  $('firmwareHelp').open = true;
  fail(error);
}

$('firmwareFile').addEventListener('change', async event => {
  const selection = beginFirmwareSelection();
  const file = event.target.files[0];
  if (!file) { $('firmwareState').textContent = 'No package selected'; await validateSelectedBackup(false); return; }
  try {
    const loaded = await loadStockPackage(new Uint8Array(await file.arrayBuffer()));
    if (selection !== firmwareSelection) return;
    target = loaded; $('firmwareState').textContent = `${file.name} · official v1.0.38 verified`;
    status('Official v1.0.38 firmware package verified locally.');
  } catch (error) {
    if (selection !== firmwareSelection) return;
    rejectFirmware('Package rejected', error);
  }
  await validateSelectedBackup(false);
});
$('loadBuild').addEventListener('click', async () => {
  const selection = beginFirmwareSelection();
  busy = true; refresh();
  try {
    status(`Downloading the pinned ${release.label} package and recovery seeds from H9 Toolbox.`);
    const hosted = await loadHostedBuild();
    if (selection !== firmwareSelection) return;
    target = devSmartBeacon ? { kind: 'build', image: hosted.bench, packageHash: smartBeacon.packageSha256 } : hosted.build;
    stockSeed = hosted.stock; buildImage = hosted.build.image; benchImage = hosted.bench; previousImages = hosted.previous;
    $('firmwareFile').value = '';
    $('firmwareState').textContent = `${release.label} · exact .fw verified`;
    status(`${release.label} package, exact stock v1.0.38 seed, and earlier-build sectors verified locally. No radio write sent.`);
    await validateSelectedBackup(false);
  } catch (error) {
    if (selection === firmwareSelection) rejectFirmware(`${release.label} package rejected`, error);
  } finally { busy = false; refresh(); }
});
$('loadStockGitHub').addEventListener('click', async () => {
  const selection = beginFirmwareSelection();
  busy = true; refresh();
  try {
    status('Downloading the pinned Type-C v1.0.38 ZIP from the TDH9 GitHub mirror.');
    const [zipResponse, hosted] = await Promise.all([fetch(TYPE_C_ZIP_URL, { cache: 'no-store' }), loadHostedBuild()]);
    if (!zipResponse.ok) throw new Error('Could not download the stock firmware source.');
    const stockBytes = await extractPinnedStockFirmware(new Uint8Array(await zipResponse.arrayBuffer()));
    const loaded = await loadStockPackage(stockBytes);
    if (selection !== firmwareSelection) return;
    target = loaded; buildImage = hosted.build.image; benchImage = hosted.bench; previousImages = hosted.previous; $('firmwareFile').value = '';
    $('firmwareState').textContent = 'Stock v1.0.38 · pinned GitHub ZIP and extracted .fw verified';
    status('Stock Type-C v1.0.38 package verified locally. The hosted builds are available as fast backup seeds. No radio write sent.');
    await validateSelectedBackup(false);
  } catch (error) {
    if (selection === firmwareSelection) rejectFirmware('GitHub stock package rejected', error);
  } finally { busy = false; refresh(); }
});
$('backupFile').addEventListener('change', async () => {
  busy = true; refresh();
  try { await validateSelectedBackup(); }
  finally { busy = false; refresh(); }
});
$('connect').addEventListener('click', async () => {
  if (!browserReady || busy || device) return;
  let chosen;
  busy = true;
  refresh();
  try {
    const devices = await navigator.hid.requestDevice({ filters: [{ vendorId: 0x4c4a, productId: 0x4155, usagePage: 0xff00, usage: 1 }] });
    if (devices.length !== 1) {
      throw new Error(devices.length ? 'Choose exactly one JieLi device.' : 'No radio selected. Open the picker again and choose JieLi.');
    }
    chosen = devices[0];
    if (!chosen.collections.some(collection => collection.usagePage === 0xff00 && collection.usage === 1 && collection.inputReports.some(report => report.reportId === 0) && collection.outputReports.some(report => report.reportId === 0))) throw new Error('Unexpected HID report layout.');
    await chosen.open();
    const candidate = new H9Hid(chosen), geometry = await candidate.initialize();
    device = chosen; hid = candidate; radioBackedUp = false;
    $('deviceState').textContent = 'Connected';
    $('deviceState').classList.add('connected');
    status(`Radio connected (app 0x${geometry.address.toString(16)}, 4 KiB sectors). You can choose your firmware now.`);
  } catch (error) {
    if (chosen?.opened) await chosen.close().catch(() => {});
    fail(error);
  } finally {
    busy = false;
    refresh();
  }
});
function clearConnection() {
  device = null;
  hid = null;
  radioBackedUp = false;
  resetBackupOverride();
  $('acknowledge').checked = false;
  $('deviceState').textContent = 'Disconnected';
  $('deviceState').classList.remove('connected');
}
if (browserReady) navigator.hid.addEventListener('disconnect', event => {
  if (event.device !== device || busy) return;
  clearConnection();
  if (currentStep !== slides.length - 1) status('The radio disconnected. Return to step 2 to reconnect it.', true);
  refresh();
});
$('disconnect').addEventListener('click', async () => {
  busy = true;
  refresh();
  try {
    await device.close();
    clearConnection();
    status('Disconnected.');
  } catch (error) { fail(error); }
  finally { busy = false; refresh(); }
});
$('acknowledge').addEventListener('change', refresh);
$('skipBackup').addEventListener('change', refresh);
$('cancelBackup').addEventListener('click', () => { cancelBackup = true; $('cancelBackup').disabled = true; status('Stopping after the current HID reply…'); });
async function createBackup() {
  backupRunning = true;
  $('cancelBackup').disabled = false;
  phase('Starting backup');
  progress(0, 1);
  refresh();
  const backupBegan = performance.now();
  status('Scanning accessible flash sector CRCs. No erase or write will be sent.');
  phase('Scanning sector CRCs');
  const current = await scanRadio(hid, progress);
  await ensureReferences();
  const images = namedImages();
  let source = 'other';
  if (images.length > 1) {
    try { source = classifyRadio(current, images); } catch {
      // Not an exact known image: still fine if it is stock everywhere except
      // the sectors the install rewrites (e.g. leftovers after an official update).
      const leftovers = stockSeed ? stockCompatible(current, stockSeed) : null;
      if (leftovers) {
        source = 'stock-like';
        status(`This radio is stock v1.0.38 except ${leftovers.length} sector(s) the install rewrites anyway (${leftovers.slice(0, 8).map(p => `0x${p.toString(16)}`).join(', ')}${leftovers.length > 8 ? ', …' : ''}). Backing it up in full, which takes longer.`);
      } else {
        // Not stock-compatible: still take the full backup (it is only a read). That
        // backup then unlocks a full-image install of every differing sector.
        source = 'other';
        status('This firmware needs a full backup, which takes longer. The updater will then check which sectors need to be written.');
      }
    }
  }
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
  else if (source === 'stock-like') verifyStockCompatible(loaded.image, stockSeed);
  const expected = source === 'build' ? BUILD.imageSha256
      : source === smartBeacon.label ? smartBeacon.imageSha256 : previousInfo(source)?.imageSha256;
  if (expected && await sha256(loaded.image) !== expected)
    throw new Error('Recovered image differs from the pinned build it matched. No backup accepted.');
  if (cancelBackup) throw new Error('HID backup cancelled. No radio write was sent.');
  $('cancelBackup').disabled = true;
  const sourceName = source === 'build' ? 'H9-Toolbox-Firmware-'
      : source === smartBeacon.label ? 'SmartBeacon-Bench-'
        : source === 'stock-like' ? 'Stock-' : source === 'other' ? 'Unrecognised-' : previousInfo(source)?.file ?? '';
  const sourceLabel = source === 'build' ? BUILD.label : source === smartBeacon.label ? smartBeacon.label : previousInfo(source)?.label;
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
  if (source === smartBeacon.label)
    status('SmartBeacon main firmware backup saved. This backup covers the main firmware only.');
  finish(`Backup built and verified in ${formatDuration((performance.now() - backupBegan) / 1000)}`);
}
// Check the radio and plan both directions: a full sector-CRC scan, the
// pinned sector set, the live shifted-window check, and any interrupted
// sector. Nothing is erased or written here.
async function inspectRadio(reference, known) {
  status('Checking the radio: scanning 251 sectors (CRC only, no writes)…');
  phase('Checking the radio (CRC scan, no writes)');
  const current = await scanRadio(hid, progress);
  const marker = pending();
  const resumeSector = marker?.packageHash === target.packageHash && marker?.backupHash === reference.hash ? marker.sector : null;
  let changed;
  try {
    changed = planUpdate(current, reference.image, target.image, resumeSector, known);
  } catch (error) {
    const mismatch = /^Radio differs from the supplied backup at (0x[\da-f]+)\./i.exec(error.message);
    if (reference.source === 'verified-firmware' && mismatch) {
      const selected = target.kind === 'stock' ? 'stock v1.0.38' : release.label;
      throw new BackupRequiredError(`The radio’s main firmware doesn’t match the images verified for a skip-backup install. The first difference is at ${mismatch[1]}. To install ${selected}, clear Skip backup or select a saved recovery file. Nothing was written.`);
    }
    throw error;
  }
  if (target.kind === 'build' && !reference.fullInstall && !changed.every(physical => release.sectors.includes(physical)))
    throw new Error(`${release.label} sector plan leaves its ${release.sectors.length} pinned sectors. No writes allowed.`);
  phase('Checking the radio (shifted CRC windows)');
  if (target.kind === 'build' || reference.source === 'verified-firmware')
    await verifyLivePlan(current, changed, resumeSector, reference, known);
  const backupCrcs = expectedCrcs(reference.image), targetCrcs = expectedCrcs(target.image);
  const knownCrcs = known.map(expectedCrcs);
  const unknown = changed.filter(physical => {
    const index = (physical - APP_START) / SECTOR_SIZE;
    return current[index] !== backupCrcs[index] && current[index] !== targetCrcs[index] &&
      !knownCrcs.some(crcs => crcs[index] === current[index]);
  });
  if (unknown.length && (unknown.length !== 1 || unknown[0] !== resumeSector))
    throw new Error(`The radio does not match this backup: ${unknown.length} sector(s) are neither in the backup nor in the new firmware (${unknown.slice(0, 6).map(p => `0x${p.toString(16)}`).join(', ')}${unknown.length > 6 ? ', …' : ''}). Choose the backup taken from this radio before the install, or take a new one. No writes allowed.`);
  const index = physical => (physical - APP_START) / SECTOR_SIZE;
  return {
    update: changed.filter(physical => current[index(physical)] !== targetCrcs[index(physical)]),
    restore: changed.filter(physical => current[index(physical)] !== backupCrcs[index(physical)]),
    interrupted: unknown.length > 0,
  };
}
const stockRestart = ['Disconnect USB and power-cycle the radio.', 'Check the firmware version, channels, and settings.'];
const toolboxRestart = [
  'Disconnect USB and power-cycle the radio.',
  'From the channel screen, hold Back to open the app menu.',
  devSmartBeacon ? 'Open the SmartBeacon catalog in App Manager and install the matching menu and apps.'
    : 'Use the App Manager over Bluetooth or experimental USB-C to add apps or a boot logo.',
];
const appManagerLink = {
  href: devSmartBeacon ? '../apps/?catalog=smartbeacon' : '../apps/',
  label: 'Open the App Manager',
};

function aprsResult(state) {
  return state === 'ready' ? ' The optional APRS update is ready. Open Unlock KISS TNC below for keypad instructions.' : '';
}

async function performWrite(direction) {
  if (direction === 'update' ? $('flash').disabled : $('restore').disabled) return;
  for (const id of ['backupHelp', 'writeHelp']) {
    $(id).hidden = true;
    $(id).open = false;
  }
  showResult(null);
  busy = true; refresh();
  try {
    if (direction === 'update' && !$('skipBackup').checked && !(radioBackedUp && backup)) {
      await createBackup();
      if (!backup) throw new Error('The updater could not verify the recovery backup. Check the selected restore point and try again.');
      backupRunning = false;
      refresh();
    }
    const began = performance.now();
    // Only use this connection's backup or a file explicitly selected by the user for an install.
    let reference = direction === 'restore' || radioBackedUp || $('backupFile').files.length ? backup : null;
    let known = knownImages();
    if (!reference) {
      await ensureReferences();
      const image = target.kind === 'stock' ? target.image : stockSeed;
      reference = { image, hash: await sha256(image), source: 'verified-firmware' };
      if (target.kind === 'stock') known = [buildImage, benchImage, ...previousImages.map(item => item.image)].filter(Boolean);
    }
    const destination = direction === 'update' ? target.image : reference.image;
    const checked = await inspectRadio(reference, known);
    const work = checked[direction];
    const toolbox = direction === 'update' && target.kind === 'build';
    let aprs = null;
    if (toolbox && devSmartBeacon) await stageSmartBeaconResident();
    if (toolbox && !devSmartBeacon) {
      status('Checking the APRS update file and staging area. Keep USB connected.');
      const response = await fetch(`./${BUILD.aprsStageFile}?v=${BUILD.aprsStageSha256.slice(0, 8)}`, { cache: 'no-store' });
      if (!response.ok) throw new Error('Could not download the staged APRS update file.');
      phase('Staging and verifying the APRS update');
      aprs = await stageAprs(hid, new Uint8Array(await response.arrayBuffer()), progress);
      status(aprs === 'ready' ? 'Staged APRS update verified. The restart slide has the keypad instructions.'
        : 'The APRS staging area is occupied. Preserving its contents and continuing the main firmware install.');
    }
    $('planState').textContent = `Radio checked: ${work.length} sector(s) to ${direction === 'update' ? 'write' : 'restore'}${checked.interrupted ? ', including one interrupted sector' : ''}.`;
    $('unlock-kiss').hidden = aprs !== 'ready';
    $('unlock-kiss').open = false;
    $('kissGate').hidden = aprs !== 'occupied';
    if (!work.length) {
      finish('Main firmware already matches');
      $('acknowledge').checked = false;
      resetBackupOverride();
      status('Every main firmware sector already matches.');
      showResult({
        title: toolbox ? `${release.label} is already installed` : 'The radio already matches the selected firmware',
        text: 'Every firmware sector matched during verification.' + aprsResult(aprs),
        steps: toolbox ? toolboxRestart : stockRestart,
        action: toolbox ? appManagerLink : null,
      });
      return;
    }
    if (direction === 'update' && reference.fullInstall && work.length > release.sectors.length)
      status(`Full firmware install: this radio is not on stock v1.0.38, so ${work.length} of ${(IMAGE_SIZE - APP_START) / SECTOR_SIZE} firmware sectors differ and are rewritten (the rest already match). This takes longer. Keep USB connected and the radio powered on; if it is interrupted, follow the recovery guidance on this step.`);
    status(`Radio checked. ${direction === 'update' ? 'Writing' : 'Restoring'} ${work.length} sectors. Keep USB connected and the radio powered on.`);
    phase((done, total) => done >= total ? `${total} sectors written and verified` : `Writing and verifying sector ${done + 1} of ${total}`);
    progress(0, work.length);
    for (let index = 0; index < work.length; index++) {
      const physical = work[index];
      localStorage.setItem(resumeKey, JSON.stringify({ packageHash: target.packageHash, backupHash: reference.hash, sector: physical, direction }));
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
    resetBackupOverride();
    const seconds = formatDuration((performance.now() - began) / 1000);
    finish(`${work.length} sectors ${direction === 'update' ? 'installed' : 'restored'} and every sector CRC verified in ${seconds}`);
    status('Firmware verification passed. You can disconnect USB and restart the radio.');
    showResult({
      title: toolbox ? `${release.label} installed and verified`
        : direction === 'update' ? 'Stock firmware installed and verified'
          : reference.source === 'hosted-stock' ? 'Stock v1.0.38 restored and verified' : 'Backup restored and verified',
      text: `Updated ${work.length} sector(s) and verified every firmware sector (${seconds}).` + aprsResult(aprs),
      steps: toolbox ? toolboxRestart : stockRestart,
      action: toolbox ? appManagerLink : null,
    });
  } catch (error) {
    if (backupRunning) $('backupProgress').hidden = true;
    if (cancelBackup) {
      status('Backup cancelled. No radio write was sent.');
    } else {
      fail(error);
      showResult({
        failed: true,
        title: backupRunning ? 'Recovery backup did not finish'
          : error instanceof BackupRequiredError ? 'Backup required to install this firmware' : 'Firmware verification did not finish',
        text: error?.message || String(error),
      });
    }
  } finally {
    busy = false; backupRunning = false; cancelBackup = false; refresh();
  }
}
$('flash').addEventListener('click', () => performWrite('update'));
$('restore').addEventListener('click', () => performWrite('restore'));

$('browserCheck').textContent = browserReady ? 'Your browser supports USB updates. Select Start setup when you’re ready.'
  : !window.isSecureContext ? 'Open this page using a secure link that starts with https://, then try again.'
    : 'This browser can’t connect to the radio over USB. Open this page in Chrome or Edge on a computer.';
$('browserCheck').classList.toggle('unavailable', !browserReady);
if (!browserReady) slides[0].querySelector('.guide-help').open = true;
refresh();
