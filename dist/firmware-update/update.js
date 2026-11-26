import { APP_START, IMAGE_SIZE, SECTOR_SIZE, APRS_STAGE_START, RESIDENT_START, RESIDENT_END, BUILD, PREVIOUS, crc16, decodePackage, expectedCrcs, H9Hid, loadBackup, loadBuildPackage, loadPreviousImage, loadStockPackage, loadStockSeed, planUpdate, scanRadio, sha256, looksLikeH9, stockCompatible, writeSector } from './firmware.mjs?v=20261119m';
import { BackupStoppedError, createHidArchive, knownAppMatches, loadHidArchive, planFirmwareWithoutBackup, readFirmwareBackup, UnverifiedRadioError } from './readback.mjs?v=20261126quick';
import { extractPinnedStockFirmware, TYPE_C_ZIP_URL } from './stock-source.mjs?v=20261110a';
import { backedUpFar, FarFirmwareHid, planFarFirmware, planFarFirmwareWithoutBackup, writeFarFirmware } from './far-region.mjs?v=20261126sectors';
import { loadAprsStage, planStageRegion, writeStageRegion } from './aprs-stage.mjs?v=20261126sectors';
import { createFlashMap } from './flash-map.mjs?v=20261126sectors';
import { availableCombinedBuilds, selectCombinedBuild } from './combined-builds.mjs?v=20261126options';

const $ = id => document.getElementById(id);
const slides = [...document.querySelectorAll('.guide-slide')];
const stepIndicators = [...document.querySelectorAll('.guide-steps li')];
const browserReady = window.isSecureContext && Boolean(navigator.hid?.requestDevice);
const legacyDevBuild = {
  id: 'H9 Toolbox Firmware v0.1.5 (dev)',
  label: BUILD.label,
  file: 'dev/h9-toolbox-firmware-v0.1.5.fw',
  packageSha256: '32b2b6308b2e8ef71d2ef5e15d543ce9b2594fd9455041d909653324446ec51e',
  imageSha256: '4a2b543fd2fdae8256bd0d568d286a25f66c18e8e1bf9bd9662a98921fe01c38',
  aprsStageFile: 'dev/h9-toolbox-aprs-stage.bin',
  aprsStageSha256: '55be5fafe4efae0287345a25278e9ee00171e7af164e1c502b1bcc98a37924ba',
  residentFile: 'dev/h9-toolbox-aprs-scheduler.bin',
  residentSha256: 'aa6f231787913d7787ba282ea0bf503733b5dfbb19b9bdb1760bded90364959e',
  sectors: [...BUILD.sectors, 0x61000, 0x63000, 0x6d000, 0x74000, 0x75000, 0x7b000, 0x80000, 0x89000, 0x8a000],
};
const earlierDevBuild = {
  id: 'Earlier H9 Toolbox development build',
  label: 'Earlier H9 Toolbox development build',
  file: 'dev/h9-toolbox-earlier-dev.fw',
  packageSha256: 'ee06b1f2ac4d224341ce25048b00ea2db999c16b2fe076251ff30a102ae7fc04',
  imageSha256: 'c230c64720957d7d8a021c9038c53399c5fd406c40577919fd1ae291cd70dd3d',
};
const priorDevBuild = {
  id: 'Previous H9 Toolbox v0.1.5 development build',
  label: 'Previous H9 Toolbox v0.1.5 development build',
  file: 'dev/h9-toolbox-dev-before-aprs.fw',
  packageSha256: 'e64b6a5f3d5dd17077a392bffc837486a7076e9e52c6cc2b19e34e2a0dec85e0',
  imageSha256: 'e976a10f89aea4f42757c582bb10cc828018e56bf36eb7b8ee2093748b335e92',
};
const firmwareOption = $('optBuild');
const requestedRelease = new URLSearchParams(location.search).get('release') ?? '';
const useLegacyDevBuild = firmwareOption.hasAttribute('data-legacy-dev-firmware') && requestedRelease === '0.1.5';
const useRc = firmwareOption.hasAttribute('data-release-firmware') && !useLegacyDevBuild;
const devOptions = useRc && firmwareOption.hasAttribute('data-33cm-firmware');
const usePackagedRelease = useRc || useLegacyDevBuild;
const previewBuild = selectCombinedBuild();
const packagedBuild = useRc ? previewBuild : legacyDevBuild;
const release = usePackagedRelease ? packagedBuild : BUILD;

