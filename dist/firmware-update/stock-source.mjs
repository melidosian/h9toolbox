import { APP_START, SECTOR_SIZE, decodePackage, sha256 } from './firmware.mjs?v=20261127stock';
import { STOCK_RELEASES, STOCK_SIGNATURE_SECTORS } from './stock-catalog.mjs';

const currentStock = STOCK_RELEASES.find(release => release.version === '1.0.38');
export const TYPE_C_ZIP_URL = currentStock.url;
export const TYPE_C_ZIP_SHA256 = currentStock.zipSha256;

function requireValue(condition, message) { if (!condition) throw new Error(message); }

export function stockReferenceForScan(crcs) {
  // A near match only selects a download. The backup verifies each reused
  // sector against the radio before trusting the reference bytes.
  const matches = STOCK_RELEASES.filter(release =>
    STOCK_SIGNATURE_SECTORS.filter((address, index) =>
      crcs[(address - APP_START) / SECTOR_SIZE] === release.signature[index]).length >= 4);
  return matches.length === 1 ? matches[0] : null;
}

export async function extractPinnedStockFirmware(zip, release = currentStock) {
  if (release.outer) {
    const { path, bytes, sha256: digest } = release.outer;
    zip = await extractPinnedEntry(zip, bytes, digest, path, release.zipBytes);
  }
  const firmware = await extractPinnedEntry(zip, release.zipBytes, release.zipSha256, release.path, release.packageBytes);
  requireValue(await sha256(firmware) === release.packageSha256, 'Extracted stock package checksum mismatch.');
  return firmware;
}

export async function loadStockReference(release, fetchFile = fetch) {
  const response = await fetchFile(release.url);
  requireValue(response.ok, 'Could not download the stock firmware reference.');
  const image = decodePackage(await extractPinnedStockFirmware(new Uint8Array(await response.arrayBuffer()), release));
  requireValue(image.length === release.imageBytes && await sha256(image) === release.imageSha256,
    'Stock firmware reference image checksum mismatch.');
  return image;
}

async function extractPinnedEntry(zip, zipBytes, zipSha256, path, entryBytes) {
  requireValue(zip.length === zipBytes && await sha256(zip) === zipSha256,
    'Stock firmware ZIP does not match the pinned SHA-256.');
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
    if (name === path) {
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
  requireValue((entry.flags & 1) === 0 && entry.method === 8 && entry.size === entryBytes,
    'Unexpected stock firmware ZIP compression or size.');
  requireValue(entry.local + 30 <= zip.length && view.getUint32(entry.local, true) === 0x04034b50,
    'Stock firmware local ZIP header is invalid.');
  const localName = view.getUint16(entry.local + 26, true);
  const localExtra = view.getUint16(entry.local + 28, true);
  const dataStart = entry.local + 30 + localName + localExtra;
  requireValue(dataStart + entry.compressed <= offset &&
    new TextDecoder().decode(zip.subarray(entry.local + 30, entry.local + 30 + localName)) === path,
    'Stock firmware ZIP entry is out of bounds.');
  let firmware;
  try {
    const stream = new Blob([zip.subarray(dataStart, dataStart + entry.compressed)])
      .stream().pipeThrough(new DecompressionStream('deflate-raw'));
    firmware = new Uint8Array(await new Response(stream).arrayBuffer());
  } catch (error) {
    throw new Error(`Browser could not decompress the Type-C firmware ZIP: ${error.message}`);
  }
  requireValue(firmware.length === entryBytes, 'Extracted stock firmware size is invalid.');
  return firmware;
}
