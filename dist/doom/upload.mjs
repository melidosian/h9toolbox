// Game-data uploader for the TD-H9 resource flash (used by /doom/ and the
// research prototype in prototypes/data-upload).
//
// Writes a data pack (for example the Freedoom E1M1 pack for the Doom
// firmware) over Bluetooth through the H9 Toolbox write service (S2), under
// the rules in firmware-research/doom-firmware/README.md:
//   - only sectors that were erased in the 2026-11-09 full-chip dump, in
//     partitions stock never writes (the `resource`, `font` and `voice` tails);
//   - existing data there is overwritten only if a previous upload's
//     directory lists it; anything else is refused;
//   - everything written is read back and compared;
//   - restore erases exactly what the directory lists (or, deep, every
//     non-erased sector of the three tails), returning them to 0xFF.
// Layout: game slot: a 4 KiB directory sector at 0xA1F000, the pack from
// 0xA20000 to 0xCC0000, continuing into the font and then the voice tail if it
// is larger. Modules slot: directory 0xCC0000, data 0xCC1000-0xCE0000. Music
// slot: directory 0xCE0000, data 0xCE1000-0xD00000.
// Order: the directory is written first with its last 32-byte block left
// erased ("in progress"); it claims the sectors, so an interrupted upload
// resumes where it stopped. After the pack is written and verified, that
// block is programmed with COMPLETE (an erased block can be programmed
// without erasing the sector). Readers must ignore packs not marked complete.

import {
  BLOCK, LINK_FIXED_ADDRESS, LINK_STOCK_ADDRESS, OP, SECTOR,
  controlFrame, crc16, dataFrame, parseBulkReply,
} from '../boot-logo/resource.mjs';

const ERASED_CRC = 0xe03e;
const CHECK_EVERY = 16;

// Erased in the full-chip dump (SHA-256 b6d60e2e...), never written by stock.
export const AREAS = [
  { name: 'resource', start: 0xa1f000, end: 0xd00000 },
  { name: 'font', start: 0x717000, end: 0x800000 },
  { name: 'voice', start: 0x3a7000, end: 0x400000 },
];
export const DIRECTORY = 0xa1f000;
// Upload slots, each with its own directory sector. The game pack fills the
// resource tail up to the modules slot, then the font and voice tails; the
// modules slot (an H9MODS01 bundle for H9 Toolbox Firmware's module loader,
// firmware-research/module-loader) and music (an H9MU song for the Doom
// firmware, doom-firmware/music.py) get the last 2 x 128 KiB of the resource
// tail. A slot only claims or erases what its own directory lists.
export const MODULES_DIRECTORY = 0xcc0000;
export const MUSIC_DIRECTORY = 0xce0000;
export const SLOTS = {
  game: {
    name: 'game', directory: DIRECTORY,
    areas: [
      { name: 'resource', start: DIRECTORY + 0x1000, end: MODULES_DIRECTORY },
      { name: 'font', start: 0x717000, end: 0x800000 },
      // Stops two sectors short: 0x3FF000 holds the last GPS fix (the loader stub and
      // SAT PASS write it) and 0x3FE000 the user's own VFO frequencies while SAT PASS's
      // VFO tracker has changed them (module-loader/modules/common/vfo_backup.h).
      { name: 'voice', start: 0x3a7000, end: 0x3fe000 },
    ],
  },
  modules: { name: 'modules', directory: MODULES_DIRECTORY, areas: [{ name: 'modules', start: MODULES_DIRECTORY + 0x1000, end: MUSIC_DIRECTORY }] },
  music: { name: 'music', directory: MUSIC_DIRECTORY, areas: [{ name: 'music', start: MUSIC_DIRECTORY + 0x1000, end: 0xd00000 }] },
};
export const MAGIC = 'H9DATA01';
export const COMPLETE = 'H9DATA-COMPLETE';
export const COMPLETE_OFFSET = SECTOR - BLOCK;
const NAME_BYTES = 32;

const hex = value => `0x${value.toString(16)}`;
const isErased = bytes => bytes.every(byte => byte === 0xff);
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

export function inAreas(start, end) {
  return AREAS.some(area => start >= area.start && end <= area.end);
}

