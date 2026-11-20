// H9 Toolbox App Manager: stage module changes, then write one verified
// bundle to the modules slot over Bluetooth or USB-C (dist/doom/upload.mjs).
import { crc16 } from '../boot-logo/resource.mjs';
import { connectBluetooth, connectUsb } from '../boot-logo/radio.mjs?v=usb3';
import { mountLogoTool } from '../boot-logo/logo-tool.mjs?v=usb3';
import { SLOTS, readPack, restore, uploadPack } from '../doom/upload.mjs?v=usb1';
import { CHIP_SLOT, buildBundle, checkModule, parseBundle, usage } from './bundle.mjs?v=am3';
import { describeChanges } from './changes.mjs?v=am3';
import { appState, fetchApp, loadCatalog, menuState, partsOf, selectAppCatalog, withApp, withModule, withModules, withUpdatedMenu, withoutApp } from './catalog.mjs?v=am6';
import { mountInfo } from './info.mjs?v=am5';

// No stale files: the service worker (sw.js) fetches everything uncached.
if ('serviceWorker' in navigator) navigator.serviceWorker.register('./sw.js', { scope: './', updateViaCache: 'none' }).catch(() => {});
const $ = id => document.getElementById(id);
const BUNDLE_NAME = 'H9 modules';
let session = null, busy = false, connecting = false, logoBusy = false;
let logoTool = null;    // the Boot logo section (boot-logo/logo-tool.mjs) on this page's link
let installed = null;   // modules on the radio (null: not read / none)
let staged = [];        // the list "Write to radio" will install
let catalog = null;     // catalog/catalog.json
const catalogs = { classic: null, bench: null };
const benchLink = $('smartbeaconCatalogLink');
const preferBench = Boolean(benchLink && new URLSearchParams(location.search).get('catalog') === 'smartbeacon');
const downloaded = new Map();  // catalog SHA-256 -> modules

async function catalogApp(item) {
  if (!downloaded.has(item.sha256)) downloaded.set(item.sha256, await fetchApp(catalog, item));
  return downloaded.get(item.sha256);
}
const catalogModule = async item => (await catalogApp(item))[0];

// Stage a whole catalog app (with the menu when it is the first one). Apps with refreshable data
// (SAT PASS) get fresh data in place of the built-in default unless `data` is given or the fetch fails.
async function stageApp(item, data, update = false) {
  const menu = staged.length ? null : await catalogModule(catalog.menu);
  let modules = await catalogApp(item);
  const refreshed = data ?? (item.config === 'satellites' ? await infoDialog.fresh(item) : null);
  if (refreshed) modules = modules.map(m => (m.name === refreshed.name ? refreshed : m));
  staged = refreshed ? withModules(staged, modules, menu) : withApp(staged, modules, menu, item);
  log(`Staged: ${update ? 'update' : 'add'} ${item.title}${menu ? ' (and the menu)' : ''}${refreshed ? ' with fresh satellite data' : ''}.`);
}

const infoDialog = mountInfo({
  dialog: $('appInfo'),
  catalog: () => catalog,
  staged: () => staged,
  installed: () => installed ?? [],
  stage: modules => { staged = modules.reduce((list, m) => withModule(list, m), staged); render(); },
  addApp: async (item, data) => { await stageApp(item, data); render(); },
  log: (message, level) => log(message, level),
  busy: () => busy,
});

