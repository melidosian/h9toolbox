// TD-H9 v1.0.38 HID updater. The package is supplied locally and never uploaded.
export const APP_START = 0x5000;
export const IMAGE_SIZE = 0xda000;
export const FLASH_SIZE = 0x100000;
export const SECTOR_SIZE = 4096;
export const STOCK_PACKAGE_SHA256 = 'c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6';
export const STOCK_IMAGE_SHA256 = '74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648';
// Hosted build: H9 Toolbox Firmware with the app (module) loader: stock
// v1.0.38, the resource-flash service (boot logo, App Manager uploads) and the
// loader that runs apps from the resource chip (hold Back), which also keeps
// the last GPS fix on the resource chip (sector 0x3FF000). Built by
// firmware-research/module-loader (build.py; installer files by
// release_installer.mjs). Same file name as before: the Doom installer
// fetches it as a reference image.
export const BUILD = {
  label: 'H9 Toolbox Firmware',
  packageFile: 'h9-toolbox-firmware-v1.0.38.fw',
  packageSha256: 'dceac70c056a8c799aa68107c0f5d51154e2108a1a07941aececbddf05745339',
  imageSha256: '2f5e428d307a9f224cecbbd6c010ae84a70f9e7fc9c3be18c01f40d28ac97da6',
  // Stock bytes of the sectors the build changes: rebuilds exact stock.
  stockSeedFile: 'h9-toolbox-firmware-stock-sectors.bin',
  stockSeedSha256: 'e93a1c99e88902c75ee9f74d83255f9abe490acb389e519c0f8521260633116e',
  // Every sector any H9 Toolbox image differs from stock in; installs and
  // restores write only these.
  sectors: [0x5000, 0x62000, 0x64000, 0x8d000, 0xaa000, 0xac000, 0xae000, 0xaf000, 0xd9000],
};
// Earlier builds, recognised so a radio running one is backed up quickly and
// upgraded directly: each seed holds that image's bytes at the sectors where it
// differs from BUILD.
export const PREVIOUS = [
  { id: 'toolbox1', label: 'Brick Breaker build of H9 Toolbox Firmware', file: 'H9-Toolbox-Firmware-1-',
    seedFile: 'h9-toolbox-firmware-1-sectors.bin',
    seedSha256: '0463cf0273403d70e1ccf2f3eab12451534e114d79f8a8fdb361074abd84f7f0',
    imageSha256: '83f79fb3281c37ab22a4fd3f154a6c52a50d3cf0d8063b546eb47ce3c603bac6',
    sectors: [0x5000, 0x64000, 0x8d000, 0xac000, 0xae000, 0xd9000] },
  { id: 'original', label: 'original Brick Breaker', file: 'Brick-Breaker-',
    seedFile: 'brick-breaker-original-sectors.bin',
    seedSha256: 'cebebf7c4dbdb6160438494f93a192db6b4c518c83db805dcc60452ba4ab09a9',
    imageSha256: '741bbeb87eb84cd7c6988054c56a267bb73c89bbe5cc74dc4e5bc0d75ce7e193',
    sectors: [0x5000, 0x62000, 0x64000, 0x8d000, 0xaa000, 0xac000, 0xae000, 0xaf000, 0xd9000] },
  { id: 'preview', label: 'H9 Toolbox Firmware app-loader preview', file: 'H9-Toolbox-Firmware-preview-',
    seedFile: 'h9-toolbox-firmware-preview-sectors.bin',
    seedSha256: 'a4d4ed7e58de2e5a16c499a3ef7a02481249438101321e2514ba757c401c36ab',
    imageSha256: '79858310c73879a360e3f8c0cfb003a0043d74593a05421ddeb6f81b728bbcc1',
    sectors: [0x5000, 0xac000, 0xd9000] },
  { id: 'release1', label: 'H9 Toolbox Firmware (app loader, first release)', file: 'H9-Toolbox-Firmware-release1-',
    seedFile: 'h9-toolbox-firmware-release1-sectors.bin',
    seedSha256: '9a7c37551d1ed404c3ff6f7de238e48b11876b42eef8390d1ca2f5eed241a19a',
    imageSha256: '08d2409af07979f46b1591e28c04e0c86c6575ef97150f23d153bf5832f9b160',
    sectors: [0x5000, 0xac000, 0xd9000] },
];
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

async function loadPackage(bytes, expectedPackageHash, expectedImageHash, kind) {
  requireValue(bytes.length >= 0x200 && bytes.length <= 16 * 1024 * 1024, 'Invalid firmware package size.');
  const packageHash = await sha256(bytes);
  requireValue(packageHash === expectedPackageHash, `This is not the verified ${kind} TD-H9 v1.0.38 package.`);
  const image = decodePackage(bytes);
  requireValue(image.length === IMAGE_SIZE && await sha256(image) === expectedImageHash, `Firmware image does not match the verified ${kind} image.`);
  return { image, packageHash, kind };
}