// Chip ranges for a pack of `length` bytes in a slot's areas, in order (for
// the game slot: resource after the directory sector, then font, then voice).
export function planLayout(length, slot = SLOTS.game) {
  if (!(length > 0)) throw new Error('The pack is empty.');
  const segments = [];
  let left = length;
  for (const area of slot.areas) {
    const start = area.start;
    const room = area.end - start;
    const take = Math.min(left, room);
    if (take > 0) segments.push({ area: area.name, address: start, length: take });
    left -= take;
    if (!left) return segments;
  }
  throw new Error(`The pack (${length.toLocaleString()} bytes) is larger than the stock-safe space of the ${slot.name} slot.`);
}

export async function sha256(bytes) {
  return new Uint8Array(await globalThis.crypto.subtle.digest('SHA-256', bytes));
}

// Directory sector: magic, version, pack length, SHA-256, name, segments,
// CRC-16 of everything before it; the rest of the sector stays 0xFF.
export async function buildDirectory(pack, name, slot = SLOTS.game) {
  const segments = planLayout(pack.length, slot);
  const sector = new Uint8Array(SECTOR).fill(0xff);
  const view = new DataView(sector.buffer);
  sector.set(new TextEncoder().encode(MAGIC), 0);
  view.setUint32(8, 1, true);
  view.setUint32(12, pack.length, true);
  sector.set(await sha256(pack), 16);
  const label = new Uint8Array(NAME_BYTES);
  label.set(new TextEncoder().encode(name).subarray(0, NAME_BYTES));
  sector.set(label, 48);
  view.setUint32(80, segments.length, true);
  segments.forEach((segment, i) => {
    view.setUint32(84 + 8 * i, segment.address, true);
    view.setUint32(88 + 8 * i, segment.length, true);
  });
  const end = 84 + 8 * segments.length;
  view.setUint16(end, crc16(sector.subarray(0, end)), true);
  return { sector, segments };
}

export function parseDirectory(sector) {
  if (new TextDecoder().decode(sector.subarray(0, 8)) !== MAGIC) return null;
  const view = new DataView(sector.buffer, sector.byteOffset, sector.byteLength);
  const count = view.getUint32(80, true);
  if (count < 1 || count > AREAS.length) return null;
  const end = 84 + 8 * count;
  if (view.getUint16(end, true) !== crc16(sector.subarray(0, end))) return null;
  const segments = Array.from({ length: count }, (_, i) => ({
    address: view.getUint32(84 + 8 * i, true), length: view.getUint32(88 + 8 * i, true),
  }));
  if (!segments.every(s => inAreas(s.address, s.address + s.length))) return null;
  const name = new TextDecoder().decode(sector.subarray(48, 80)).replace(/\0+$/, '');
  const complete = new TextDecoder().decode(sector.subarray(COMPLETE_OFFSET, COMPLETE_OFFSET + COMPLETE.length)) === COMPLETE;
  return { length: view.getUint32(12, true), sha256: sector.slice(16, 48), name, segments, complete };
}

// Sector actions to turn `current` into `next` at chip `address` (whole
// sectors). `owned(address)` says whether existing data there belongs to a
// previous upload and may be erased.
export function sectorPlan(address, current, next, owned = () => false) {
  if (address % SECTOR || current.length !== next.length || next.length % SECTOR) {
    throw new Error('Sector plans need whole, aligned sectors.');
  }
  if (!inAreas(address, address + next.length)) throw new Error(`${hex(address)} is outside the stock-safe areas.`);
  const plan = [];
  for (let offset = 0; offset < next.length; offset += SECTOR) {
    const old = current.subarray(offset, offset + SECTOR);
    const fresh = next.subarray(offset, offset + SECTOR);
    if (old.every((byte, i) => byte === fresh[i])) continue;
    const erase = !isErased(old);
    if (erase && !owned(address + offset)) {
      throw new Error(`Sector ${hex(address + offset)} holds data this tool did not write; refusing to overwrite it.`);
    }
    plan.push({ address: address + offset, offset, erase, oldCrc: crc16(old), program: !isErased(fresh) });
  }
  return plan;
}

const pad = bytes => {
  const out = new Uint8Array(Math.ceil(bytes.length / SECTOR) * SECTOR).fill(0xff);
  out.set(bytes);
  return out;
};

