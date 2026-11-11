import { APP_START, FLASH_SIZE, IMAGE_SIZE, SECTOR_SIZE, crc16, scanRadio, sha256 } from './firmware.mjs?v=20261111f';

const SECTORS = (FLASH_SIZE - APP_START) / SECTOR_SIZE;
const BOUNDARIES = SECTORS - 1;
const APP_SECTORS = (IMAGE_SIZE - APP_START) / SECTOR_SIZE;
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
for (let crc = 0; crc < 65536; crc++) inverseZeroStep[step(crc, 0)] = crc;
for (let byte = 0; byte < 256; byte++) {
  byteCrc[byte] = step(0, byte);
  byteEffect[byteCrc[byte]] = byte;
  let contribution = byteCrc[byte];
  for (let i = 1; i < SECTOR_SIZE; i++) contribution = step(contribution, 0);
  removeCrc[byte] = contribution;
  removeEffect[contribution] = byte;
}

// Each CRC covers 4096 bytes. Shift a window by one byte and cancel the
// outgoing byte's CRC contribution to recover the incoming byte exactly.
export function recoverNextSector(previous, windows, nextSectorCrc) {
  requireValue(previous.length === SECTOR_SIZE && windows.length === SECTOR_SIZE, 'Invalid CRC recovery input.');
  const result = new Uint8Array(SECTOR_SIZE);
  for (let offset = 0; offset < SECTOR_SIZE; offset++) {
    const rest = windows[offset] ^ removeCrc[previous[offset]];
    const observed = offset === SECTOR_SIZE - 1 ? nextSectorCrc : windows[offset + 1];
    const value = byteEffect[observed ^ step(rest, 0)];
    requireValue(value >= 0, `CRC recovery failed at byte ${offset}.`);
    result[offset] = value;
  }
  requireValue(crc16(result) === nextSectorCrc, 'Recovered sector CRC mismatch.');
  return result;
}

export function recoverPreviousSector(next, windows, previousSectorCrc) {
  requireValue(next.length === SECTOR_SIZE && windows.length === SECTOR_SIZE, 'Invalid CRC recovery input.');
  requireValue(windows[0] === previousSectorCrc, 'Previous sector CRC mismatch.');
  const result = new Uint8Array(SECTOR_SIZE);
  const nextSectorCrc = crc16(next);
  for (let offset = SECTOR_SIZE - 1; offset >= 0; offset--) {
    const observedNext = offset === SECTOR_SIZE - 1 ? nextSectorCrc : windows[offset + 1];
    const rest = inverseZeroStep[observedNext ^ byteCrc[next[offset]]];
    const value = removeEffect[windows[offset] ^ rest];
    requireValue(value >= 0, `Reverse CRC recovery failed at byte ${offset}.`);
    result[offset] = value;
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

async function collectWindows(hid, crcs, startBoundary, onProgress, cancelled) {
  const rows = Array(BOUNDARIES).fill(null);
  const groups = Math.ceil((BOUNDARIES - startBoundary) / 16);
  const total = groups * (SECTOR_SIZE - 1);
  let done = 0;
  for (let base = startBoundary; base < BOUNDARIES; base += 16) {
    const count = Math.min(16, BOUNDARIES - base);
    for (let index = 0; index < count; index++) {
      rows[base + index] = new Uint16Array(SECTOR_SIZE);
      rows[base + index][0] = crcs[base + index];
    }
    for (let shift = 1; shift < SECTOR_SIZE; shift++) {
      if (cancelled()) throw new Error('HID backup cancelled. No radio write was sent.');
      const values = await hid.windowCrcs(base * SECTOR_SIZE + shift, count);
      for (let index = 0; index < count; index++) rows[base + index][shift] = values[index];
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
  const rows = await collectWindows(hid, crcs, startBoundary, onProgress, cancelled);
  if (startBoundary === 0) {
    for (let shift = 0; shift < SECTOR_SIZE; shift++) requireValue(rows[seedIndex][shift] === BLANK_CRC, 'Blank seed has inconsistent sliding CRCs.');
    for (let index = seedIndex - 1; index >= 0; index--) sectors[index] = recoverPreviousSector(sectors[index + 1], rows[index], crcs[index]);
  }
  for (let index = seedIndex; index < BOUNDARIES; index++) {
    if (sectors[index + 1]) continue;
    sectors[index + 1] = recoverNextSector(sectors[index], rows[index], crcs[index + 1]);
  }
  const accessible = new Uint8Array(FLASH_SIZE - APP_START);
  for (let index = 0; index < SECTORS; index++) {
    requireValue(sectors[index] && crc16(sectors[index]) === crcs[index], `HID backup sector 0x${(APP_START + index * SECTOR_SIZE).toString(16)} failed verification.`);
    accessible.set(sectors[index], index * SECTOR_SIZE);
  }
  const after = await scanRadio(hid);
  requireValue(after.every((value, index) => value === crcs[index]), 'Radio CRCs changed during backup; start again.');
  return { accessible, method, settings: true };
}

// v1 archives hold the whole accessible flash (0x5000..0xFFFFF); v2 archives
// hold only the application area (0x5000..0xD9FFF), the part this page writes.
export async function createHidArchive(accessible, method) {
  const full = accessible.length === FLASH_SIZE - APP_START;
  requireValue(full || accessible.length === IMAGE_SIZE - APP_START, 'Incomplete accessible flash region.');
  const meta = {
    format: full ? 'h9toolbox-hid-backup-v1' : 'h9toolbox-hid-backup-v2',
    radio: 'TD-H9',
    start: APP_START,
    length: accessible.length,
    bootRegion: '0x0000..0x4fff unavailable through HID; not included',
    ...(full ? {} : { settingsRegion: '0xda000..0xfffff not included (never written by this page)' }),
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
  const length = { 'h9toolbox-hid-backup-v1': FLASH_SIZE - APP_START, 'h9toolbox-hid-backup-v2': IMAGE_SIZE - APP_START }[meta.format];
  requireValue(length && meta.start === APP_START && meta.length === length, 'Unexpected HID backup metadata.');
  requireValue(archive.length === MAGIC.length + 4 + metaLength + length, 'HID backup archive length is invalid.');
  const accessible = archive.slice(MAGIC.length + 4 + metaLength);
  const hash = await sha256(accessible);
  requireValue(meta.sha256 === hash, 'HID backup archive checksum mismatch.');
  requireValue(target.length === IMAGE_SIZE, 'Select the official firmware package first.');
  const image = target.slice(); // protected boot bytes are a placeholder and never written through HID
  image.set(accessible.subarray(0, IMAGE_SIZE - APP_START), APP_START);
  return { image, accessible, hash, source: 'hid' };
}