// The flash.bin image of any well-formed TD-H9 .fw package (header, file list
// and image CRCs checked; no hash pin). Callers pin or check what they need.
export function decodePackage(bytes) {
  requireValue(bytes.length >= 0x200 && bytes.length <= 16 * 1024 * 1024, 'Invalid firmware package size.');
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
  requireValue(image, 'No firmware image (flash.bin) in the package.');
  return image;
}

export function loadStockPackage(bytes) {
  return loadPackage(bytes, STOCK_PACKAGE_SHA256, STOCK_IMAGE_SHA256, 'stock');
}

export function loadBuildPackage(bytes) {
  return loadPackage(bytes, BUILD.packageSha256, BUILD.imageSha256, 'build');
}

// Replace `sectors` of a verified image with hosted bytes and check the result.
async function rebuildImage(base, baseSha256, seed, seedSha256, sectors, resultSha256, label) {
  requireValue(seed.length === sectors.length * SECTOR_SIZE && await sha256(seed) === seedSha256,
    `Hosted ${label} sectors are corrupt or unexpected.`);
  requireValue(base.length === IMAGE_SIZE && await sha256(base) === baseSha256, 'Build image is unexpected.');
  const image = base.slice();
  sectors.forEach((physical, index) => image.set(seed.subarray(index * SECTOR_SIZE, (index + 1) * SECTOR_SIZE), physical));
  requireValue(await sha256(image) === resultSha256, `Hosted sectors do not rebuild the exact ${label} image.`);
  return image;
}

export function loadStockSeed(bytes, buildImage) {
  return rebuildImage(buildImage, BUILD.imageSha256, bytes, BUILD.stockSeedSha256, BUILD.sectors, STOCK_IMAGE_SHA256, 'stock v1.0.38');
}

export function loadPreviousImage(bytes, buildImage, previous) {
  return rebuildImage(buildImage, BUILD.imageSha256, bytes, previous.seedSha256, previous.sectors, previous.imageSha256, previous.label);
}

// Sectors (below IMAGE_SIZE) where the radio's CRCs differ from exact stock.
export function stockDifferences(current, stock) {
  const result = [];
  for (let physical = APP_START; physical < IMAGE_SIZE; physical += SECTOR_SIZE) {
    if (current[(physical - APP_START) / SECTOR_SIZE] !== crc16(stock.subarray(physical, physical + SECTOR_SIZE))) result.push(physical);
  }
  return result;
}

// "Stock-compatible": a radio (or backup) that is exact stock v1.0.38 in every
// sector the installer does not rewrite. Installing H9 Toolbox Firmware writes
// only BUILD.sectors, so whatever those hold (for example leftovers of an older
// firmware after an official update) is replaced, and the result is still the
// exact package. Returns the differing sectors (all pinned), or null.
export function stockCompatible(current, stock) {
  const differing = stockDifferences(current, stock);
  return differing.every(physical => BUILD.sectors.includes(physical)) ? differing : null;
}

// Flash holds the application encrypted: each 32-byte block is XORed with a
// CRC stream (key 0xF181 ^ ((flash - 0x5000) >> 2), stepped per byte).
function decryptFlash(image) {
  const out = new Uint8Array(Math.max(0, image.length - APP_START));
  for (let flash = APP_START; flash < image.length; flash += 32) {
    let key = (0xf181 ^ ((flash - APP_START) >> 2)) & 0xffff;
    for (let i = 0; i < 32 && flash + i < image.length; i++) {
      out[flash - APP_START + i] = image[flash + i] ^ (key & 0xff);
      key = ((key << 1) ^ ((key & 0x8000) ? 0x1021 : 0)) & 0xffff;
    }
  }
  return out;
}

// A TD-H9 application carries its model name ("TD-H9", at app+0xB162A in
// v1.0.38); another model's does not. Full-image installs on an unrecognised
// radio require it, since the stock-compatibility check (which implies the
// model) no longer applies.
export function looksLikeH9(image) {
  const plain = decryptFlash(image), name = new TextEncoder().encode('TD-H9');
  outer: for (let i = 0; i + name.length <= plain.length; i++) {
    if (plain[i] !== name[0]) continue;
    for (let k = 1; k < name.length; k++) if (plain[i + k] !== name[k]) continue outer;
    return true;
  }
  return false;
}