// Erase and program the planned sectors (the S2 service verifies each block
// and refuses everything after a failure; its status is checked every 16
// blocks and at each sector's end). Returns bytes programmed.
async function writePlan(radio, plan, next, onSector) {
  let armed = false;
  let programmed = 0;
  for (const entry of plan) {
    radio.checkStop();
    const label = hex(entry.address);
    if (entry.erase) {
      await radio.sendWrite(controlFrame(OP.erase, entry.address, entry.oldCrc));
      await radio.expectWrite(entry.address, `Erase ${label}`, state => state.crc === ERASED_CRC);
    }
    if (entry.program) {
      const data = next.subarray(entry.offset, entry.offset + SECTOR);
      await radio.sendWrite(controlFrame(OP.program, entry.address));
      armed = true;
      await radio.expectWrite(entry.address, `Begin ${label}`, state => state.programming && state.pointer === entry.address);
      try {
        for (let offset = 0; offset < SECTOR; offset += BLOCK) {
          await radio.sendWrite(dataFrame(data.subarray(offset, offset + BLOCK)));
          if ((offset / BLOCK) % CHECK_EVERY === CHECK_EVERY - 1) {
            await radio.expectWrite(entry.address, `Program ${label}+${hex(offset + BLOCK - CHECK_EVERY * BLOCK)}`,
              state => state.pointer === entry.address + offset + BLOCK && state.programming === (offset + BLOCK < SECTOR));
          }
        }
      } catch (error) {
        try { await radio.sendWrite(controlFrame(OP.disarm)); } catch { /* reported by the caller */ }
        throw error;
      }
      armed = false;
      programmed += SECTOR;
    }
    onSector(entry);
  }
  return { programmed, armed };
}

async function withWriteLink(radio, work) {
  let linkRequested = false;
  await radio.enterProgramming();
  try {
    const link = parseBulkReply(await radio.readBlock(LINK_FIXED_ADDRESS));
    if (link) {
      linkRequested = link.status === 0;
      radio.log(`Requested a fixed 15 ms link for writing: ${link.statusText}.`, link.status === 0 ? 'info' : 'warn');
      await sleep(radio.linkDelayMs);
    }
    return await work();
  } finally {
    if (linkRequested) { try { await radio.readBlock(LINK_STOCK_ADDRESS); } catch { /* the link resets on disconnect */ } }
    await radio.leaveProgramming();
  }
}

// Read the chip ranges [{address, length}] (whole sectors). Fast (bulk)
// reads need a 32 KiB-aligned start, so each read starts at the bank
// boundary below and the extra bytes are dropped.
const BANK = 0x8000;
async function readRanges(radio, ranges, onProgress) {
  const total = ranges.reduce((sum, r) => sum + r.length + r.address % BANK, 0);
  let done = 0;
  const out = [];
  for (const range of ranges) {
    const lead = range.address % BANK;
    const { bytes } = await radio.readRange(range.address - lead, range.length + lead,
      fraction => onProgress((done + fraction * (range.length + lead)) / total));
    done += range.length + lead;
    out.push(bytes.slice(lead));
  }
  return out;
}

// A directory sector as far as parseDirectory needs it: the record (first
// 128 bytes: magic ... segments and CRC) and the COMPLETE block (last 32),
// in 32-byte reads. Fast reads start at a 32 KiB bank, so a fast read of the
// game directory (0xA1F000) would fetch 32 KiB: a minute in upload mode.
export async function readDirectory(radio, slot) {
  const sector = new Uint8Array(SECTOR).fill(0xff);
  if (typeof radio.readSlow !== 'function') {
    return (await readRanges(radio, [{ address: slot.directory, length: SECTOR }], () => {}))[0];
  }
  sector.set((await radio.readSlow(slot.directory, 128, () => {})).bytes, 0);
  sector.set((await radio.readSlow(slot.directory + COMPLETE_OFFSET, BLOCK, () => {})).bytes, COMPLETE_OFFSET);
  return sector;
}

// What a slot holds: { dir, pack }. `dir` is null for an empty or unreadable
// directory; `pack` is null unless the upload is complete and its bytes read
// back with the directory's SHA-256.
export async function readPack(radio, slot, onProgress = () => {}) {
  const dir = parseDirectory(await readDirectory(radio, slot));
  if (!dir?.complete) return { dir, pack: null };
  const parts = await readRanges(radio, dir.segments.map(s => ({ address: s.address, length: Math.ceil(s.length / BLOCK) * BLOCK })),
    onProgress);
  const pack = new Uint8Array(dir.length);
  let offset = 0;
  dir.segments.forEach((s, i) => { pack.set(parts[i].subarray(0, s.length), offset); offset += s.length; });
  if (!sameBytes(await sha256(pack), dir.sha256)) throw new Error(`The ${slot.name} slot did not read back with its recorded SHA-256.`);
  return { dir, pack };
}

