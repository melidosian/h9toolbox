import { APP_START, FLASH_SIZE, IMAGE_SIZE, SECTOR_SIZE, crc16, scanRadio, sha256, stockDifferences } from './firmware.mjs?v=20261111g';
import { FAR_START, FAR_END } from './far-region.mjs?v=20261126backup';

export class BackupStoppedError extends Error {
  constructor() { super('HID backup cancelled. No radio write was sent.'); }
}

export class UnverifiedRadioError extends Error {
  constructor() {
    super('The updater could not verify the TD-H9 model data. Select an exact backup from this radio or use the recovery guide.');
  }
}

const SECTORS = (FLASH_SIZE - APP_START) / SECTOR_SIZE;
const BOUNDARIES = SECTORS - 1;
const APP_SECTORS = (IMAGE_SIZE - APP_START) / SECTOR_SIZE;
const APP_BYTES = IMAGE_SIZE - APP_START;
const FAR_OFFSET = FAR_START - APP_START;
const FAR_BYTES = FAR_END - FAR_START;
const RECOVERY_STRIDE = 2;
const BLANK = new Uint8Array(SECTOR_SIZE).fill(0xff);
const BLANK_CRC = crc16(BLANK);
const MAGIC = new TextEncoder().encode('H9HID1\n');
const SHIFTS = [1, 37, 1024, 2048, 3072, 4095];

function requireValue(condition, message) { if (!condition) throw new Error(message); }
function step(crc, byte) {
  crc ^= byte << 8;
  for (let bit = 0; bit < 8; bit++) crc = ((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0)) & 0xffff;
  return crc;
}

const byteEffect = new Int16Array(65536).fill(-1);
const removeEffect = new Int16Array(65536).fill(-1);
const inverseZeroStep = new Uint16Array(65536);
const byteCrc = new Uint16Array(256);
const removeCrc = new Uint16Array(256);
const pairEffect = new Uint16Array(65536);
const removePairEffect = new Uint16Array(65536);
for (let crc = 0; crc < 65536; crc++) inverseZeroStep[step(crc, 0)] = crc;
for (let byte = 0; byte < 256; byte++) {
  byteCrc[byte] = step(0, byte);
  byteEffect[byteCrc[byte]] = byte;
  let contribution = byteCrc[byte];
  for (let i = 1; i < SECTOR_SIZE; i++) contribution = step(contribution, 0);
  removeCrc[byte] = contribution;
  removeEffect[contribution] = byte;
}
for (let pair = 0; pair < 65536; pair++) {
  const first = pair >> 8, second = pair & 255;
  pairEffect[step(byteCrc[first], second)] = pair;
  removePairEffect[removeCrc[first] ^ inverseZeroStep[removeCrc[second]]] = pair;
}

// A 16-bit CRC determines two unknown bytes of a window when the rest is
// known. A two-byte shift halves the number of HID requests.
export function recoverNextSector(previous, windows, nextSectorCrc, stride = 1) {
  requireValue(previous.length === SECTOR_SIZE && windows.length === SECTOR_SIZE &&
    (stride === 1 || stride === RECOVERY_STRIDE), 'Invalid CRC recovery input.');
  const result = new Uint8Array(SECTOR_SIZE);
  for (let offset = 0; offset < SECTOR_SIZE; offset += stride) {
    const nextOffset = offset + stride;
    const observed = nextOffset === SECTOR_SIZE ? nextSectorCrc : windows[nextOffset];
    const rest = windows[offset] ^ removeCrc[previous[offset]];
    if (stride === 2) {
      const remaining = rest ^ inverseZeroStep[removeCrc[previous[offset + 1]]];
      const pair = pairEffect[observed ^ step(step(remaining, 0), 0)];
      result[offset] = pair >> 8;
      result[offset + 1] = pair & 255;
    } else {
      const value = byteEffect[observed ^ step(rest, 0)];
      requireValue(value >= 0, `CRC recovery failed at byte ${offset}.`);
      result[offset] = value;
    }
  }
  requireValue(crc16(result) === nextSectorCrc, 'Recovered sector CRC mismatch.');
  return result;
}