export function verifyStockCompatible(image, stock) {
  requireValue(image.length === IMAGE_SIZE, 'Select a raw 0xDA000-byte full-flash backup.');
  const crcs = expectedCrcs(image);
  requireValue(stockCompatible(crcs, stock), `${BUILD.label} requires a stock v1.0.38 main-firmware backup (it differs from stock outside the sectors the install rewrites). No writes allowed.`);
}

export async function verifyStockBackup(image) {
  requireValue(image.length === IMAGE_SIZE && await sha256(image) === STOCK_IMAGE_SHA256,
    `${BUILD.label} requires an exact stock v1.0.38 main-firmware backup. No writes allowed.`);
}

// For a radio that matches no known image: which known image it is closest to
// and where it differs (sector addresses), so a report says what to look at.
export function describeMismatch(current, images) {
  let best = null;
  for (const [name, image] of images) {
    if (!image) continue;
    const differing = [];
    for (let physical = APP_START; physical < IMAGE_SIZE; physical += SECTOR_SIZE) {
      if (current[(physical - APP_START) / SECTOR_SIZE] !== crc16(image.subarray(physical, physical + SECTOR_SIZE))) differing.push(physical);
    }
    if (!best || differing.length < best.differing.length) best = { name, differing };
  }
  if (!best) return '';
  const label = best.name === 'stock' ? 'stock v1.0.38' : best.name === 'build' ? BUILD.label : best.name;
  const total = (IMAGE_SIZE - APP_START) / SECTOR_SIZE;
  const shown = best.differing.slice(0, 16).map(physical => `0x${physical.toString(16)}`).join(', ');
  return `Closest: ${label}, ${best.differing.length} of ${total} firmware sectors differ (${shown}${best.differing.length > 16 ? ', …' : ''}).`;
}

// Which known image the radio holds, by sector CRC: `images` is a list of
// [name, image] pairs.
export function classifyRadio(current, images) {
  requireValue(current.length === (FLASH_SIZE - APP_START) / SECTOR_SIZE,
    'Incomplete radio CRC scan.');
  const matches = image => {
    for (let physical = APP_START; physical < IMAGE_SIZE; physical += SECTOR_SIZE) {
      const index = (physical - APP_START) / SECTOR_SIZE;
      if (current[index] !== crc16(image.subarray(physical, physical + SECTOR_SIZE))) return false;
    }
    return true;
  };
  const found = images.find(([, image]) => image && matches(image));
  if (found) return found[0];
  throw new Error(`Radio is neither exact stock v1.0.38, ${BUILD.label}, nor an earlier H9 Toolbox build. No backup seed accepted. ${describeMismatch(current, images)}`);
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
  // Observed on the TD-H9: C4 uses 4 KiB CRC windows spaced 4 KiB apart,
  // even when block_size is one. An unaligned address shifts every window.
  // readback.mjs checks the behavior against known image or erased bytes.
  async windowCrcs(relative, count) {
    requireValue(Number.isInteger(relative) && relative >= 0 && count >= 1 && count <= 16 && relative + count * SECTOR_SIZE <= FLASH_SIZE - APP_START, 'Invalid sliding CRC request.');
    const data = await this.exchange(0xc4, u32(relative, count, 1));
    requireValue(data.length === count * 2, 'Invalid sliding CRC reply length.');
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

// `known` lists other exact images (earlier H9 Toolbox builds) whose sectors
// the radio may hold instead of the backup's or target's; their sectors are
// written too.
export function planUpdate(current, backup, target, resumeSector = null, known = []) {
  const original = expectedCrcs(backup), desired = expectedCrcs(target), changed = [];
  const others = known.map(expectedCrcs);
  requireValue(current.length === original.length, 'Incomplete radio CRC scan.');
  for (let index = 0; index < current.length; index++) {
    const physical = APP_START + index * SECTOR_SIZE;
    if (physical >= IMAGE_SIZE) continue; // HID cannot back up or verify retained VM bytes.
    const before = backup.subarray(physical, physical + SECTOR_SIZE);
    const after = target.subarray(physical, physical + SECTOR_SIZE);
    const otherDiffers = known.some(image => !same(before, image.subarray(physical, physical + SECTOR_SIZE)));
    if (!same(before, after) || otherDiffers) {
      const collision = (!same(before, after) && original[index] === desired[index]) ||
        known.some((image, k) => !same(after, image.subarray(physical, physical + SECTOR_SIZE)) && others[k][index] === desired[index]) ||
        known.some((image, k) => !same(before, image.subarray(physical, physical + SECTOR_SIZE)) && others[k][index] === original[index]);
      requireValue(!collision, `CRC collision at 0x${physical.toString(16)}; HID cannot verify this sector.`);
      changed.push(physical);
    }
    if (current[index] !== original[index] && current[index] !== desired[index] &&
        !others.some(crcs => crcs[index] === current[index]) && physical !== resumeSector) {
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