export function completeBlock() {
  const block = new Uint8Array(BLOCK);
  block.set(new TextEncoder().encode(COMPLETE));
  return block;
}

// One line per sector: how long the GATT writes and the radio's ACKs took.
function logWriteTimes(radio, address) {
  const times = radio.writeTimes;
  if (!times?.length) return;
  radio.writeTimes = [];
  const stat = k => {
    const v = times.map(t => t[k]).sort((a, b) => a - b);
    return `median ${v[v.length >> 1].toFixed(0)} ms, max ${v[v.length - 1].toFixed(0)} ms`;
  };
  radio.log(`${hex(address)}: ${times.length} writes; GATT write ${stat(0)}; ACK wait ${stat(1)}.`);
}

const sameBytes = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const rangesOf = (dir, slot) => [...dir.segments.map(s => ({ address: s.address, length: Math.ceil(s.length / SECTOR) * SECTOR })),
  { address: slot.directory, length: SECTOR }];

async function programBlock(radio, address, block) {
  await radio.sendWrite(controlFrame(OP.program, address));
  await radio.expectWrite(address, `Begin ${hex(address)}`, state => state.programming && state.pointer === address);
  await radio.sendWrite(dataFrame(block));
  await radio.expectWrite(address, `Program ${hex(address)}`, state => state.pointer === address + BLOCK && !state.programming);
}

// Upload `pack` (Uint8Array). onProgress(fraction, phase, bytesPerSecond).
// Returns a manifest for the record.
export async function uploadPack(radio, pack, { name = 'pack', slot = SLOTS.game, onProgress = () => {}, now = () => Date.now() } = {}) {
  const { sector: directory, segments } = await buildDirectory(pack, name, slot);
  const digest = await sha256(pack);
  const regions = [{ area: `${slot.name} directory`, address: slot.directory, length: SECTOR },
    ...segments.map(s => ({ ...s, length: Math.ceil(s.length / SECTOR) * SECTOR }))];
  let offset = 0;
  const targets = regions.map((region, i) => {
    if (!i) return directory;
    const part = pad(pack.subarray(offset, offset + segments[i - 1].length));
    offset += segments[i - 1].length;
    return part;
  });
  const started = now();
  // Already there? The directory alone says so: COMPLETE is written only after
  // the whole upload read back correctly. One sector instead of the whole
  // target (about 17 minutes for the 1 MB game pack at upload mode's rate).
  const already = parseDirectory(await readDirectory(radio, slot));
  if (already?.complete && already.length === pack.length && sameBytes(already.sha256, digest)) {
    radio.log(`"${already.name}" is already on the radio and complete; nothing to upload.`);
    onProgress(1, 'Already on the radio');
    return {
      kind: 'td-h9-data-upload', slot: slot.name, name: already.name, bytes: pack.length,
      sha256: Array.from(digest, b => b.toString(16).padStart(2, '0')).join(''),
      directory: hex(slot.directory), segments: already.segments.map(s => ({ address: hex(s.address), length: s.length })),
      sectorsWritten: 0, bytesProgrammed: 0, writeSeconds: 0, writeBytesPerSecond: 0,
      totalSeconds: (now() - started) / 1000, verified: true, complete: true, alreadyComplete: true,
    };
  }
  radio.log(`Reading ${regions.reduce((s, r) => s + r.length, 0).toLocaleString()} bytes to check the target sectors.`);
  let current = await readRanges(radio, regions, f => onProgress(0.2 * f, 'Checking target sectors'));
  const previous = parseDirectory(current[0]);
  const same = previous && previous.length === pack.length && sameBytes(previous.sha256, digest);
  if (previous && !same) {
    radio.log(`Replacing the previous upload "${previous.name}": erasing its ${previous.segments.length} range(s) first.`);
    await restore(radio, { slot, onProgress: f => onProgress(0.2 * f, 'Erasing the previous upload') });
    current = await readRanges(radio, regions, f => onProgress(0.2 * f, 'Checking target sectors'));
  } else if (same) {
    radio.log(`Resuming "${previous.name}" (${previous.complete ? 'already complete' : 'in progress'}).`);
  }
  const owned = same ? address => rangesOf(previous, slot).some(r => address >= r.address && address < r.address + r.length)
    : () => false;
  // A complete directory differs from `directory` only in its last block.
  const dirTarget = same && previous.complete ? current[0] : directory;
  const plans = regions.map((region, i) => sectorPlan(region.address, current[i], i ? targets[i] : dirTarget, owned));
  const sectors = plans.reduce((n, p) => n + p.length, 0);
  let written = 0, programmed = 0;
  const writeStart = now();
  if (sectors) {
    await withWriteLink(radio, async () => {
      for (const [i, plan] of plans.entries()) {
        // Count each programmed sector as it completes, so the live rate is right.
        await writePlan(radio, plan, i ? targets[i] : dirTarget, entry => {
          logWriteTimes(radio, entry.address);
          written += 1;
          if (entry.program) programmed += SECTOR;
          const seconds = Math.max(0.001, (now() - writeStart) / 1000);
          onProgress(0.2 + 0.6 * written / sectors, `Writing sector ${written} of ${sectors}`, programmed / seconds);
        });
      }
    });
  }
  const writeSeconds = (now() - writeStart) / 1000;
  // Read back the pack (and directory) before marking it complete.
  const check = await readRanges(radio, regions, f => onProgress(0.8 + 0.18 * f, 'Verifying'));
  for (let i = 1; i < regions.length; i++) {
    const bad = check[i].findIndex((byte, k) => byte !== targets[i][k]);
    if (bad >= 0) throw new Error(`Read-back differs at ${hex(regions[i].address + bad)}.`);
  }
  if (!sameBytes(check[0].subarray(0, COMPLETE_OFFSET), directory.subarray(0, COMPLETE_OFFSET))) {
    throw new Error('Directory read-back differs.');
  }
  if (!parseDirectory(check[0]).complete) {
    await withWriteLink(radio, () => programBlock(radio, slot.directory + COMPLETE_OFFSET, completeBlock()));
    const [dir] = await readRanges(radio, [{ address: slot.directory, length: SECTOR }], () => {});
    if (!parseDirectory(dir)?.complete) throw new Error('The directory did not read back as complete.');
  }
  onProgress(1, 'Done');
  return {
    kind: 'td-h9-data-upload', slot: slot.name, name, bytes: pack.length,
    sha256: Array.from(digest, b => b.toString(16).padStart(2, '0')).join(''),
    directory: hex(slot.directory), segments: segments.map(s => ({ area: s.area, address: hex(s.address), length: s.length })),
    sectorsWritten: sectors, bytesProgrammed: programmed, writeSeconds,
    writeBytesPerSecond: Math.round(programmed / Math.max(0.001, writeSeconds)),
    totalSeconds: (now() - started) / 1000, verified: true, complete: true,
  };
}