function selectedCombinedBuild() {
  return selectCombinedBuild({
    quickControls: $('quickControls').checked,
    band33cm: $('band33cm').checked,
    includeModem: $('includeModem').checked,
    dev: devOptions,
  });
}
if (useRc) {
  const defaultVersion = devOptions ? '0.1.8' : firmwareOption.hasAttribute('data-next-firmware') ? '0.1.7' : '0.1.6';
  const requestedVersion = requestedRelease.replace(/-no-modem$/, '') || defaultVersion;
  for (const id of ['buildProfiles', 'coreFeatures', 'radioFeatures', 'packetFeatures']) $(id).hidden = false;
  if (devOptions) {
    $('standardBuildOption').hidden = false;
    $('fullBuildDescription').textContent = 'All options, including experimental 33 cm controls.';
  }
  $('band33cm').checked = devOptions && requestedVersion === '0.1.8';
  $('quickControls').checked = $('band33cm').checked || requestedVersion === '0.1.7';
  $('includeModem').checked = requestedRelease !== '0.1.6-rc.4' &&
    !(requestedRelease.endsWith('-no-modem') && ($('quickControls').checked || requestedVersion === '0.1.6'));
  if ($('quickControls').checked && $('includeModem').checked) {
    $(!$('band33cm').checked && devOptions ? 'standardBuild' : 'fullBuild').checked = true;
  } else if (!$('quickControls').checked && !$('includeModem').checked) {
    $('essentialsBuild').checked = true;
  } else {
    $('customBuild').checked = true;
  }
  firmwareOption.querySelector('small').textContent = 'Select a preset or customize the features installed on your radio.';
} else if (useLegacyDevBuild) {
  firmwareOption.querySelector('strong').textContent = legacyDevBuild.label;
  firmwareOption.querySelector('small').textContent = 'Includes the Toolbox app loader, Bluetooth packet support, SmartBeacon, timed APRS beacons, and APRS Mute.';
  $('loadBuild').textContent = 'Load H9 Toolbox v0.1.5';
  $('rcPreviewLink').hidden = false;
}
let currentStep = 0;
// target: the image to install (kind 'build' or 'stock'). stockSeed: exact
// stock rebuilt from the build. buildImage / previousImages: the hosted build
// and earlier builds ([{ info: PREVIOUS[i], image }]), known so a radio
// running any of them is recognised, backed up quickly and upgraded directly.
// createdBackup: the archive made on this page ({ archive, name }),
// kept across firmware selection and used when no backup file is chosen.
let createdBackup = null;
// True once the connected radio has been backed up on this page.
let radioBackedUp = false;
let target = null, stockSeed = null, buildImage = null, packagedImage = null, previousImages = [], backup = null, device = null, hid = null, busy = false;
let backupCancelled = false, backupRunning = false;
let firmwareSelection = 0, backupSelection = 0;
const resumeKey = 'h9-hid-pending-v1';
const farResumeKey = 'h9-hid-pending-far-v1';
const flashMap = createFlashMap($('flashMap'), $('flashBlocks'), $('flashMapSummary'));
let reviewedPlan = null;
function resetPreview() {
  if (reviewedPlan) {
    $('progress').style.width = '0%';
    $('progressText').textContent = '0%';
  }
  reviewedPlan = null;
  flashMap.clear();
}
function showStep(index) {
  slides[currentStep].hidden = true;
  stepIndicators[currentStep].removeAttribute('aria-current');
  currentStep = index;
  if (index < 3) resetPreview();
  else $(index === 3 ? 'installFlashMap' : 'completedFlashMap').append($('flashMap'));
  $('status').hidden = true;
  slides[currentStep].hidden = false;
  stepIndicators[currentStep].setAttribute('aria-current', 'step');
  stepIndicators.forEach((indicator, step) => indicator.classList.toggle('past', step < currentStep));
  refresh();
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
function rcBackupSelected() { return useRc && target?.kind === 'build'; }

function refresh() {
  if (!busy && device && !device.opened) clearConnection();
  $('connect').disabled = busy || !browserReady || !!device;
  $('disconnect').disabled = busy || !device;
  $('cancelBackup').hidden = !backupRunning;
  $('cancelBackup').disabled = backupCancelled;
  const ready = Boolean(hid && target);
  const canRestore = ready && Boolean(backup) && (!rcBackupSelected() || Boolean(backedUpFar(backup.accessible)));
  $('legacyStageNote').hidden = useRc || target?.kind !== 'build' || !target.includeModem;
  $('rcInstallNote').hidden = !rcBackupSelected();
  $('exactBackup').disabled = busy || !ready || Boolean($('backupFile').files.length);
  $('acknowledge').disabled = busy || !ready;
  $('flash').disabled = busy || !ready || (reviewedPlan?.direction === 'update' && !$('acknowledge').checked);
  $('restore').disabled = busy || !canRestore || (reviewedPlan?.direction === 'restore' && !$('acknowledge').checked);
  if (!busy) $('planState').textContent = !hid ? 'Return to step 2 to connect your radio.'
    : !target ? 'Return to step 3 to select firmware.' : '';
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
  $('buildProfiles').disabled = busy;
  const canCustomize = useRc && $('customBuild').checked && !busy;
  $('includeModem').disabled = useRc ? !canCustomize : busy;
  $('quickControls').disabled = !canCustomize;
  $('band33cm').disabled = !canCustomize || !devOptions || !$('quickControls').checked;
  if (useRc) {
    const { version } = selectedCombinedBuild();
    firmwareOption.querySelector('strong').textContent = `H9 Toolbox Firmware v${version}`;
    $('loadBuild').textContent = `Load H9 Toolbox v${version}`;
  }
  $('loadStockGitHub').disabled = busy;
  $('backupFile').disabled = busy;
  refreshGuide();
  $('flash').textContent = reviewedPlan?.direction === 'update'
    ? target.kind === 'build' ? `Install ${target.combined?.label ?? release.label}` : 'Install stock v1.0.38'
    : 'Review flash plan';
  $('restore').textContent = reviewedPlan?.direction === 'restore' ? 'Restore backup' : 'Review restore plan';
}
function requireH9(image) {
  if (!looksLikeH9(image)) throw new Error('This radio\'s firmware does not look like a TD-H9 application (no "TD-H9" model name). Installing is blocked.');
}
function fail(error) { status(error?.message || String(error), true); }
function pending(key = resumeKey) { try { return JSON.parse(localStorage.getItem(key)); } catch { return null; } }
function forgetPlan() { resetPreview(); $('acknowledge').checked = false; refresh(); }
const previousInfo = kind => previousImages.find(item => item.info.id === kind)?.info;
// [name, image] pairs of every verified firmware reference.
function namedImages() {
  return [['stock', stockSeed ?? target?.image], ['build', buildImage], [packagedBuild.label, packagedImage], ...previousImages.map(p => [p.info.id, p.image])]
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
  buildImage = hosted.build.image; packagedImage = hosted.packaged; previousImages = hosted.previous; stockSeed ??= hosted.stock;
}
function knownImages() {
  if (target?.kind !== 'build') return [];
  const known = previousImages.map(item => item.image);
  return usePackagedRelease ? [buildImage, ...known] : known;
}

async function loadReleaseFile(file, expectedHash) {
  const response = await fetch(`./${file}?v=${expectedHash.slice(0, 8)}`);
  if (!response.ok) throw new Error('The H9 Toolbox release file could not be downloaded.');
  const bytes = new Uint8Array(await response.arrayBuffer());
  if (await sha256(bytes) !== expectedHash)
    throw new Error('The H9 Toolbox release file did not match the published checksum.');
  return bytes;
}

async function loadPackagedImage(info) {
  const bytes = await loadReleaseFile(info.file, info.packageSha256);
  const image = decodePackage(bytes);
  if (image.length !== IMAGE_SIZE || await sha256(image) !== info.imageSha256)
    throw new Error('The H9 Toolbox main image did not match the published checksum.');
  return image;
}

async function prepareStage(bytes, start, label, write) {
  const plan = await planStageRegion(hid, bytes, start, label);
  return { bytes, start, label, write, plan };
}

async function writePreparedStage(stage) {
  const { bytes, start, label, write, plan } = stage;
  phase(`Writing and verifying the ${label}`);
  return writeStageRegion(hid, bytes, start, label, write, progress, plan, flashMap.mark);
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
  let packaged = null;
  if (usePackagedRelease) {
    const otherDevBuilds = [earlierDevBuild, priorDevBuild, ...(useRc
      ? [...availableCombinedBuilds(devOptions).filter(build => build.id !== previewBuild.id), legacyDevBuild]
      : [])];
    const [current, ...others] = await Promise.all([packagedBuild, ...otherDevBuilds].map(loadPackagedImage));
    packaged = current;
    previous.push(...otherDevBuilds.map((info, index) => ({ info, image: others[index] })));
  }
  return { build, stock, previous, packaged };
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
      if (useRc || !stockCompatible(expectedCrcs(loaded.image), stockSeed)) { requireH9(loaded.image); loaded.fullInstall = true; }
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
  $('exactBackup').checked = false;
  $('firmwareHelp').hidden = true;
  $('firmwareHelp').open = false;
  $('status').hidden = true;
  ++backupSelection; target = null; stockSeed = null; buildImage = null; packagedImage = null; previousImages = []; backup = null; forgetPlan();
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
async function loadSelectedBuild() {
  const selection = beginFirmwareSelection();
  busy = true; refresh();
  try {
    const includeModem = $('includeModem').checked;
    const selectedBuild = useRc ? selectedCombinedBuild() : packagedBuild;
    const versionLabel = useRc ? `H9 Toolbox Firmware v${selectedBuild.version}` : release.label;
    status(`Downloading the verified ${versionLabel} package.`);
    const loaded = usePackagedRelease
      ? { kind: 'build', image: await loadPackagedImage(selectedBuild), packageHash: selectedBuild.packageSha256 }
      : await loadBuildPackage(await loadReleaseFile(BUILD.packageFile, BUILD.packageSha256));
    if (selection !== firmwareSelection) return;
    target = { ...loaded, combined: useRc ? selectedBuild : null, includeModem };
    $('firmwareFile').value = '';
    $('firmwareState').textContent = `${versionLabel} · ${includeModem ? 'with' : 'without'} modem updater`;
    status(`${versionLabel} package verified locally.`);
    await validateSelectedBackup(false);
  } catch (error) {
    if (selection === firmwareSelection) rejectFirmware('Toolbox package rejected', error);
  } finally { busy = false; refresh(); }
}
$('loadBuild').addEventListener('click', loadSelectedBuild);
function applyBuildSelection() {
  if (target?.kind === 'build') void loadSelectedBuild();
  else refresh();
}

$('buildProfiles').addEventListener('change', event => {
  const preset = event.target.value;
  if (preset === 'custom') {
    refresh();
    return;
  }
  $('quickControls').checked = preset !== 'essentials';
  $('band33cm').checked = preset === 'full' && devOptions;
  $('includeModem').checked = preset !== 'essentials';
  applyBuildSelection();
});
$('firmwareFeatures').addEventListener('change', () => {
  if (!$('quickControls').checked) $('band33cm').checked = false;
  applyBuildSelection();
});
$('exactBackup').addEventListener('change', () => { resetPreview(); refresh(); });
$('loadStockGitHub').addEventListener('click', async () => {
  const selection = beginFirmwareSelection();
  busy = true; refresh();
  try {
    status('Downloading the pinned Type-C v1.0.38 ZIP from the TDH9 GitHub mirror.');
    const zipResponse = await fetch(TYPE_C_ZIP_URL, { cache: 'no-store' });
    if (!zipResponse.ok) throw new Error('Could not download the stock firmware source.');
    const stockBytes = await extractPinnedStockFirmware(new Uint8Array(await zipResponse.arrayBuffer()));
    const loaded = await loadStockPackage(stockBytes);
    if (selection !== firmwareSelection) return;
    target = loaded; $('firmwareFile').value = '';
    $('firmwareState').textContent = 'Stock v1.0.38 · pinned GitHub ZIP and extracted .fw verified';
    status('Stock Type-C v1.0.38 package verified locally.');
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
    const candidate = useRc ? new FarFirmwareHid(chosen) : new H9Hid(chosen);
    const geometry = await candidate.initialize();
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
function clearConnection(keepMap = false) {
  if (!keepMap) resetPreview();
  device = null;
  hid = null;
  radioBackedUp = false;
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
$('cancelBackup').addEventListener('click', () => {
  if (!backupRunning || backupCancelled) return;
  backupCancelled = true;
  refresh();
  status('Stopping the backup after the current HID reply…');
});

async function createBackup() {
  backupRunning = true;
  phase('Starting backup');
  progress(0, 1);
  refresh();
  const backupBegan = performance.now();
  status('Scanning accessible flash sector CRCs. No erase or write will be sent.');
  phase('Scanning sector CRCs');
  const current = await scanRadio(hid, progress);
  if (backupCancelled) throw new BackupStoppedError();
  await ensureReferences();
  if (backupCancelled) throw new BackupStoppedError();
  const images = namedImages();
  const includeFar = useRc && target?.kind === 'build';
  const farImages = includeFar ? await Promise.all(availableCombinedBuilds(devOptions)
    .map(build => loadReleaseFile(build.farFile, build.farSha256))) : null;
  if (backupCancelled) throw new BackupStoppedError();
  status('Checking firmware against verified images and reading back any different sectors. Keep USB connected.');
  phase('Backing up firmware');
  progress(0, 1);
  const recovered = await readFirmwareBackup(hid, current, images.map(([, image]) => image),
    farImages, progress, () => backupCancelled);
  if (backupCancelled) throw new BackupStoppedError();
  const archive = await createHidArchive(recovered.accessible, recovered.method, recovered.far);
  // The protected boot bytes are a placeholder from a known image (they are
  // identical in all of them) and are never written through HID.
  const loaded = await loadHidArchive(archive, target?.image ?? stockSeed);
  // Classify the recovered bytes, so a sector CRC collision cannot label the
  // backup as a pinned release or select the wrong stock restore point.
  const match = images.find(([, image]) => image.length === IMAGE_SIZE &&
    recovered.accessible.every((byte, offset) => byte === image[APP_START + offset]));
  let source = match?.[0] ?? 'other';
  if (source === 'other' && stockSeed && stockCompatible(expectedCrcs(loaded.image), stockSeed))
    source = 'stock-like';
  if (backupCancelled) throw new BackupStoppedError();
  $('cancelBackup').disabled = true;
  const sourceName = source === 'build' || previousInfo(source)?.catalog || source === packagedBuild.label ? 'H9-Toolbox-Firmware-'
    : [earlierDevBuild.id, priorDevBuild.id].includes(source) ? 'H9-Toolbox-Earlier-Dev-'
      : source === 'stock-like' ? 'Stock-' : source === 'other' ? 'Unrecognised-' : previousInfo(source)?.file ?? '';
  const sourceLabel = source === 'build' ? BUILD.label : source === packagedBuild.label ? packagedBuild.label : previousInfo(source)?.label;
  const date = new Date().toISOString().slice(0, 10);
  const name = `TD-H9-${sourceName}HID-recovery-${date}.h9hid`;
  const url = URL.createObjectURL(new Blob([archive], { type: 'application/octet-stream' }));
  const link = document.createElement('a'); link.href = url; link.download = name; document.body.append(link);
  link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 60000);
  createdBackup = { archive, name };
  radioBackedUp = true;
  status(`${sourceLabel ? `Backup of the installed ${sourceLabel}` : 'Backup'} download started. Keep that file.`);
  // Choose the restore point the same way as for a selected file.
  await validateSelectedBackup(false);
  if (includeFar) status('Recovery backup saved with both firmware regions.');
  else if (source === packagedBuild.label) status('H9 Toolbox firmware backup saved. This file covers the main firmware only.');
  finish(`Backup built and verified in ${formatDuration((performance.now() - backupBegan) / 1000)}`);
}
// Check the radio and plan both directions: a full sector-CRC scan, the
// pinned sector set, the live shifted-window check, and any interrupted
// sector. Nothing is erased or written here.
async function inspectRadio(reference, known) {
  status('Checking the radio: scanning 251 sectors (CRC only, no writes)…');
  phase('Checking the radio (CRC scan, no writes)');
  const current = await scanRadio(hid, progress);
  if (reference.source === 'verified-firmware')
    return planFirmwareWithoutBackup(hid, current, target.image);
  const marker = pending();
  const resumeSector = marker?.packageHash === target.packageHash && marker?.backupHash === reference.hash ? marker.sector : null;
  const changed = planUpdate(current, reference.image, target.image, resumeSector, known);
  if (target.kind === 'build' && !reference.fullInstall && !changed.every(physical => release.sectors.includes(physical)))
    throw new Error(`${release.label} sector plan leaves its ${release.sectors.length} pinned sectors. No writes allowed.`);
  phase('Checking the radio (shifted CRC windows)');
  if (target.kind === 'build')
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
async function restartAfterVerification() {
  status('Firmware verified. Requesting a radio restart…');
  try {
    if (await hid.reboot()) {
      clearConnection(true);
      status('The radio left USB update mode after the restart request.');
      return 'Wait for the normal channel screen. If it stays blank, unplug USB and turn the radio off and back on.';
    }
    status('The radio stayed in USB update mode after the restart request.');
  } catch (error) {
    status(`Firmware verified. The restart request could not be completed. ${error.message}`);
  }
  return 'Unplug USB, then turn the radio off and back on to finish restarting.';
}

const stockRestart = ['Check the firmware version, channels, and settings.'];
const toolboxRestart = [
  useRc ? 'From the channel screen, press Menu to open Toolbox.' : 'From the channel screen, hold Back to open the app menu.',
  usePackagedRelease ? 'Open App Manager to install the matching apps.'
    : 'Use the App Manager over Bluetooth or experimental USB-C to add apps or a boot logo.',
];
function appManagerLink() {
  const catalog = target.combined?.catalog ?? (useLegacyDevBuild ? 'smartbeacon' : null);
  return { href: catalog ? `../apps/?catalog=${catalog}` : '../apps/', label: 'Open the App Manager' };
}

function aprsResult(state) {
  return state === 'ready' ? ' The optional APRS update is ready. Open the KISS TNC instructions below.' : '';
}

async function performWrite(direction) {
  if (direction === 'update' ? $('flash').disabled : $('restore').disabled) return;
  for (const id of ['backupHelp', 'writeHelp']) {
    $(id).hidden = true;
    $(id).open = false;
  }
  showResult(null);
  busy = true; refresh();
  let writingStarted = false;
  try {
    const toolbox = direction === 'update' && target.kind === 'build';
    const combined = target.combined ?? previewBuild;
    const installingRc = toolbox && Boolean(target.combined);
    const selectedFile = $('backupFile').files.length > 0;
    if (selectedFile && !backup)
      throw new Error('Select a valid recovery file or clear your file selection before installing.');
    const exactBackup = $('exactBackup').checked;
    let reference = direction === 'restore' || selectedFile || (radioBackedUp && exactBackup) ? backup : null;
    if (direction === 'update' && !selectedFile && exactBackup &&
        (!reference || (installingRc && !backedUpFar(reference.accessible)))) {
      await createBackup();
      if (!backup) throw new Error('The updater could not verify the recovery backup.');
      reference = backup;
      backupRunning = false;
      refresh();
    }
    if (installingRc && reference && !backedUpFar(reference.accessible))
      throw new Error('Select a backup with both firmware regions or clear your file selection.');
    const began = performance.now();
    const useVerifiedFirmware = !reference;
    if (useVerifiedFirmware) {
      reference = { image: target.image, hash: target.packageHash, source: 'verified-firmware' };
      status('Checking the radio for installation from the verified firmware package.');
    }
    const destination = direction === 'update' ? target.image : reference.image;
    const checked = await inspectRadio(reference, knownImages());
    const work = checked[direction];
    const farBackup = backedUpFar(reference.accessible);
    const restoringFar = useRc && direction === 'restore' && farBackup;
    let farPlan;
    function saveFarProgress(sector) {
      if (sector === null) localStorage.removeItem(farResumeKey);
      else localStorage.setItem(farResumeKey, JSON.stringify({ backupHash: reference.hash, packageHash: combined.packageSha256, sector }));
    }
    if (installingRc || restoringFar) {
      phase(useVerifiedFirmware ? 'Checking the far firmware' : 'Checking the far firmware against the recovery backup');
      const far = await loadReleaseFile(combined.farFile, combined.farSha256);
      const marker = pending(farResumeKey);
      const interrupted = marker?.backupHash === reference.hash && marker?.packageHash === combined.packageSha256 ? marker.sector : null;
      farPlan = useVerifiedFirmware ? await planFarFirmwareWithoutBackup(hid, far)
        : await planFarFirmware(hid, farBackup, far, direction, interrupted);
    }
    let schedulerStage, aprsStage;
    if (toolbox && useLegacyDevBuild) {
      const scheduler = await loadReleaseFile(packagedBuild.residentFile, packagedBuild.residentSha256);
      const image = new Uint8Array(RESIDENT_END - RESIDENT_START).fill(0xff);
      image.set(scheduler);
      schedulerStage = await prepareStage(image, RESIDENT_START, 'APRS scheduler area', hid.writeSmartBeaconResident.bind(hid));
      if (!schedulerStage.plan) throw new Error('The APRS scheduler area contains different data.');
    }
    if (toolbox && !installingRc && target.includeModem) {
      const bytes = await loadReleaseFile(release.aprsStageFile, release.aprsStageSha256);
      await loadAprsStage(bytes, release.aprsStageSha256);
      aprsStage = await prepareStage(bytes, APRS_STAGE_START, 'APRS staging area', hid.writeAprsStage.bind(hid));
    }
    const stages = [schedulerStage, aprsStage].filter(Boolean);
    const addresses = [...work, ...(farPlan?.work ?? []),
      ...stages.flatMap(stage => (stage.plan ?? []).map(entry => stage.start + entry.offset))];
    const signature = JSON.stringify([direction, target.packageHash, reference.hash, addresses, stages.map(stage => stage.plan)]);
    flashMap.show(addresses);
    if (reviewedPlan?.signature !== signature) {
      if (reviewedPlan?.direction === direction)
        status('The radio changed since the last scan. Review the updated flash plan.');
      reviewedPlan = { direction, signature };
      $('progress').style.width = '0%';
      $('progressText').textContent = addresses.length ?
        `${addresses.length} sectors to write. Review the map, then confirm.` :
        'All selected firmware sectors match.';
      $('flashMapTitle').focus({ preventScroll: true });
      $('flashMapTitle').scrollIntoView({ block: 'nearest' });
      return;
    }
    reviewedPlan = null;
    writingStarted = true;
    if (installingRc) {
      phase('Writing and verifying the far firmware');
      await writeFarFirmware(hid, farPlan, saveFarProgress, progress, flashMap.mark);
    }
    if (schedulerStage) await writePreparedStage(schedulerStage);
    let aprs = installingRc && target.includeModem ? 'ready' : null;
    if (aprsStage) aprs = await writePreparedStage(aprsStage);
    $('planState').textContent = `Radio checked: ${work.length} sector(s) to ${direction === 'update' ? 'write' : 'restore'}${checked.interrupted ? ', including one interrupted sector' : ''}.`;
    $('unlock-kiss').hidden = aprs !== 'ready';
    $('unlock-kiss').open = false;
    $('kissGate').hidden = aprs !== 'occupied';
    if (restoringFar && !work.length) {
      phase('Restoring and verifying the far firmware');
      await writeFarFirmware(hid, farPlan, saveFarProgress, progress, flashMap.mark);
    }
    if (!work.length) {
      const farChanged = Boolean(farPlan?.work.length);
      finish('Main firmware already matches');
      $('acknowledge').checked = false;
      status('Every main firmware sector already matches.');
      const restart = await restartAfterVerification();
      showResult({
        title: toolbox
          ? `${target.combined?.label ?? release.label} ${farChanged ? 'installed and verified' : 'is already installed'}`
          : farChanged ? 'Backup restored and verified' : 'The radio already matches the selected firmware',
        text: (farPlan ? 'Both firmware regions passed verification.' : 'Every firmware sector matched during verification.') + aprsResult(aprs),
        steps: [restart, ...(toolbox ? toolboxRestart : stockRestart)],
        action: toolbox ? appManagerLink() : null,
      });
      return;
    }
    if (direction === 'update' && reference.fullInstall && release.sectors && work.length > release.sectors.length)
      status(`Full firmware install: this radio is not on stock v1.0.38, so ${work.length} of ${(IMAGE_SIZE - APP_START) / SECTOR_SIZE} firmware sectors differ and are rewritten (the rest already match). This takes longer. Keep USB connected and the radio powered on; if it is interrupted, follow the recovery guidance on this step.`);
    status(`Radio checked. ${direction === 'update' ? 'Writing' : 'Restoring'} ${work.length} sectors. Keep USB connected and the radio powered on.`);
    phase((done, total) => done >= total ? `${total} sectors written and verified` : `Writing and verifying sector ${done + 1} of ${total}`);
    progress(0, work.length);
    for (let index = 0; index < work.length; index++) {
      const physical = work[index];
      localStorage.setItem(resumeKey, JSON.stringify({ packageHash: target.packageHash, backupHash: reference.hash, sector: physical, direction }));
      flashMap.mark(physical, 'writing');
      await writeSector(hid, physical, destination);
      flashMap.mark(physical, 'verified');
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
    if (checked.verifyShiftedWindows && !await knownAppMatches(hid, after, destination))
      throw new Error('The final shifted-window check found firmware bytes that differ from the selected package.');
    if (restoringFar) {
      phase('Restoring and verifying the far firmware');
      await writeFarFirmware(hid, farPlan, saveFarProgress, progress, flashMap.mark);
    }
    $('acknowledge').checked = false;
    const seconds = formatDuration((performance.now() - began) / 1000);
    finish(`${work.length} sectors ${direction === 'update' ? 'installed' : 'restored'} and every sector CRC verified in ${seconds}`);
    const restart = await restartAfterVerification();
    showResult({
      title: toolbox ? `${target.combined?.label ?? release.label} installed and verified`
        : direction === 'update' ? 'Stock firmware installed and verified'
          : 'Backup restored and verified',
      text: `Updated ${work.length + (farPlan?.work.length ?? 0)} sector(s) and verified ${farPlan ? 'both firmware regions' : 'every firmware sector'} (${seconds}).` + aprsResult(aprs),
      steps: [restart, ...(toolbox ? toolboxRestart : stockRestart)],
      action: toolbox ? appManagerLink() : null,
    });
  } catch (error) {
    reviewedPlan = null;
    if (writingStarted) {
      flashMap.interrupt();
      if (device && !device.opened) clearConnection(true);
    } else flashMap.clear();
    if (backupRunning) $('backupProgress').hidden = true;
    if (error instanceof BackupStoppedError && backupCancelled) {
      status('Backup cancelled. No radio write was sent.');
    } else {
      fail(error);
      showResult({
        failed: true,
        title: backupRunning ? 'Recovery backup did not finish'
          : error instanceof UnverifiedRadioError ? 'Radio model could not be verified' : 'Firmware verification did not finish',
        text: error?.message || String(error),
      });
    }
  } finally {
    busy = false; backupRunning = false; backupCancelled = false; refresh();
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