function renderCatalog() {
  const box = $('catalog');
  const bench = catalog?.profile === 'smartbeacon';
  if (benchLink) benchLink.hidden = bench;
  const menu = catalog && menuState(catalog, staged, preferBench);
  $('menuUpdate').hidden = !menu?.canUpdate;
  $('stageMenuUpdate').disabled = busy;
  if (menu?.canUpdate) {
    let label = bench ? 'Update menu and apps' : 'Update menu';
    if (menu.status === 'different') label = 'Use matching menu and apps';
    $('stageMenuUpdate').textContent = label;
  }
  if (!catalog) { box.replaceChildren(); return; }
  const cards = catalog.apps.map(item => {
    const state = appState(item, installed, staged);
    const card = document.createElement('div');
    card.className = `app${staged.some((m, i) => i > 0 && m.name === item.name) ? ' staged' : ''}`;
    card.innerHTML = '<h3><span></span><span class="tag"></span></h3><p class="summary"></p><div class="facts"></div><div class="row"><span class="status"></span><span class="buttons"></span></div>';
    card.querySelector('h3 span').textContent = item.title;
    card.querySelector('.tag').textContent = item.category ?? 'App';
    card.querySelector('.summary').textContent = item.summary ?? item.description;
    const parts = partsOf(item).length;
    card.querySelector('.facts').textContent = `v${item.version} · ${parts > 1 ? `${parts} modules · ` : ''}${kib(item.bytes)}`;
    const status = card.querySelector('.status');
    status.className = `status ${state.status}`;
    status.textContent = state.label;
    const buttons = card.querySelector('.buttons');
    const button = (label, act, primary = false) => {
      const b = document.createElement('button');
      b.textContent = label; b.disabled = busy; if (primary) b.className = 'primary';
      b.addEventListener('click', () => run(act));
      buttons.append(b);
    };
    const info = document.createElement('button');
    info.textContent = item.config ? 'Info & settings' : 'Info'; info.className = 'info';
    info.addEventListener('click', () => infoDialog.open(item));
    buttons.append(info);
    if (state.action === 'update') button('Update', () => stageApp(item, null, true), true);
    if (state.action === 'add') button('Add', () => stageApp(item), true);
    else button('Remove', async () => { staged = withoutApp(staged, item); log(`Staged: remove ${item.title}.`); });
    return card;
  });
  box.replaceChildren(...cards);
  $('catalogNote').textContent = bench
    ? 'Requires the dev build of H9 Toolbox Firmware v0.1.5.'
    : `${catalog.apps.length} app(s) available. Add or remove them here, then Write to radio.`;
}

function log(message, level = 'info') {
  $('log').textContent = `${new Date().toLocaleTimeString()} ${message}\n` + $('log').textContent;
  (level === 'error' ? console.error : console.info)('[app-manager]', message);
}
const kib = n => `${(n / 1024).toFixed(1)} KiB`;
const hex4 = n => n.toString(16).padStart(4, '0');

function meter(id, used, total, label) {
  $(`${id}Text`).textContent = label;
  $(`${id}Bar`).firstElementChild.style.width = `${Math.min(100, (used / total) * 100)}%`;
  $(`${id}Bar`).classList.toggle('full', used > total);
}