// Erase what the directory lists (or, with deep, every non-erased sector in
// the three tails), then confirm 0xFF by read-back.
export async function restore(radio, { deep = false, slot = SLOTS.game, onProgress = () => {} } = {}) {
  const [dirSector] = await readRanges(radio, [{ address: slot.directory, length: SECTOR }], () => {});
  const dir = parseDirectory(dirSector);
  let ranges;
  if (deep) {
    ranges = AREAS.map(a => ({ address: a.start, length: a.end - a.start }));
  } else {
    if (!dir) {
      if (isErased(dirSector)) return { erased: 0, note: 'No upload directory; nothing to restore.' };
      throw new Error('The directory sector holds unknown data. Use a deep restore.');
    }
    ranges = rangesOf(dir, slot);
  }
  const current = await readRanges(radio, ranges, f => onProgress(0.5 * f, 'Reading'));
  const plans = ranges.map((r, i) => sectorPlan(r.address, current[i], new Uint8Array(r.length).fill(0xff), () => true));
  const total = plans.reduce((s, p) => s + p.length, 0);
  let done = 0;
  await withWriteLink(radio, async () => {
    for (const [i, plan] of plans.entries()) {
      await writePlan(radio, plan, new Uint8Array(ranges[i].length).fill(0xff),
        () => onProgress(0.5 + 0.4 * (++done) / Math.max(1, total), `Erasing sector ${done} of ${total}`));
    }
  });
  const erasedRanges = ranges.filter((_, i) => plans[i].length);
  const check = await readRanges(radio, erasedRanges, f => onProgress(0.9 + 0.1 * f, 'Verifying'));
  check.forEach((bytes, i) => {
    if (!isErased(bytes)) throw new Error(`${hex(erasedRanges[i].address)} is not erased after restore.`);
  });
  return { erased: total, name: dir?.name ?? null };
}
