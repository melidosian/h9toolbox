import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import test from 'node:test';
import {
  CHIP_SLOT, HEADER, ENTRY, RAM_SLOT, buildBundle, checkModule, layout, parseBundle, usage,
} from '../dist/apps/bundle.mjs';
import { describeChanges } from '../dist/apps/changes.mjs';
import { SLOTS, readPack, uploadPack } from '../dist/doom/upload.mjs';
import { crc16 } from '../dist/boot-logo/resource.mjs';
import { SimulatedRadio } from './support/simulated-radio.mjs';

const body = (n, seed = 1) => Uint8Array.from({ length: n }, (_, i) => (i * 37 + seed) & 255);
const mod = (name, n, extra = {}) => ({ name, body: body(n, name.length), bss: 0, entry: 0, ...extra });
const MENU = mod('MENU', 1853, { entry: 0x268 });
const BB = mod('BRICK BREAKER', 1628, { entry: 0x52 });
const ABOUT = mod('ABOUT', 1590);

test('bundle layout matches the loader stub and build.py', () => {
  const bytes = buildBundle([MENU, BB]);
  const view = new DataView(bytes.buffer);
  assert.equal(new TextDecoder().decode(bytes.subarray(0, 8)), 'H9MODS01');
  assert.equal(view.getUint16(8, true), 1);
  assert.equal(view.getUint16(10, true), 2);
  assert.equal(view.getUint32(12, true), bytes.length);
  const first = HEADER + ENTRY * 2;                   // bodies follow the entries
  assert.equal(view.getUint32(HEADER + 16, true), first);
  assert.equal(view.getUint32(HEADER + ENTRY + 16, true) % 4, 0);  // 4-byte aligned bodies
  assert.equal(view.getUint16(HEADER + 30, true), crc16(MENU.body));
  assert.equal(crc16(new TextEncoder().encode('123456789')), 0x31c3);  // XMODEM, as the stub
  assert.equal(layout([MENU, BB]).bytes, bytes.length);
});

test('round trip and corruption', () => {
  const bytes = buildBundle([MENU, BB, ABOUT]);
  const back = parseBundle(bytes);
  assert.deepEqual(back.map(m => [m.name, m.entry, m.body.length]), [['MENU', 0x268, 1853], ['BRICK BREAKER', 0x52, 1628], ['ABOUT', 0, 1590]]);
  bytes[bytes.length - 1] ^= 1;
  assert.throws(() => parseBundle(bytes), /CRC/);
  assert.throws(() => parseBundle(new Uint8Array(64)), /Not an H9MODS01/);
});

test('memory limits: RAM slot, chip slot, names', () => {
  assert.deepEqual(checkModule(mod('OK', RAM_SLOT)).errors, []);
  assert.match(checkModule(mod('BIG', RAM_SLOT, { bss: 1 })).errors.join(), /RAM/);
  assert.match(checkModule(mod('X', 10, { entry: 10 })).errors.join(), /entry/);
  assert.match(checkModule(mod('A NAME LONGER THAN 16', 10)).errors.join(), /name/);
  assert.match(checkModule(mod('TETRIS_2', 10)).warnings.join(), /"_"/);
  const many = [MENU, ...Array.from({ length: 16 }, (_, i) => mod(`APP ${i}`, 8000))];
  assert.throws(() => buildBundle(many), /larger than/);
  const u = usage([MENU, BB]);
  assert.equal(u.chipTotal, CHIP_SLOT);
  assert.equal(u.largestRam, 1853);
  assert.equal(u.listed, 1);
});

test('describeChanges: add, remove, update, reorder, first install', () => {
  const installed = parseBundle(buildBundle([MENU, BB]));
  assert.equal(describeChanges(installed, installed).any, false);
  assert.deepEqual(describeChanges(installed, [...installed, ABOUT]).added, ['ABOUT']);
  assert.deepEqual(describeChanges(installed, [installed[0]]).removed, ['BRICK BREAKER']);
  const newer = { ...installed[1], body: body(1700, 9) };
  assert.deepEqual(describeChanges(installed, [installed[0], newer]).updated, ['BRICK BREAKER']);
  const three = parseBundle(buildBundle([MENU, BB, ABOUT]));
  assert.equal(describeChanges(three, [three[0], three[2], three[1]]).reordered, true);
  assert.match(describeChanges([], three).lines.join(), /install 3 module/);
});

test('a bundle written to the modules slot reads back for the manager', async () => {
  const sim = new SimulatedRadio();
  const empty = await readPack(sim.radio, SLOTS.modules);
  assert.equal(empty.pack, null);
  const bytes = buildBundle([MENU, BB, ABOUT]);
  await uploadPack(sim.radio, bytes, { name: 'H9 modules', slot: SLOTS.modules });
  const { dir, pack } = await readPack(sim.radio, SLOTS.modules);
  assert.equal(dir.complete, true);
  assert.deepEqual(pack, bytes);
  assert.deepEqual(parseBundle(pack).map(m => m.name), ['MENU', 'BRICK BREAKER', 'ABOUT']);
  // remove a module and write again: the slot then holds the smaller bundle
  const smaller = buildBundle([MENU, ABOUT]);
  await uploadPack(sim.radio, smaller, { name: 'H9 modules', slot: SLOTS.modules });
  assert.deepEqual((await readPack(sim.radio, SLOTS.modules)).pack, smaller);
});

