import { APP_START, APRS_STAGE_START, APRS_STAGE_END, BUILD, SECTOR_SIZE, crc16, sha256 } from './firmware.mjs?v=20261119h';

const SIZE = APRS_STAGE_END - APRS_STAGE_START;
const CHUNK_SIZE = 40;
const SHIFTS = [1, 37, 1024, 2048, 3072, 4095];
const BLANK = new Uint8Array(SECTOR_SIZE).fill(0xff);
const BLANK_CRC = crc16(BLANK);

export async function loadAprsStage(bytes) {
  if (bytes.length !== SIZE || await sha256(bytes) !== BUILD.aprsStageSha256)
    throw new Error('The staged APRS file did not match the firmware release. Reload the page and try again.');
  return bytes;
}

// A previously completed 40-byte report can be resumed, with the existing
// prefix confirmed by both aligned and shifted CRCs. Ambiguous matches are
// rejected. A torn report inside a chunk is left alone.
function matchingPrefix(wanted, liveCrc) {
  const expected = crc16(wanted);
  if (expected === BLANK_CRC && wanted.some(byte => byte !== 0xff)) return null;
  if (liveCrc === expected) return SECTOR_SIZE;
  if (liveCrc === BLANK_CRC) return 0;
  const prefix = BLANK.slice();
  let match = null;
  for (let pos = 0; pos + CHUNK_SIZE < SECTOR_SIZE; pos += CHUNK_SIZE) {
    const chunk = wanted.subarray(pos, pos + CHUNK_SIZE);
    if (chunk.every(byte => byte === 0xff)) continue;
    prefix.set(chunk, pos);
    if (crc16(prefix) !== liveCrc) continue;
    if (match !== null) return null;
    match = pos + CHUNK_SIZE;
  }
  return match;
}

async function inspectStage(hid, bytes, start, label) {
  const base = start - APP_START;
  const count = bytes.length / SECTOR_SIZE;
  const observed = await hid.crcs(base, count);
  if (observed.length !== count) throw new Error(`Incomplete ${label} checksum scan.`);
  const live = new Uint8Array(bytes.length).fill(0xff);
  const plan = [];
  for (let i = 0; i < count; i++) {
    const offset = i * SECTOR_SIZE;
    const wanted = bytes.subarray(offset, offset + SECTOR_SIZE);
    const prefix = matchingPrefix(wanted, observed[i]);
    if (prefix === null) return null;
    live.set(wanted.subarray(0, prefix), offset);
    if (prefix < SECTOR_SIZE) plan.push({ offset, prefix });
  }
  for (const shift of SHIFTS) {
    const actual = await hid.windowCrcs(base + shift, count - 1);
    if (actual.length !== count - 1) throw new Error(`Incomplete ${label} window scan.`);
    if (actual.some((crc, i) => crc !== crc16(live.subarray(i * SECTOR_SIZE + shift, (i + 1) * SECTOR_SIZE + shift))))
      return null;
  }
  return plan.reverse(); // The executable lives in the first sector and is written last.
}

export async function stageAprs(hid, bytes, progress = () => {}) {
  await loadAprsStage(bytes);
  return stageRegion(hid, bytes, APRS_STAGE_START, 'APRS staging area', hid.writeAprsStage.bind(hid), progress);
}

// These reserved regions only accept programming into erased bytes. Readback
// validates any existing prefix before continuing a previous attempt.
export async function stageRegion(hid, bytes, start, label, write, progress) {
  const plan = await inspectStage(hid, bytes, start, label);
  if (!plan) return 'occupied';
  if (!plan.length) return 'ready';
  progress(0, plan.length);
  for (const [i, { offset, prefix }] of plan.entries()) {
    const physical = start + offset;
    const sector = bytes.subarray(offset, offset + SECTOR_SIZE);
    for (let pos = prefix; pos < SECTOR_SIZE; pos += CHUNK_SIZE) {
      const chunk = sector.subarray(pos, pos + CHUNK_SIZE);
      if (chunk.some(byte => byte !== 0xff)) await write(physical, pos, chunk);
    }
    if (await hid.sectorCrc(physical) !== crc16(sector))
      throw new Error(`A sector in the ${label} failed its checksum. Keep USB connected and try Install again.`);
    progress(i + 1, plan.length);
  }
  const verified = await inspectStage(hid, bytes, start, label);
  if (!verified || verified.length) throw new Error(`The ${label} failed its final verification.`);
  return 'ready';
}
