import { sha256 } from './firmware.mjs?v=20261106d';

export const TYPE_C_ZIP_URL = 'https://raw.githubusercontent.com/teedubyeah/TDH9/1ccb67c2684204b0097533486c545607caaf65b8/Firmware/1.0.38/H9_Update-Type-C_1.0.38.zip';
export const TYPE_C_ZIP_SHA256 = '1d1b6f87705f026dc5c5b118786d133cbc7f0589f606c8572581df981492155a';
const ZIP_SIZE = 7091957;
const FIRMWARE_PATH = 'H9_Update-Type-C/TD-H9-V1.0.38.fw';
const FIRMWARE_SIZE = 897440;

function requireValue(condition, message) { if (!condition) throw new Error(message); }

export async function extractPinnedStockFirmware(zip) {
  requireValue(zip.length === ZIP_SIZE && await sha256(zip) === TYPE_C_ZIP_SHA256,
    'GitHub Type-C v1.0.38 ZIP does not match the pinned SHA-256.');
  const view = new DataView(zip.buffer, zip.byteOffset, zip.byteLength);
  let end = -1;
  for (let offset = zip.length - 22; offset >= Math.max(0, zip.length - 65557); offset--) {
    if (view.getUint32(offset, true) === 0x06054b50 && offset + 22 + view.getUint16(offset + 20, true) === zip.length) {
      end = offset; break;
    }
  }
  requireValue(end >= 0, 'ZIP central directory is missing.');
  const count = view.getUint16(end + 10, true);
  const directorySize = view.getUint32(end + 12, true);
  let offset = view.getUint32(end + 16, true);
  requireValue(view.getUint16(end + 4, true) === 0 && view.getUint16(end + 6, true) === 0 &&
    view.getUint16(end + 8, true) === count && count > 0 && count <= 100 &&
    offset + directorySize === end, 'Unsupported ZIP directory layout.');
  const directoryEnd = end;
  let entry = null;
  for (let index = 0; index < count; index++) {
    requireValue(offset + 46 <= directoryEnd && view.getUint32(offset, true) === 0x02014b50,
      'Invalid ZIP file entry.');
    const nameLength = view.getUint16(offset + 28, true);
    const extraLength = view.getUint16(offset + 30, true);
    const commentLength = view.getUint16(offset + 32, true);
    const next = offset + 46 + nameLength + extraLength + commentLength;
    requireValue(next <= directoryEnd, 'ZIP file entry extends past directory.');
    const name = new TextDecoder().decode(zip.subarray(offset + 46, offset + 46 + nameLength));
    if (name === FIRMWARE_PATH) {
      requireValue(!entry, 'Duplicate stock firmware in ZIP.');
      entry = {
        flags: view.getUint16(offset + 8, true),
        method: view.getUint16(offset + 10, true),
        compressed: view.getUint32(offset + 20, true),
        size: view.getUint32(offset + 24, true),
        local: view.getUint32(offset + 42, true),
      };
    }
    offset = next;
  }
  requireValue(offset === directoryEnd && entry, 'Pinned Type-C firmware file is missing from ZIP.');
  requireValue((entry.flags & 1) === 0 && entry.method === 8 && entry.size === FIRMWARE_SIZE,
    'Unexpected stock firmware ZIP compression or size.');
  requireValue(entry.local + 30 <= zip.length && view.getUint32(entry.local, true) === 0x04034b50,
    'Stock firmware local ZIP header is invalid.');
  const localName = view.getUint16(entry.local + 26, true);
  const localExtra = view.getUint16(entry.local + 28, true);
  const dataStart = entry.local + 30 + localName + localExtra;
  requireValue(dataStart + entry.compressed <= offset &&
    new TextDecoder().decode(zip.subarray(entry.local + 30, entry.local + 30 + localName)) === FIRMWARE_PATH,
    'Stock firmware ZIP entry is out of bounds.');
  let firmware;
  try {
    const stream = new Blob([zip.subarray(dataStart, dataStart + entry.compressed)])
      .stream().pipeThrough(new DecompressionStream('deflate-raw'));
    firmware = new Uint8Array(await new Response(stream).arrayBuffer());
  } catch (error) {
    throw new Error(`Browser could not decompress the Type-C firmware ZIP: ${error.message}`);
  }
  requireValue(firmware.length === FIRMWARE_SIZE, 'Extracted stock firmware size is invalid.');
  return firmware;
}
