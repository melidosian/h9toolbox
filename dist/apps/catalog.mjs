// The App Manager's catalog (catalog/catalog.json, written by
// firmware-research/module-loader/publish_catalog.py): apps you can add or
// remove, plus the menu (module 0), which is installed with the first app.
import { crc16 } from '../boot-logo/resource.mjs';
import { parseBundle } from './bundle.mjs?v=am3';

const hex = bytes => [...bytes].map(b => b.toString(16).padStart(2, '0')).join('');

export async function loadCatalog(base = new URL('catalog/', import.meta.url), fetcher = fetch) {
  const response = await fetcher(new URL('catalog.json', base), { cache: 'no-store' });
  if (!response.ok) throw new Error(`The app catalog did not load (${response.status}).`);
  const catalog = await response.json();
  if (catalog.format !== 1 || !catalog.menu || !Array.isArray(catalog.apps)) throw new Error('Unknown app catalog format.');
  return { ...catalog, base: String(base) };
}

// The modules a catalog item installs (main first); older catalogs list one.
export const partsOf = item => item.modules ?? [{ name: item.name, crc16: item.crc16, bytes: item.bytes }];

// Download one catalog item's .h9mod and check it against the catalog;
// returns its modules, main first.
export async function fetchApp(catalog, item, fetcher = fetch) {
  const response = await fetcher(new URL(item.file, catalog.base), { cache: 'no-store' });
  if (!response.ok) throw new Error(`${item.title} did not download (${response.status}).`);
  const bytes = new Uint8Array(await response.arrayBuffer());
  const digest = hex(new Uint8Array(await globalThis.crypto.subtle.digest('SHA-256', bytes)));
  if (digest !== item.sha256) throw new Error(`${item.title}: the download does not match the catalog (SHA-256).`);
  const modules = parseBundle(bytes);
  const parts = partsOf(item);
  if (modules.length !== parts.length || modules.some((m, i) => m.name !== parts[i].name)) throw new Error(`${item.title}: unexpected module file.`);
  if (modules.some((m, i) => crcOf(m) !== parts[i].crc16)) throw new Error(`${item.title}: CRC differs from the catalog.`);
  return modules;
}

// One-module items (the menu).
export async function fetchModule(catalog, item, fetcher = fetch) {
  return (await fetchApp(catalog, item, fetcher))[0];
}

function crcOf(m) { return crc16(m.body).toString(16).padStart(4, '0'); }

export function selectAppCatalog(classic, bench, modules, preferBench = false) {
  if (preferBench) return bench;
  if (!bench || !modules.length) return classic;
  const match = part => part && modules.some(m => m.name === part.name && m.body.length === part.bytes && crcOf(m) === part.crc16);
  const beacon = bench.apps.flatMap(partsOf).find(part => part.name === 'APRS BEACON');
  return match(bench.menu) || match(beacon) ? bench : classic;
}

// Where a catalog app stands, comparing the radio (installed), the staged
// list and the catalog version. Present = its main module is listed;
// current = every one of its modules is there with the catalog CRC.
// Modules the user's own settings produce (the satellite data of SAT PASS): the catalog only ships a default.
// They are not compared by CRC (refreshed data must not look like an outdated app), they must be present,
// and updating the app keeps the ones you already have.
const dataNames = item => new Set(item.data ?? []);

export function appState(item, installed, staged) {
  const data = dataNames(item);
  const has = list => (list ?? []).slice(1).some(m => m.name === item.name);
  const current = list => {
    const modules = (list ?? []).slice(1);
    return partsOf(item).every(p => modules.some(m => m.name === p.name && (data.has(p.name) || crcOf(m) === p.crc16))) &&
      !modules.some(m => item.retiredModules?.includes(m.name));
  };
  const onRadio = has(installed), inStaged = has(staged);
  if (inStaged) {
    if (!onRadio) return { status: 'adding', label: 'Will be added', action: 'remove' };
    if (!current(staged)) return { status: 'outdated', label: 'Update available', action: 'update' };
    if (!current(installed)) return { status: 'updating', label: 'Will be updated', action: 'remove' };
    return { status: 'installed', label: 'Installed', action: 'remove' };
  }
  if (onRadio) return { status: 'removing', label: 'Will be removed', action: 'add' };
  return { status: 'available', label: 'Not installed', action: 'add' };
}

export function withModule(staged, module, menu) {
  if (!staged.length) {
    if (!menu) throw new Error('The menu is needed for the first app.');
    return [menu, module];
  }
  const at = staged.findIndex((m, i) => i > 0 && m.name === module.name);
  if (at > 0) return staged.map((m, i) => (i === at ? module : m));
  return [...staged, module];
}

export function withoutModule(staged, name) {
  return staged.filter((m, i) => i === 0 || m.name !== name);
}

// Several modules (an app), added or replaced in order.
export function withModules(staged, modules, menu) {
  return modules.reduce((list, m) => withModule(list, m, menu), staged);
}

// Add or update a catalog app: like withModules, but a data module you already have staged (the satellite
// data you refreshed) is kept instead of being replaced by the catalog's default.
export function withApp(staged, modules, menu, item) {
  const keep = dataNames(item);
  const mine = new Map((staged ?? []).slice(1).filter(m => keep.has(m.name)).map(m => [m.name, m]));
  const existing = staged.filter((m, i) => i === 0 || !item.retiredModules?.includes(m.name));
  return modules.reduce((list, m) => withModule(list, mine.get(m.name) ?? m, menu), existing);
}

export function withoutApp(staged, item) {
  const names = new Set(partsOf(item).map(p => p.name));
  return staged.filter((m, i) => i === 0 || !names.has(m.name));
}

// A release declares which menu builds it replaces. Other CRCs may belong to newer firmware.
export function menuOutdated(catalog, staged) {
  if (!catalog || !staged.length) return false;
  const crc = crcOf(staged[0]);
  return crc !== catalog.menu.crc16 && Boolean(catalog.menu.replaces?.includes(crc));
}

export async function withUpdatedMenu(catalog, staged, fetcher = fetch) {
  if (!menuOutdated(catalog, staged)) throw new Error('This catalog has no update for the installed menu.');
  const menu = await fetchModule(catalog, catalog.menu, fetcher);
  let next = [menu, ...staged.slice(1)];
  if (catalog.profile === 'smartbeacon') {
    const names = new Set(staged.slice(1).map(m => m.name));
    for (const item of catalog.apps) {
      if (!partsOf(item).some(part => names.has(part.name))) continue;
      const modules = await fetchApp(catalog, item, fetcher);
      next = withApp(next, names.has(item.name) ? modules : modules.filter(m => names.has(m.name)), null, item);
    }
  }
  return next;
}