function render() {
  const connected = Boolean(session);
  logoTool?.refresh();
  const changes = describeChanges(installed ?? [], staged);
  let buildError = null;
  try { if (staged.length) buildBundle(staged); } catch (error) { buildError = error.message; }

  $('disconnect').disabled = busy || !connected;
  $('reload').disabled = busy || !connected;
  $('apply').disabled = busy || !connected || !staged.length || !changes.any || Boolean(buildError);
  $('discard').disabled = busy || !changes.any;
  $('removeAll').disabled = busy || !connected || !installed;
  $('exportBundle').disabled = !staged.length || Boolean(buildError);
  $('addButton').disabled = busy || (!staged.length && !catalog);
  $('bundleButton').disabled = busy;
  // Keep both connection buttons disabled until the first read completes.
  const stateText = connecting ? (session ? `Connecting · ${session.name} · reading the radio…` : 'Connecting…')
    : connected ? `Connected · ${session.name}` : 'Disconnected';
  $('state').classList.toggle('connecting', connecting);
  $('state').textContent = stateText;
  $('connect').disabled = busy || connected || connecting;
  $('connectUsb').disabled = $('connect').disabled;

  const u = usage(staged);
  meter('chip', u.chipUsed, u.chipTotal, staged.length ? `${kib(u.chipUsed)} of ${kib(u.chipTotal)} (${kib(Math.max(0, u.chipFree))} free)` : '–');
  meter('ram', u.largestRam, u.ramTotal, staged.length ? `largest ${u.largestRam.toLocaleString()} of ${u.ramTotal.toLocaleString()} bytes` : '–');
  meter('list', u.listed, u.listMax, staged.length ? `${u.listed} of ${u.listMax}` : '–');

  $('summary').textContent = installed
    ? `On the radio: ${installed.length - 1} module(s) plus the menu.`
    : connected ? 'No modules on the radio yet: load a bundle (with the menu) to install.' : 'Not connected.';

  const rows = staged.map((m, i) => {
    const tr = document.createElement('tr');
    const { errors, warnings } = checkModule(m);
    const before = installed?.find(x => x.name === m.name);
    const status = !installed || !before ? 'new' : before.crc !== m.crc || before.bss !== m.bss || before.entry !== m.entry || before.category !== m.category ? 'changed' : '';
    const cells = [
      String(i),
      null,
      `${m.body.length.toLocaleString()} B`,
      `${(m.body.length + m.bss).toLocaleString()} B`,
      hex4(m.crc ?? crc16(m.body)),
      null,
      null,
    ];
    cells.forEach((text, c) => {
      const td = document.createElement('td');
      if (c >= 2 && c <= 4) td.className = 'num';
      if (text !== null) td.textContent = text;
      tr.append(td);
    });
    const [, nameCell, , , , statusCell, actions] = tr.children;
    if (i === 0) {
      nameCell.innerHTML = `<b>${m.name}</b> <span class="tag">system menu</span>`;
    } else {
      const input = document.createElement('input');
      input.type = 'text'; input.value = m.name; input.maxLength = 16; input.disabled = busy;
      input.addEventListener('change', () => {
        const name = input.value.trim();
        if (!name || staged.some((x, j) => j !== i && x.name === name)) { log(`"${name}" is empty or already used.`, 'error'); render(); return; }
        staged[i] = { ...m, name }; render();
      });
      nameCell.append(input);
    }
    const notes = [...errors.map(e => `<span class="bad">${e}</span>`), ...warnings.map(w => `<span class="warn">${w}</span>`)];
    statusCell.innerHTML = (status ? `<span class="tag ${status}">${status}</span> ` : '') + notes.join('<br>');
    if (i > 0) {
      const button = (label, title, disabled, act) => {
        const b = document.createElement('button');
        b.className = 'small'; b.textContent = label; b.title = title; b.disabled = busy || disabled;
        b.addEventListener('click', () => { act(); render(); });
        actions.append(b);
      };
      button('↑', 'Move up in the menu', i === 1, () => { [staged[i - 1], staged[i]] = [staged[i], staged[i - 1]]; });
      button('↓', 'Move down in the menu', i === staged.length - 1, () => { [staged[i + 1], staged[i]] = [staged[i], staged[i + 1]]; });
      button('Remove', 'Remove from the radio (on Write)', false, () => { staged.splice(i, 1); log(`Staged: remove ${m.name}.`); });
    }
    return tr;
  });
  for (const gone of changes.removed) {
    const tr = document.createElement('tr');
    tr.className = 'removed';
    tr.innerHTML = '<td>–</td><td></td><td></td><td></td><td></td><td><span class="tag">removed</span></td><td></td>';
    tr.children[1].textContent = gone;
    rows.push(tr);
  }
  $('modules').replaceChildren(...rows);

  renderCatalog();
  $('pending').className = buildError ? 'bad' : 'note';
  $('pending').textContent = buildError ?? (changes.any ? `Pending: ${changes.lines.join('; ')}.` : 'No changes.');
}

async function run(task) {
  if (busy) return;
  busy = true; render();
  try { await task(); } catch (error) { log(error.message, 'error'); $('result').textContent = `Stopped: ${error.message}`; }
  finally { busy = false; if (session) session.radio.stopRequested = false; render(); }
}
function progress(began) {
  return (fraction, phase, rate) => {
    $('progress').value = fraction;
    const elapsed = (performance.now() - began) / 1000;
    const left = fraction > 0.03 ? ` · about ${Math.ceil(elapsed * (1 - fraction) / fraction)} s left` : '';
    $('progressText').textContent = `${Math.floor(fraction * 100)}% · ${phase}${rate ? ` · writing ${(rate / 1024).toFixed(2)} KiB/s` : ''}${left}`;
  };
}

