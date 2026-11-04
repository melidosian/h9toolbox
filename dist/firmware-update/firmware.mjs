// TD-H9 v1.0.38 HID updater. The package is supplied locally and never uploaded.
export const APP_START = 0x5000;
export const IMAGE_SIZE = 0xda000;
export const FLASH_SIZE = 0x100000;
export const SECTOR_SIZE = 4096;
export const STOCK_PACKAGE_SHA256 = 'c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6';
export const STOCK_IMAGE_SHA256 = '74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648';
const KEY = new Uint8Array([0x4e, 0x61, 0xbc, 0x00]); // 12345678, little endian

export function crc16(bytes) {
  let crc = 0;
  for (const value of bytes) {
    crc ^= value << 8;
    for (let bit = 0; bit < 8; bit++) crc = ((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0)) & 0xffff;
  }
  return crc;
}

export async function sha256(bytes) {
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(digest), value => value.toString(16).padStart(2, '0')).join('');
}

function same(a, b) { return a.length === b.length && a.every((value, index) => value === b[index]); }
function requireValue(condition, message) { if (!condition) throw new Error(message); }

export async function loadStockPackage(bytes) {
  requireValue(bytes.length >= 0x200 && bytes.length <= 16 * 1024 * 1024, 'Invalid firmware package size.');
  const packageHash = await sha256(bytes);
  requireValue(packageHash === STOCK_PACKAGE_SHA256, 'Only the verified official TD-H9 v1.0.38 .fw package is supported.');
  const header = bytes.slice(0, 0x200);
  let key = 0xf181;
  for (let index = 0; index < 0x40; index++) {
    header[index] ^= key & 0xff;
    key = ((key << 1) ^ ((key & 0x8000) ? 0x1021 : 0)) & 0xffff;
  }
  const view = new DataView(header.buffer);
  requireValue(crc16(header.slice(2, 0x40)) === view.getUint16(0, true), 'Firmware header CRC is invalid.');
  requireValue(view.getUint32(4, true) === bytes.length && view.getUint32(12, true) === 0x200, 'Firmware package length is invalid.');
  requireValue(new TextDecoder().decode(header.slice(16, 22)) === 'AC695X', 'Firmware chip type is not AC695X.');
  const count = view.getUint16(8, true);
  requireValue(count >= 1 && count <= 6, 'Firmware file count is invalid.');
  requireValue(crc16(header.slice(0x40, 0x40 + 0x50 * count)) === view.getUint16(2, true), 'Firmware file list CRC is invalid.');
  let image;
  for (let entry = 0; entry < count; entry++) {
    const record = bytes.slice(0x40 + entry * 0x50, 0x40 + (entry + 1) * 0x50);
    key = 0xf181;
    for (let index = 0; index < record.length; index++) {
      record[index] ^= key & 0xff;
      key = ((key << 1) ^ ((key & 0x8000) ? 0x1021 : 0)) & 0xffff;
    }
    const rv = new DataView(record.buffer);
    const name = new TextDecoder().decode(record.slice(64, 80)).split('\0')[0];
    if (rv.getUint16(0, true) === 0 && name === 'flash.bin') {
      requireValue(!image, 'Duplicate firmware image.');
      const offset = rv.getUint32(8, true), size = rv.getUint32(12, true);
      requireValue(offset >= 0x200 && offset + size <= bytes.length, 'Firmware image extends past the package.');
      image = bytes.slice(offset, offset + size);
      requireValue(crc16(image) === rv.getUint16(4, true), 'Firmware image CRC is invalid.');
    }
  }
  requireValue(image?.length === IMAGE_SIZE && await sha256(image) === STOCK_IMAGE_SHA256, 'Firmware image does not match the verified v1.0.38 image.');
  return { image, packageHash };
}