export function recoverPreviousSector(next, windows, previousSectorCrc, stride = 1) {
  requireValue(next.length === SECTOR_SIZE && windows.length === SECTOR_SIZE &&
    (stride === 1 || stride === RECOVERY_STRIDE), 'Invalid CRC recovery input.');
  requireValue(windows[0] === previousSectorCrc, 'Previous sector CRC mismatch.');
  const result = new Uint8Array(SECTOR_SIZE);
  const nextSectorCrc = crc16(next);
  for (let offset = SECTOR_SIZE - stride; offset >= 0; offset -= stride) {
    const nextOffset = offset + stride;
    const observedNext = nextOffset === SECTOR_SIZE ? nextSectorCrc : windows[nextOffset];
    if (stride === 2) {
      const incoming = step(byteCrc[next[offset]], next[offset + 1]);
      const rest = inverseZeroStep[inverseZeroStep[observedNext ^ incoming]];
      const pair = removePairEffect[windows[offset] ^ rest];
      result[offset] = pair >> 8;
      result[offset + 1] = pair & 255;
    } else {
      const rest = inverseZeroStep[observedNext ^ byteCrc[next[offset]]];
      const value = removeEffect[windows[offset] ^ rest];
      requireValue(value >= 0, `Reverse CRC recovery failed at byte ${offset}.`);
      result[offset] = value;
    }
  }
  requireValue(crc16(result) === previousSectorCrc, 'Recovered reverse sector CRC mismatch.');
  return result;
}

export async function knownAppMatches(hid, crcs, target) {
  if (target.length !== IMAGE_SIZE) return false;
  for (let index = 0; index < APP_SECTORS; index++) {
    if (crc16(target.subarray(APP_START + index * SECTOR_SIZE, APP_START + (index + 1) * SECTOR_SIZE)) !== crcs[index]) return false;
  }
  // Check shifted windows too; plain 16-bit sector CRCs alone could collide.
  for (const shift of SHIFTS) for (let start = 0; start < APP_SECTORS - 1; start += 16) {
    const count = Math.min(16, APP_SECTORS - 1 - start);
    const observed = await hid.windowCrcs(start * SECTOR_SIZE + shift, count);
    for (let index = 0; index < count; index++) {
      const physical = APP_START + (start + index) * SECTOR_SIZE + shift;
      if (observed[index] !== crc16(target.subarray(physical, physical + SECTOR_SIZE))) return false;
    }
  }
  return true;
}

export async function planFirmwareWithoutBackup(hid, crcs, target) {
  requireValue(crcs.length === SECTORS && target.length === IMAGE_SIZE, 'Incomplete firmware or radio CRC scan.');
  // Stock and Toolbox share the sector containing the Type-C TD-H9 model
  // identifier at 0xB672A and its neighbor. Keep this check even when all
  // other sectors differ.
  const modelSector = 0xb6000;
  const relative = modelSector - APP_START;
  const index = relative / SECTOR_SIZE;
  const model = target.subarray(modelSector, modelSector + 2 * SECTOR_SIZE);
  if (crcs[index] !== crc16(model.subarray(0, SECTOR_SIZE)) ||
      crcs[index + 1] !== crc16(model.subarray(SECTOR_SIZE))) throw new UnverifiedRadioError();
  for (const shift of SHIFTS) {
    const [actual] = await hid.windowCrcs(relative + shift, 1);
    if (actual !== crc16(model.subarray(shift, shift + SECTOR_SIZE))) throw new UnverifiedRadioError();
  }
  const update = stockDifferences(crcs, target).reverse();
  if (!update.length && !await knownAppMatches(hid, crcs, target))
    throw new Error('The shifted-window check found firmware bytes that differ from the selected package.');
  return { update, verifyShiftedWindows: true };
}

async function findBlankPair(hid, crcs) {
  for (let index = 0; index < SECTORS - 1; index++) {
    if (crcs[index] !== BLANK_CRC || crcs[index + 1] !== BLANK_CRC) continue;
    let match = true;
    for (const shift of SHIFTS) {
      if ((await hid.windowCrcs(index * SECTOR_SIZE + shift, 1))[0] !== BLANK_CRC) { match = false; break; }
    }
    if (match) return index;
  }
  throw new Error('No verified blank sector pair found. HID backup cannot be reconstructed on this radio.');
}

function groupBoundaries(boundaries) {
  const groups = [];
  for (const index of boundaries) {
    const group = groups.at(-1);
    if (group && index - group[0] < 16) group.push(index);
    else groups.push([index]);
  }
  return groups;
}