// Fetch the app catalog again (uncached) so every read of the radio compares it with the newest apps.
function pickCatalog() {
  catalog = selectAppCatalog(catalogs.classic, catalogs.bench, session ? (installed ?? []) : staged, preferBench);
}

async function refreshCatalog() {
  const sources = [['classic', undefined]];
  if (benchLink) sources.push(['bench', new URL('catalog/smartbeacon/', import.meta.url)]);
  await Promise.all(sources.map(async ([key, base]) => {
    try { catalogs[key] = await loadCatalog(base); }
    catch (error) { log(error.message, 'error'); }
  }));
  pickCatalog();
  if (!catalog) $('catalogNote').textContent = preferBench ? 'The development app catalog did not load.' : 'The app catalog did not load.';
}
// After reading the radio: which installed apps have a newer version in the catalog.
function reportUpdates() {
  if (!catalog || !installed) return;
  const outdated = catalog.apps.filter(item => {
    const here = (installed ?? []).slice(1).some(m => m.name === item.name);
    return here && appState(item, installed, installed).status === 'outdated';
  }).map(item => `${item.title} ${item.version}`);
  const menu = menuState(catalog, installed, preferBench);
  if (menu.status === 'outdated') outdated.unshift(`${catalog.menu.title} ${catalog.menu.version}`);
  if (outdated.length) log(`Updates available for ${outdated.join(', ')}.`);
  if (menu.status === 'different') {
    log(menu.canUpdate
      ? 'A different Toolbox menu is installed. Select Use matching menu and apps to stage the version for the dev firmware.'
      : 'The installed Toolbox menu differs from this catalog.');
  } else if (!outdated.length) {
    log('The Toolbox menu and installed catalog apps are up to date.');
  }
}

async function readRadio() {
  const opened = session;
  await refreshCatalog();
  $('progressText').textContent = 'Reading the modules slot…';
  const { dir, pack } = await readPack(opened.radio, SLOTS.modules, progress(performance.now()));
  if (!pack) {
    installed = null;
    pickCatalog();
    if (dir) log(`The modules slot holds an unfinished upload ("${dir.name}"); writing will replace it.`, 'error');
    else log('The modules slot is empty.');
    $('progressText').textContent = 'Idle.';
    return;
  }
  installed = parseBundle(pack);
  staged = installed.map(m => ({ ...m }));
  pickCatalog();
  log(`Read ${installed.length} module(s), ${pack.length.toLocaleString()} bytes: ${installed.map(m => m.name).join(', ')}.`);
  reportUpdates();
  $('progressText').textContent = 'Idle.';
}

async function readFile(file) {
  const bytes = new Uint8Array(await file.arrayBuffer());
  return parseBundle(bytes);
}

const startConnection = connect => run(async () => {
  $('result').textContent = '';
  connecting = true; render();
  let opened;
  try {
    opened = await connect({
      log: (message, level) => { if (level !== 'debug') log(message, level); },
      onDisconnect: () => {
        if (session !== opened) return;
        session = null;
        log('Radio disconnected.');
        render();
      },
    });
    session = opened;
    const service = await opened.radio.checkService();
    if (service.version !== 2) throw new Error('This radio does not run H9 Toolbox Firmware (write service missing).');
    if (!service.ready) throw new Error('The resource flash is still starting. Power-cycle the radio and connect again.');
    log(`Connected to ${opened.name}. Resource flash ready.`);
    logoTool?.reset();
    render();
    await readRadio();
  } catch (error) {
    session = null;
    await opened?.disconnect();
    if (!opened && error.name === 'NotFoundError') {
      log('No radio selected.');
      return;
    }
    throw error;
  } finally {
    connecting = false;
  }
});
$('connect').addEventListener('click', () => startConnection(connectBluetooth));
$('connectUsb').addEventListener('click', () => startConnection(connectUsb));
$('disconnect').addEventListener('click', () => run(async () => {
  const opened = session;
  session = null;
  await opened?.disconnect();
}));
$('reload').addEventListener('click', () => run(readRadio));

