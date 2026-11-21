import { APP_START, FLASH_SIZE, H9Hid, SECTOR_SIZE, crc16, writeSector } from './firmware.mjs';

export const FAR_START = 0xea000;
export const FAR_END = 0xfc000;
const FAR_SIZE = FAR_END - FAR_START;
const COUNT = FAR_SIZE / SECTOR_SIZE;
const SHIFTS = [1, 37, 1024, 2048, 3072, 4095];

export class FarFirmwareHid extends H9Hid {
  async erase(physical) {
    if (physical < FAR_START || physical >= FAR_END) return super.erase(physical);
    if (physical % SECTOR_SIZE !== 0) throw new Error('Unaligned far firmware erase.');
    const body = new Uint8Array(8);
    const view = new DataView(body.buffer);
    view.setUint32(0, physical - APP_START, true);
    view.setUint32(4, 2, true);
    await this.exchange(0xc2, body);
  }

  async write(physical, offset, bytes) {
    if (physical < FAR_START || physical >= FAR_END) return super.write(physical, offset, bytes);
    await this.writeFarFirmware(physical, offset, bytes);
  }
}

export function backedUpFar(accessible) {
  if (accessible?.length !== FLASH_SIZE - APP_START) return null;
  return accessible.subarray(FAR_START - APP_START, FAR_END - APP_START);
}

function sector(bytes, i) {
  return bytes.subarray(i * SECTOR_SIZE, (i + 1) * SECTOR_SIZE);
}

function equal(a, b) {
  return a.every((byte, i) => byte === b[i]);
}

async function verifyWindows(hid, bytes, interrupted = -1) {
  // Include windows that straddle each sector boundary in the managed region.
  for (const shift of SHIFTS) {
    const base = FAR_START - APP_START + shift;
    const observed = [
      ...await hid.windowCrcs(base, 16),
      ...await hid.windowCrcs(base + 16 * SECTOR_SIZE, COUNT - 17),
    ];
    if (observed.length !== COUNT - 1 || observed.some((crc, i) =>
      i !== interrupted && i + 1 !== interrupted &&
      crc !== crc16(bytes.subarray(i * SECTOR_SIZE + shift, (i + 1) * SECTOR_SIZE + shift))))
      throw new Error('The far firmware does not match the verified data. Check that you selected the original backup from this radio.');
  }
}

export async function planFarFirmware(hid, saved, release, direction, interruptedAddress) {
  if (saved?.length !== FAR_SIZE || release?.length !== FAR_SIZE)
    throw new Error('Select a recovery backup that includes the far firmware.');
  const base = FAR_START - APP_START;
  const observed = [...await hid.crcs(base, 16), ...await hid.crcs(base + 16 * SECTOR_SIZE, COUNT - 16)];
  if (observed.length !== COUNT) throw new Error('Incomplete far firmware checksum scan.');
  const destination = direction === 'restore' ? saved : release;
  const live = new Uint8Array(FAR_SIZE);
  const work = [];
  let interrupted = -1;
  for (let i = 0; i < COUNT; i++) {
    const address = FAR_START + i * SECTOR_SIZE;
    const before = sector(saved, i);
    const next = sector(release, i);
    const beforeCrc = crc16(before);
    const nextCrc = crc16(next);
    if (beforeCrc === nextCrc && !equal(before, next))
      throw new Error('HID cannot distinguish a far firmware sector from the backup.');
    if (observed[i] === nextCrc) live.set(next, i * SECTOR_SIZE);
    else if (observed[i] === beforeCrc) live.set(before, i * SECTOR_SIZE);
    else if (address === interruptedAddress) interrupted = i;
    else throw new Error('The far firmware differs from both the recovery backup and the preview. Select the original backup from this radio.');
    if (observed[i] !== crc16(sector(destination, i))) work.unshift(address);
  }
  await verifyWindows(hid, live, interrupted);
  return { destination, work };
}

export async function writeFarFirmware(hid, plan, pending, progress) {
  const image = new Uint8Array(FAR_END);
  image.set(plan.destination, FAR_START);
  progress(0, plan.work.length);
  for (const [index, address] of plan.work.entries()) {
    pending(address);
    await writeSector(hid, address, image);
    pending(null);
    progress(index + 1, plan.work.length);
  }
  const base = FAR_START - APP_START;
  const observed = [...await hid.crcs(base, 16), ...await hid.crcs(base + 16 * SECTOR_SIZE, COUNT - 16)];
  if (observed.length !== COUNT || observed.some((crc, i) => crc !== crc16(sector(plan.destination, i))))
    throw new Error('Final far firmware checksum verification failed.');
  await verifyWindows(hid, plan.destination);
}