async function collectWindows(hid, crcs, boundaries, onProgress, cancelled) {
  const rows = Array(BOUNDARIES).fill(null);
  const groups = groupBoundaries(boundaries);
  const total = groups.length * (SECTOR_SIZE / RECOVERY_STRIDE - 1);
  let done = 0;
  for (const group of groups) {
    const base = group[0], count = group.at(-1) - base + 1;
    for (const index of group) {
      rows[index] = new Uint16Array(SECTOR_SIZE);
      rows[index][0] = crcs[index];
    }
    for (let shift = RECOVERY_STRIDE; shift < SECTOR_SIZE; shift += RECOVERY_STRIDE) {
      if (cancelled()) throw new BackupStoppedError();
      const values = await hid.windowCrcs(base * SECTOR_SIZE + shift, count);
      for (const index of group) rows[index][shift] = values[index - base];
      done++;
      if (done % 128 === 0 || done === total) onProgress(done, total);
    }
  }
  return rows;
}

// With settings false and a verified known application, only the application
// area (0x5000..0xD9FFF) is returned: its bytes come from that image, so no
// shifted-window recovery runs. The settings area above it (38 sectors) costs
// about 12,000 HID requests to recover and is never written by this page.
export async function readAccessibleFlash(hid, crcs, knownImages, onProgress = () => {}, cancelled = () => false, { settings = true } = {}) {
  requireValue(crcs.length === SECTORS, 'Incomplete HID sector scan.');
  const checkCancelled = () => { if (cancelled()) throw new BackupStoppedError(); };
  const sectors = Array(SECTORS).fill(null);
  let seedIndex, startBoundary, method;
  const candidates = Array.isArray(knownImages) ? knownImages : [knownImages];
  let verifiedSeed = null;
  for (const image of candidates) {
    if (await knownAppMatches(hid, crcs, image)) { verifiedSeed = image; break; }
  }
  if (verifiedSeed) {
    for (let index = 0; index < APP_SECTORS; index++) sectors[index] = verifiedSeed.slice(APP_START + index * SECTOR_SIZE, APP_START + (index + 1) * SECTOR_SIZE);
    seedIndex = APP_SECTORS - 1;
    startBoundary = seedIndex;
    method = 'verified-known-image-seed';
    if (!settings) {
      const application = new Uint8Array(IMAGE_SIZE - APP_START);
      for (let index = 0; index < APP_SECTORS; index++) application.set(sectors[index], index * SECTOR_SIZE);
      const after = await scanRadio(hid);
      requireValue(after.every((value, index) => value === crcs[index]), 'Radio CRCs changed during backup; start again.');
      onProgress(1, 1);
      return { accessible: application, method, settings: false };
    }
  } else {
    seedIndex = await findBlankPair(hid, crcs);
    sectors[seedIndex] = BLANK.slice();
    sectors[seedIndex + 1] = BLANK.slice();
    startBoundary = 0;
    method = 'verified-erased-sector-seed';
  }
  const boundaries = Array.from({ length: BOUNDARIES - startBoundary }, (_, i) => startBoundary + i);
  const rows = await collectWindows(hid, crcs, boundaries, onProgress, cancelled);
  if (startBoundary === 0) {
    for (let shift = 0; shift < SECTOR_SIZE; shift += RECOVERY_STRIDE)
      requireValue(rows[seedIndex][shift] === BLANK_CRC, 'Blank seed has inconsistent sliding CRCs.');
    for (let index = seedIndex - 1; index >= 0; index--)
      sectors[index] = recoverPreviousSector(sectors[index + 1], rows[index], crcs[index], RECOVERY_STRIDE);
  }
  for (let index = seedIndex; index < BOUNDARIES; index++) {
    if (sectors[index + 1]) continue;
    sectors[index + 1] = recoverNextSector(sectors[index], rows[index], crcs[index + 1], RECOVERY_STRIDE);
  }
  // Odd shifts are independent of the even windows used for reconstruction.
  requireValue(!(await mismatchedWindows(hid, sectors, boundaries, checkCancelled)).size, 'HID backup failed shifted-window verification.');
  const accessible = new Uint8Array(FLASH_SIZE - APP_START);
  for (let index = 0; index < SECTORS; index++) {
    requireValue(sectors[index] && crc16(sectors[index]) === crcs[index], `HID backup sector 0x${(APP_START + index * SECTOR_SIZE).toString(16)} failed verification.`);
    accessible.set(sectors[index], index * SECTOR_SIZE);
  }
  const after = await scanRadio(hid);
  requireValue(after.every((value, index) => value === crcs[index]), 'Radio CRCs changed during backup; start again.');
  return { accessible, method, settings: true };
}