export async function loadBackup(bytes, target) {
  requireValue(bytes.length === IMAGE_SIZE, 'Select a raw 0xDA000-byte full-flash backup, not a Bluetooth .img.');
  requireValue(same(bytes.slice(0, APP_START), target.slice(0, APP_START)), 'Boot region differs. HID cannot update the first 0x5000 bytes.');
  // The first 32 bytes contain the encrypted flash header. Exact equality above
  // checks product ID, flash capacity and chip metadata without reinterpreting it.
  return { image: bytes, hash: await sha256(bytes) };
}

export function makePacket(command, body = new Uint8Array()) {
  requireValue(body.length <= 56, 'HID payload is too long.');
  const report = new Uint8Array(64);
  const view = new DataView(report.buffer);
  report.set([0xaa, 0x55]);
  view.setUint16(2, body.length + 2, true);
  report[4] = command;
  report.set(body, 6);
  for (let i = 4; i < 6 + body.length; i++) report[i] ^= KEY[(i - 4) % 4];
  view.setUint16(6 + body.length, crc16(report.slice(0, 6 + body.length)), true);
  return report;
}

export function parseReply(report, command) {
  requireValue(report.length === 64 && report[0] === 0xaa && report[1] === 0x55, 'Malformed HID reply.');
  const view = new DataView(report.buffer, report.byteOffset, report.byteLength);
  const length = view.getUint16(2, true), end = 4 + length;
  requireValue(length >= 2 && length <= 58, 'Invalid HID reply length.');
  requireValue(crc16(report.slice(0, end)) === view.getUint16(end, true), 'HID reply CRC mismatch.');
  const content = report.slice(4, end);
  for (let i = 0; i < content.length; i++) content[i] ^= KEY[i % 4];
  requireValue(content[0] === command, 'Unexpected HID reply command.');
  requireValue(content[1] === 0, `Radio rejected HID command 0x${command.toString(16)} (status ${content[1]}).`);
  return content.slice(2);
}

function u32(...values) {
  const data = new Uint8Array(values.length * 4), view = new DataView(data.buffer);
  values.forEach((value, index) => view.setUint32(index * 4, value, true));
  return data;
}

export class H9Hid {
  constructor(device) { this.device = device; }
  async exchange(command, body = new Uint8Array()) {
    const device = this.device;
    let cancel;
    const reply = new Promise((resolve, reject) => {
      const timer = setTimeout(() => { device.removeEventListener('inputreport', onReport); reject(new Error(`Radio timed out on command 0x${command.toString(16)}.`)); }, 5000);
      function onReport(event) {
        if (event.reportId !== 0) return;
        clearTimeout(timer);
        device.removeEventListener('inputreport', onReport);
        try { resolve(parseReply(new Uint8Array(event.data.buffer, event.data.byteOffset, event.data.byteLength), command)); }
        catch (error) { reject(error); }
      }
      device.addEventListener('inputreport', onReport);
      cancel = () => { clearTimeout(timer); device.removeEventListener('inputreport', onReport); };
    });
    try { await device.sendReport(0, makePacket(command, body)); }
    catch (error) {
      cancel();
      throw error;
    }
    return reply;
  }
  async initialize() {
    const body = new Uint8Array(17);
    body.set(new TextEncoder().encode('app_dir_head'));
    const result = await this.exchange(0xc0, body);
    requireValue(result.length === 16, 'Invalid update geometry reply.');
    const view = new DataView(result.buffer);
    const geometry = { address: view.getUint32(0, true), length: view.getUint32(4, true), eraseOffset: view.getUint32(8, true), blockSize: view.getUint32(12, true) };
    // The observed recovery tool checks address, erase offset and sector size.
    // The length describes the named app_dir_head file, not a proven flash limit.
    requireValue(geometry.address === APP_START && geometry.eraseOffset === 0 && geometry.blockSize === SECTOR_SIZE, 'Unexpected radio update geometry.');
    return geometry;
  }
  async crcs(relative, count) {
    requireValue(relative % SECTOR_SIZE === 0 && count >= 1 && count <= 16 && relative + count * SECTOR_SIZE <= FLASH_SIZE - APP_START, 'Invalid CRC request.');
    const data = await this.exchange(0xc4, u32(relative, count * SECTOR_SIZE, SECTOR_SIZE));
    requireValue(data.length === count * 2, 'Invalid CRC reply length.');
    const view = new DataView(data.buffer);
    return Array.from({ length: count }, (_, index) => view.getUint16(index * 2, true));
  }
  async sectorCrc(physical) { return (await this.crcs(physical - APP_START, 1))[0]; }
  async erase(physical) { requireValue(physical >= APP_START && physical < IMAGE_SIZE && physical % SECTOR_SIZE === 0, 'Erase outside firmware area.'); await this.exchange(0xc2, u32(physical - APP_START, 2)); }
  async write(physical, offset, bytes) {
    requireValue(physical >= APP_START && physical < IMAGE_SIZE && offset >= 0 && offset + bytes.length <= SECTOR_SIZE && bytes.length >= 1 && bytes.length <= 40, 'Write outside firmware sector.');
    const body = new Uint8Array(8 + bytes.length);
    body.set(u32(physical - APP_START + offset, bytes.length)); body.set(bytes, 8);
    await this.exchange(0xc3, body);
  }
}

