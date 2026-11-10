// Doom for the TD-H9: install checks and plan (pure; used by doom.mjs and the
// tests). See doom.mjs for the page.

import {
  APP_START, FLASH_SIZE, H9Hid, IMAGE_SIZE, SECTOR_SIZE, crc16, decodePackage,
} from '../firmware-update/firmware.mjs?v=20261109d';

// The Doom code region: after the settings VM (0xDA000-0xE9FFF), written by no
// stock code. The firmware runs it in place, unencrypted, at 0x01EE5000.
export const FAR_START = 0xea000;
export const FAR_END = 0xfc000;
// Where "Download from GitHub" looks: a branch with the release files and
// doom-manifest.json (see firmware-research/doom-firmware/stage_release.py).
export const GITHUB = {
  base: 'https://raw.githubusercontent.com/melidosian/h9toolbox/doom-files/',
  manifest: 'doom-manifest.json',
};

export const BLANK_CRC = crc16(new Uint8Array(SECTOR_SIZE).fill(0xff));
function requireValue(condition, message) { if (!condition) throw new Error(message); }
const hex = value => `0x${value.toString(16)}`;

// ---- file checks (pure; also used by the tests)

export function checkFirmware(bytes) {
  const image = decodePackage(bytes);
  requireValue(image.length === IMAGE_SIZE, 'This .fw does not hold a TD-H9 main image.');
  return image;
}

export function checkDoomCode(bytes) {
  requireValue(bytes.length > 0 && bytes.length <= FAR_END - FAR_START,
    `The Doom code blob must be 1 to ${(FAR_END - FAR_START).toLocaleString()} bytes (it is written at ${hex(FAR_START)}).`);
  const padded = new Uint8Array(Math.ceil(bytes.length / SECTOR_SIZE) * SECTOR_SIZE).fill(0xff);
  padded.set(bytes);
  return padded;
}

export function checkPack(bytes) {
  requireValue(new TextDecoder().decode(bytes.subarray(0, 8)) === 'DMCDOOM\0', 'Not a Doom game-data pack (DMCDOOM header missing).');
  return bytes;
}

export function checkSong(bytes) {
  requireValue(bytes.length > 32 && new TextDecoder().decode(bytes.subarray(0, 4)) === 'H9MU', 'Not an H9MU song (made by doom-firmware/music.py).');
  return bytes;
}

// The whole flash target: main image, erased VM, Doom code.
export function doomTarget(image, code) {
  const whole = new Uint8Array(FAR_END).fill(0xff);
  whole.set(image, 0);
  whole.set(code, FAR_START);
  return whole;
}

// Sectors to write, from the radio's CRCs (scanRadio: one per sector from
// APP_START): the Doom code region first, then the firmware high to low
// (0x5000, the header sector, last).
export function planInstall(current, whole, codeLength) {
  requireValue(current.length === (FLASH_SIZE - APP_START) / SECTOR_SIZE, 'Incomplete radio CRC scan.');
  const crcAt = physical => current[(physical - APP_START) / SECTOR_SIZE];
  const want = physical => crc16(whole.subarray(physical, physical + SECTOR_SIZE));
  const code = [], firmware = [];
  for (let physical = FAR_START; physical < FAR_START + codeLength; physical += SECTOR_SIZE)
    if (crcAt(physical) !== want(physical)) code.push(physical);
  for (let physical = IMAGE_SIZE - SECTOR_SIZE; physical >= APP_START; physical -= SECTOR_SIZE)
    if (crcAt(physical) !== want(physical)) firmware.push(physical);
  return { code, firmware, sectors: [...code, ...firmware] };
}

export function inWritableArea(physical) {
  return physical % SECTOR_SIZE === 0 &&
    ((physical >= APP_START && physical < IMAGE_SIZE) || (physical >= FAR_START && physical < FAR_END));
}

// The web flasher's HID client, also allowed to erase and write the Doom
// code region. Never the boot region or the settings VM.
export class DoomHid extends H9Hid {
  async erase(physical) {
    requireValue(inWritableArea(physical), `Erase outside the firmware and Doom code areas (${hex(physical)}).`);
    const body = new Uint8Array(8), view = new DataView(body.buffer);
    view.setUint32(0, physical - APP_START, true);
    view.setUint32(4, 2, true);
    await this.exchange(0xc2, body);
  }
  async write(physical, offset, bytes) {
    requireValue(inWritableArea(physical) && offset >= 0 && offset + bytes.length <= SECTOR_SIZE && bytes.length >= 1 && bytes.length <= 40,
      'Write outside the firmware and Doom code areas.');
    const body = new Uint8Array(8 + bytes.length), view = new DataView(body.buffer);
    view.setUint32(0, physical - APP_START + offset, true);
    view.setUint32(4, bytes.length, true);
    body.set(bytes, 8);
    await this.exchange(0xc3, body);
  }
}