async function mismatchedWindows(hid, sectors, boundaries, checkCancelled) {
  const mismatched = new Set();
  for (const group of groupBoundaries(boundaries)) {
    const base = group[0], count = group.at(-1) - base + 1;
    const data = new Uint8Array((count + 1) * SECTOR_SIZE);
    for (const index of group) {
      data.set(sectors[index], (index - base) * SECTOR_SIZE);
      data.set(sectors[index + 1], (index - base + 1) * SECTOR_SIZE);
    }
    for (const shift of SHIFTS) {
      checkCancelled();
      const observed = await hid.windowCrcs(base * SECTOR_SIZE + shift, count);
      for (const index of group) {
        const offset = (index - base) * SECTOR_SIZE + shift;
        if (observed[index - base] !== crc16(data.subarray(offset, offset + SECTOR_SIZE))) mismatched.add(index);
      }
    }
  }
  return mismatched;
}

// Reuse matching sectors from pinned images and reconstruct gaps from a
// neighboring seed, in either direction. Full readback remains available if
// the managed regions have no usable seed or the reconstructed bytes disagree.
export async function readFirmwareBackup(hid, crcs, images, farImages, onProgress = () => {}, cancelled = () => false) {
  requireValue(crcs.length === SECTORS, 'Incomplete HID sector scan.');
  const checkCancelled = () => { if (cancelled()) throw new BackupStoppedError(); };
  const ranges = [{ first: 0, count: APP_SECTORS, images: images.map(image => image.subarray(APP_START)) }];
  if (farImages) ranges.push({ first: FAR_OFFSET / SECTOR_SIZE, count: FAR_BYTES / SECTOR_SIZE, images: farImages });
  const sectors = Array(SECTORS).fill(null);
  const boundaries = [];
  for (const { first, count, images } of ranges) {
    for (let i = 0; i < count; i++) {
      const candidates = images.map(image => image.subarray(i * SECTOR_SIZE, (i + 1) * SECTOR_SIZE));
      sectors[first + i] = [...candidates, BLANK].find(bytes =>
        bytes.length === SECTOR_SIZE && crc16(bytes) === crcs[first + i]) ?? null;
      if (i + 1 < count) boundaries.push(first + i);
    }
  }
  const seededBoundaries = boundaries.filter(i => sectors[i] && sectors[i + 1]);
  for (const i of await mismatchedWindows(hid, sectors, seededBoundaries, checkCancelled)) {
    sectors[i] = null;
    sectors[i + 1] = null;
  }

  async function verifyUnchanged() {
    checkCancelled();
    const after = await scanRadio(hid);
    checkCancelled();
    requireValue(after.length === crcs.length && after.every((value, index) => value === crcs[index]), 'Radio CRCs changed during backup; start again.');
  }

  async function fullFallback() {
    await verifyUnchanged();
    const { accessible, method } = await readAccessibleFlash(hid, crcs, images, onProgress, cancelled);
    checkCancelled();
    return { accessible: accessible.slice(0, APP_BYTES), method,
      far: farImages ? accessible.slice(FAR_OFFSET, FAR_OFFSET + FAR_BYTES) : null };
  }

  // Each job uses the previous sector as the seed when possible. For a
  // leading gap, walk backwards from the first known sector in the region.
  const jobs = [];
  for (const { first, count } of ranges) {
    const end = first + count;
    const seed = sectors.findIndex((sector, i) => i >= first && i < end && sector);
    if (seed < 0) return fullFallback();
    for (let i = seed - 1; i >= first; i--) jobs.push({ boundary: i, reverse: true });
    for (let i = seed + 1; i < end; i++) {
      if (!sectors[i]) jobs.push({ boundary: i - 1, reverse: false });
    }
  }
  const needed = jobs.map(job => job.boundary).sort((a, b) => a - b);
  const rows = await collectWindows(hid, crcs, needed, onProgress, cancelled);
  try {
    for (const { boundary, reverse } of jobs) {
      if (reverse) sectors[boundary] = recoverPreviousSector(sectors[boundary + 1], rows[boundary], crcs[boundary], RECOVERY_STRIDE);
      else sectors[boundary + 1] = recoverNextSector(sectors[boundary], rows[boundary], crcs[boundary + 1], RECOVERY_STRIDE);
    }
  } catch {
    return fullFallback();
  }
  if (jobs.length && (await mismatchedWindows(hid, sectors, boundaries, checkCancelled)).size) {
    return fullFallback();
  }
  await verifyUnchanged();
  const copy = (first, count) => {
    const bytes = new Uint8Array(count * SECTOR_SIZE);
    for (let i = 0; i < count; i++) bytes.set(sectors[first + i], i * SECTOR_SIZE);
    return bytes;
  };
  onProgress(1, 1);
  return { accessible: copy(0, APP_SECTORS), far: farImages ? copy(FAR_OFFSET / SECTOR_SIZE, FAR_BYTES / SECTOR_SIZE) : null,
    method: jobs.length ? 'verified-sector-seeds-with-readback' : 'verified-sector-seeds' };
}