const built = new URL('../firmware-research/module-loader/build/h9_modules.bin', import.meta.url);
test('the Python-built bundle parses here (when built)', { skip: !existsSync(built) }, () => {
  const modules = parseBundle(new Uint8Array(readFileSync(built)));
  assert.equal(modules[0].name, 'MENU');
  assert.deepEqual(parseBundle(buildBundle(modules)).map(m => m.crc), modules.map(m => m.crc));
});

// ---- catalog (dist/apps/catalog.mjs, catalog/ written by publish_catalog.py)
import { appState, fetchApp, fetchModule, loadCatalog, menuOutdated, partsOf, withModule, withModules, withoutApp, withoutModule } from '../dist/apps/catalog.mjs';

const catalogDir = new URL('../dist/apps/catalog/', import.meta.url);
const fileFetch = async url => {
  const path = new URL(url);
  if (!existsSync(path)) return { ok: false, status: 404 };
  const bytes = readFileSync(path);
  return { ok: true, status: 200, json: async () => JSON.parse(bytes), arrayBuffer: async () => bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.length) };
};

test('published catalog: every file matches its entry', async () => {
  const catalog = await loadCatalog(catalogDir, fileFetch);
  assert.ok(catalog.apps.some(a => a.name === 'BRICK BREAKER'));
  for (const item of [catalog.menu, ...catalog.apps]) {
    const modules = await fetchApp(catalog, item, fileFetch);
    assert.equal(modules[0].name, item.name);
    assert.deepEqual(modules.map(m => m.name), partsOf(item).map(p => p.name));
    assert.equal(modules.reduce((n, m) => n + m.body.length, 0), item.bytes);
    for (const m of modules) assert.deepEqual(checkModule(m).errors, []);
  }
  const names = catalog.apps.flatMap(a => partsOf(a).map(p => p.name));
  assert.equal(new Set(names).size, names.length);
  const aprs = catalog.apps.find(a => a.id === 'aprs');
  assert.deepEqual(partsOf(aprs).map(p => p.name), ['APRS', '.APRS INFO', 'APRS MAP', 'APRS MSG', 'APRS SEND']);
  assert.ok(buildBundle([await fetchModule(catalog, catalog.menu, fileFetch), ...await fetchApp(catalog, aprs, fileFetch)]).length < CHIP_SLOT);
});

test('a tampered catalog download is refused', async () => {
  const catalog = await loadCatalog(catalogDir, fileFetch);
  const item = { ...catalog.apps[0], sha256: '0'.repeat(64) };
  await assert.rejects(fetchModule(catalog, item, fileFetch), /does not match/);
});

test('catalog add/remove/update states and the menu on first add', () => {
  const item = { name: 'BRICK BREAKER', crc16: crc16(BB.body).toString(16).padStart(4, '0') };
  const menu = parseBundle(buildBundle([MENU]))[0];
  const bb = parseBundle(buildBundle([BB]))[0];
  assert.equal(appState(item, null, []).status, 'available');
  const first = withModule([], bb, menu);                       // first app brings the menu
  assert.deepEqual(first.map(m => m.name), ['MENU', 'BRICK BREAKER']);
  assert.throws(() => withModule([], bb, null), /menu/);
  assert.equal(appState(item, null, first).status, 'adding');
  assert.equal(appState(item, first, first).status, 'installed');
  assert.equal(appState(item, first, withoutModule(first, 'BRICK BREAKER')).status, 'removing');
  const older = { ...bb, body: body(100, 3) };
  const onRadio = [menu, older];
  assert.equal(appState(item, onRadio, onRadio).status, 'outdated');
  assert.equal(appState(item, onRadio, withModule(onRadio, bb)).status, 'updating');
  assert.equal(withModule(first, bb).length, 2);                 // same name replaces
  assert.equal(menuOutdated({ menu: { crc16: '0000' } }, first), true);
  assert.equal(menuOutdated({ menu: { crc16: crc16(menu.body).toString(16).padStart(4, '0') } }, first), false);
});

test('multi-module app: added, updated and removed together', () => {
  const menu = parseBundle(buildBundle([MENU]))[0];
  const parts = parseBundle(buildBundle([mod('APRS', 300), mod('.APRS INFO', 200), mod('APRS MAP', 250)]));
  const hex = m => crc16(m.body).toString(16).padStart(4, '0');
  const item = { name: 'APRS', modules: parts.map(m => ({ name: m.name, crc16: hex(m) })) };
  assert.equal(appState(item, null, []).status, 'available');
  const staged = withModules([], parts, menu);
  assert.deepEqual(staged.map(m => m.name), ['MENU', 'APRS', '.APRS INFO', 'APRS MAP']);
  assert.equal(appState(item, null, staged).status, 'adding');
  assert.equal(appState(item, staged, staged).status, 'installed');
  // one part on the radio differs from the catalog: update offered
  const old = staged.map(m => (m.name === 'APRS MAP' ? { ...m, body: body(90, 4) } : m));
  assert.equal(appState(item, old, old).status, 'outdated');
  assert.equal(appState(item, old, withModules(old, parts)).status, 'updating');
  // a part missing (e.g. installed before it existed) also counts as outdated
  const partial = old.filter(m => m.name !== '.APRS INFO');
  assert.equal(appState(item, partial, partial).status, 'outdated');
  assert.deepEqual(withoutApp(staged, item).map(m => m.name), ['MENU']);
  assert.equal(appState(item, staged, withoutApp(staged, item)).status, 'removing');
});