$('stageMenuUpdate').addEventListener('click', () => run(async () => {
  staged = await withUpdatedMenu(catalog, staged, { restoreBench: preferBench });
  log('Staged the menu update.');
}));

$('addButton').addEventListener('click', () => $('addFile').click());
$('addFile').addEventListener('change', () => run(async () => {
  if (!staged.length && !catalog) throw new Error('Load a whole bundle or choose an available catalog first.');
  for (const file of $('addFile').files) {
    const modules = await readFile(file);
    if (modules.length !== 1) throw new Error(`${file.name} holds ${modules.length} modules; use Load whole bundle for bundles.`);
    const [m] = modules;
    const update = staged.some((x, i) => i > 0 && x.name === m.name);
    const menu = staged.length ? null : await catalogModule(catalog.menu);
    staged = withModule(staged, m, menu);
    log(`Staged: ${update ? 'update' : 'add'} ${m.name} (${m.body.length.toLocaleString()} bytes) from ${file.name}${menu ? ', with the menu' : ''}.`);
  }
  $('addFile').value = '';
}));
$('bundleButton').addEventListener('click', () => $('bundleFile').click());
$('bundleFile').addEventListener('change', () => run(async () => {
  const file = $('bundleFile').files[0];
  if (!file) return;
  staged = await readFile(file);
  pickCatalog();
  log(`Staged: whole bundle from ${file.name}: ${staged.map(m => m.name).join(', ')}.`);
  $('bundleFile').value = '';
}));
$('exportBundle').addEventListener('click', () => {
  const link = document.createElement('a');
  link.href = URL.createObjectURL(new Blob([buildBundle(staged)]));
  link.download = 'h9_modules.bin';
  document.body.append(link); link.click(); link.remove();
});

$('discard').addEventListener('click', () => { staged = installed ? installed.map(m => ({ ...m })) : []; pickCatalog(); log('Changes discarded.'); render(); });
$('apply').addEventListener('click', () => run(async () => {
  const bundle = buildBundle(staged);
  if (bundle.length > CHIP_SLOT) throw new Error('The bundle does not fit the modules slot.');
  $('result').textContent = '';
  const manifest = await uploadPack(session.radio, bundle, { name: BUNDLE_NAME, slot: SLOTS.modules, onProgress: progress(performance.now()) });
  $('result').innerHTML = `<span class="ok">Written and verified.</span> ${manifest.bytes.toLocaleString()} bytes, ${manifest.sectorsWritten} sector(s), ${manifest.totalSeconds.toFixed(0)} s. Hold Back on the radio for the menu.`;
  log(`Bundle written: ${staged.map(m => m.name).join(', ')} (${manifest.bytes} bytes).`);
  await readRadio();
}));
$('removeAll').addEventListener('click', () => run(async () => {
  const result = await restore(session.radio, { slot: SLOTS.modules, onProgress: progress(performance.now()) });
  $('result').textContent = result.note ?? `Removed: ${result.erased} sector(s) erased and read back as erased. Holding Back now does nothing.`;
  log($('result').textContent);
  installed = null;
  staged = [];
  pickCatalog();
}));

render();
refreshCatalog().then(render);

// Boot logo: the shared logo tool on this page's radio connection. While it
// writes, the module controls are busy, and it waits while they are.
logoTool = mountLogoTool($('logoTool'), {
  onStatus: (message, error) => log(message, error ? 'error' : undefined),
  connection: {
    session: () => (session ? { radio: session.radio, name: session.name } : null),
    onBusy: flag => { logoBusy = flag; busy = flag; render(); },
    blocked: () => busy && !logoBusy,
  },
});
render();