// v1 holds all HID-accessible flash, v2 holds the application, and v3
// concatenates the application and far firmware without the intervening gap.
export async function createHidArchive(accessible, method, far = null) {
  const full = accessible.length === FLASH_SIZE - APP_START;
  requireValue(accessible.length === APP_BYTES || (full && !far), 'Incomplete accessible flash region.');
  if (far) {
    requireValue(far.length === FAR_BYTES, 'Incomplete far firmware region.');
    const combined = new Uint8Array(APP_BYTES + FAR_BYTES);
    combined.set(accessible);
    combined.set(far, APP_BYTES);
    accessible = combined;
  }
  const meta = {
    format: far ? 'h9toolbox-hid-backup-v3' : full ? 'h9toolbox-hid-backup-v1' : 'h9toolbox-hid-backup-v2',
    radio: 'TD-H9',
    start: APP_START,
    length: accessible.length,
    bootRegion: '0x0000..0x4fff unavailable through HID; not included',
    ...(far ? { farStart: FAR_START } : {}),
    ...(!full && !far ? { settingsRegion: '0xda000..0xfffff outside the main firmware backup' } : {}),
    method,
    sha256: await sha256(accessible),
  };
  const json = new TextEncoder().encode(JSON.stringify(meta));
  const archive = new Uint8Array(MAGIC.length + 4 + json.length + accessible.length);
  archive.set(MAGIC);
  new DataView(archive.buffer).setUint32(MAGIC.length, json.length, true);
  archive.set(json, MAGIC.length + 4);
  archive.set(accessible, MAGIC.length + 4 + json.length);
  return archive;
}

export async function loadHidArchive(archive, target) {
  requireValue(archive.length >= MAGIC.length + 4 && MAGIC.every((byte, index) => archive[index] === byte), 'Not an H9 HID backup archive.');
  const metaLength = new DataView(archive.buffer, archive.byteOffset).getUint32(MAGIC.length, true);
  requireValue(metaLength > 0 && metaLength < 4096 && MAGIC.length + 4 + metaLength <= archive.length, 'HID backup archive length is invalid.');
  const meta = JSON.parse(new TextDecoder().decode(archive.subarray(MAGIC.length + 4, MAGIC.length + 4 + metaLength)));
  const hasFar = meta.format === 'h9toolbox-hid-backup-v3';
  const length = { 'h9toolbox-hid-backup-v1': FLASH_SIZE - APP_START, 'h9toolbox-hid-backup-v2': APP_BYTES,
    'h9toolbox-hid-backup-v3': APP_BYTES + FAR_BYTES }[meta.format];
  requireValue(length && meta.start === APP_START && meta.length === length, 'Unexpected HID backup metadata.');
  requireValue(!hasFar || meta.farStart === FAR_START, 'Unexpected far firmware backup offset.');
  requireValue(archive.length === MAGIC.length + 4 + metaLength + length, 'HID backup archive length is invalid.');
  const accessible = archive.slice(MAGIC.length + 4 + metaLength);
  const hash = await sha256(accessible);
  requireValue(meta.sha256 === hash, 'HID backup archive checksum mismatch.');
  requireValue(target.length === IMAGE_SIZE, 'Select the official firmware package first.');
  const image = target.slice(); // protected boot bytes are a placeholder and never written through HID
  image.set(accessible.subarray(0, IMAGE_SIZE - APP_START), APP_START);
  return { image, accessible, hash, source: 'hid' };
}