export function expectedCrcs(image) {
  const result = [];
  for (let physical = APP_START; physical < FLASH_SIZE; physical += SECTOR_SIZE) {
    result.push(physical < image.length ? crc16(image.slice(physical, physical + SECTOR_SIZE)) : null);
  }
  return result;
}

export async function scanRadio(hid, onProgress = () => {}) {
  const result = [];
  for (let relative = 0; relative < FLASH_SIZE - APP_START; relative += SECTOR_SIZE * 16) {
    const count = Math.min(16, (FLASH_SIZE - APP_START - relative) / SECTOR_SIZE);
    result.push(...await hid.crcs(relative, count));
    onProgress(result.length, (FLASH_SIZE - APP_START) / SECTOR_SIZE);
  }
  return result;
}

export function planUpdate(current, backup, target, resumeSector = null) {
  const original = expectedCrcs(backup), desired = expectedCrcs(target), changed = [];
  requireValue(current.length === original.length, 'Incomplete radio CRC scan.');
  for (let index = 0; index < current.length; index++) {
    const physical = APP_START + index * SECTOR_SIZE;
    if (physical >= IMAGE_SIZE) continue; // HID cannot back up or verify retained VM bytes.
    const before = backup.subarray(physical, physical + SECTOR_SIZE);
    const after = target.subarray(physical, physical + SECTOR_SIZE);
    if (!same(before, after)) {
      requireValue(original[index] !== desired[index], `CRC collision at 0x${physical.toString(16)}; HID cannot verify this sector.`);
      changed.push(physical);
    }
    if (current[index] !== original[index] && current[index] !== desired[index] && physical !== resumeSector) {
      throw new Error(`Radio differs from the supplied backup at 0x${physical.toString(16)}. No writes allowed.`);
    }
  }
  return changed.sort((a, b) => b - a);
}

export async function writeSector(hid, physical, target) {
  const sector = target.slice(physical, physical + SECTOR_SIZE);
  await hid.erase(physical);
  requireValue(await hid.sectorCrc(physical) === crc16(new Uint8Array(SECTOR_SIZE).fill(0xff)), `Erase verification failed at 0x${physical.toString(16)}.`);
  const ranges = physical === APP_START ? [[32, SECTOR_SIZE], [0, 32]] : [[0, SECTOR_SIZE]];
  for (const [start, end] of ranges) for (let offset = start; offset < end; offset += 40) {
    const part = sector.slice(offset, Math.min(offset + 40, end));
    if (!part.every(value => value === 0xff)) await hid.write(physical, offset, part);
  }
  requireValue(await hid.sectorCrc(physical) === crc16(sector), `Write verification failed at 0x${physical.toString(16)}.`);
}
