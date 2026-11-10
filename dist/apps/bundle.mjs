// H9MODS01 module bundles for H9 Toolbox Firmware's module loader
// (firmware-research/module-loader). Mirrors build.py's bundle()/parse_bundle()
// and the checks the main-flash stub makes before it runs a module.
//
//   +0  "H9MODS01"  +8 u16 version 1  +10 u16 count (1-255)  +12 u32 bundle bytes
//   +16 + 32*i: name[16], u32 offset, u32 size, u32 bss, u16 entry, u16 CRC-16/XMODEM
// Module 0 is the menu. A .h9mod file is a bundle holding one module.

import { crc16 } from '../boot-logo/resource.mjs';

export const MAGIC = 'H9MODS01';
export const VERSION = 1;
export const HEADER = 16;
export const ENTRY = 32;
export const NAME_BYTES = 16;
export const LOAD_ADDRESS = 0x29f00;
export const RAM_SLOT = 0x2000;          // 0x29F00-0x2BEFF, taken from the stock heap
export const CHIP_SLOT = 0x1f000;        // the modules upload slot: 0xCC1000-0xCDFFFF
export const MAX_MODULES = 255;
export const MENU_LIST_MAX = 64;         // menu.c shows at most this many modules
// Characters the menu's font can draw (modules/menu/font5x7.py; lower case is shown as upper case).
export const MENU_CHARS = ' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:>/?';

const text = new TextEncoder();
const ascii = bytes => String.fromCharCode(...bytes);

// Problems with a module that would stop the stub loading it (errors) or make
// it look wrong in the menu (warnings).
export function checkModule(m) {
  const errors = [], warnings = [];
  if (!m.name || m.name.length > NAME_BYTES || !/^[\x20-\x7e]+$/.test(m.name)) errors.push(`name must be 1-${NAME_BYTES} ASCII characters`);
  if (!(m.body?.length > 0)) errors.push('empty body');
  if (m.body?.length + m.bss > RAM_SLOT) errors.push(`needs ${(m.body.length + m.bss).toLocaleString()} bytes of RAM; the slot has ${RAM_SLOT.toLocaleString()}`);
  if (!(m.entry >= 0 && m.entry < m.body?.length && m.entry <= 0xffff)) errors.push('entry point outside the body');
  if (!(m.bss >= 0 && m.bss < RAM_SLOT)) errors.push('bad bss size');
  const unsupported = [...new Set([...(m.name ?? '').toUpperCase()].filter(c => !MENU_CHARS.includes(c)))];
  if (unsupported.length) warnings.push(`the menu shows ${unsupported.map(c => JSON.stringify(c)).join(' ')} as blank`);
  return { errors, warnings };
}

// Bundle layout: offsets and sizes without building the bytes.
export function layout(modules) {
  let offset = HEADER + ENTRY * modules.length;
  const placed = modules.map(m => {
    offset += (-offset) & 3;
    const at = offset;
    offset += m.body.length;
    return at;
  });
  return { offsets: placed, bytes: offset };
}

export function buildBundle(modules) {
  if (modules.length < 1 || modules.length > MAX_MODULES) throw new Error(`A bundle holds 1-${MAX_MODULES} modules.`);
  for (const m of modules) {
    const { errors } = checkModule(m);
    if (errors.length) throw new Error(`${m.name || 'module'}: ${errors.join('; ')}`);
  }
  const { offsets, bytes } = layout(modules);
  if (bytes > CHIP_SLOT) throw new Error(`The bundle (${bytes.toLocaleString()} bytes) is larger than the ${CHIP_SLOT.toLocaleString()}-byte modules slot.`);
  const out = new Uint8Array(bytes).fill(0xff);
  const view = new DataView(out.buffer);
  out.set(text.encode(MAGIC), 0);
  view.setUint16(8, VERSION, true);
  view.setUint16(10, modules.length, true);
  view.setUint32(12, bytes, true);
  modules.forEach((m, i) => {
    const e = HEADER + ENTRY * i;
    out.fill(0, e, e + NAME_BYTES);
    out.set(text.encode(m.name), e);
    view.setUint32(e + 16, offsets[i], true);
    view.setUint32(e + 20, m.body.length, true);
    view.setUint32(e + 24, m.bss, true);
    view.setUint16(e + 28, m.entry, true);
    view.setUint16(e + 30, crc16(m.body), true);
    out.set(m.body, offsets[i]);
  });
  return out;
}

export function parseBundle(bytes) {
  if (bytes.length < HEADER || ascii(bytes.subarray(0, 8)) !== MAGIC) throw new Error('Not an H9MODS01 module bundle.');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const version = view.getUint16(8, true), count = view.getUint16(10, true), length = view.getUint32(12, true);
  if (version !== VERSION) throw new Error(`Unsupported bundle version ${version}.`);
  if (count < 1 || count > MAX_MODULES || length !== bytes.length || HEADER + ENTRY * count > length) throw new Error('Bad bundle header.');
  const modules = [];
  for (let i = 0; i < count; i++) {
    const e = HEADER + ENTRY * i;
    const name = ascii(bytes.subarray(e, e + NAME_BYTES)).replace(/\0+$/, '');
    const offset = view.getUint32(e + 16, true), size = view.getUint32(e + 20, true), bss = view.getUint32(e + 24, true);
    const entry = view.getUint16(e + 28, true), crc = view.getUint16(e + 30, true);
    if (!(size > 0 && size + bss <= RAM_SLOT && entry < size && offset + size <= length)) throw new Error(`Module ${i} (${name}) has a bad entry.`);
    const body = bytes.slice(offset, offset + size);
    if (crc16(body) !== crc) throw new Error(`Module ${i} (${name}) fails its CRC.`);
    modules.push({ name, body, bss, entry, crc });
  }
  return modules;
}

// Chip and RAM use of a module list (menu first), for the memory view.
export function usage(modules) {
  const { bytes } = layout(modules);
  return {
    chipUsed: bytes, chipFree: CHIP_SLOT - bytes, chipTotal: CHIP_SLOT,
    largestRam: Math.max(0, ...modules.map(m => m.body.length + m.bss)), ramTotal: RAM_SLOT,
    listed: Math.max(0, modules.length - 1), listMax: MENU_LIST_MAX,
  };
}
