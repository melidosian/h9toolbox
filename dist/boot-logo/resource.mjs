// TD-H9 resource-flash service (S1 read, S2 write) and boot-logo format.
// Pure functions only: shared by the boot-logo page, the research reader in
// prototypes/resource-reader, and the Node tests. See
// firmware-research/resource-service/README.md for the radio side.

export const CHIP_SIZE = 0x1000000;
export const BANK_SIZE = 0x8000;
export const BLOCK = 0x20;
export const STATUS_BASE = 0x4000;
export const DATA_BASE = 0x8000;

// Stock v1.0.38 partition table at 0x01EB53F0.
export const PARTITIONS = [
  { name: 'filesystem', origin: 0x000000, length: 0x100000 },
  { name: 'voice', origin: 0x100000, length: 0x300000 },
  { name: 'font', origin: 0x400000, length: 0x400000 },
  { name: 'sys_data', origin: 0x800000, length: 0x100000 },
  { name: 'rw_freq', origin: 0x900000, length: 0x100000 },
  { name: 'resource', origin: 0xa00000, length: 0x300000 },
  { name: 'record', origin: 0xd00000, length: 0x300000 },
];

const MANUFACTURERS = { 0x0b: 'XTX', 0x20: 'XMC', 0x5e: 'Zbit', 0x68: 'Boya', 0x85: 'Puya', 0x9d: 'ISSI', 0xa1: 'Fudan', 0xc2: 'Macronix', 0xc8: 'GigaDevice', 0xef: 'Winbond' };

export function hex(bytes) {
  return [...bytes].map(byte => byte.toString(16).padStart(2, '0')).join(' ');
}

export function statusAddress(bank) {
  if (!Number.isInteger(bank) || bank < 0 || bank >= CHIP_SIZE / BANK_SIZE) throw new Error('Bank must be 0–511.');
  return STATUS_BASE + bank * BLOCK;
}

export function dataAddress(chipAddress) {
  if (!Number.isInteger(chipAddress) || chipAddress < 0 || chipAddress >= CHIP_SIZE || chipAddress % BLOCK) {
    throw new Error('Chip address must be 32-byte aligned inside 16 MiB.');
  }
  return DATA_BASE + (chipAddress % BANK_SIZE);
}

export function checkRange(start, length) {
  if (!Number.isInteger(start) || !Number.isInteger(length) || start < 0 || length <= 0 ||
      start % BLOCK || length % BLOCK || start + length > CHIP_SIZE) {
    throw new Error('Range must be 32-byte aligned, nonempty, and inside 16 MiB.');
  }
}

// One entry per 32-byte block; a bank selection precedes the first block of each bank.
export function* readPlan(start, length) {
  checkRange(start, length);
  let bank = -1;
  for (let chip = start; chip < start + length; chip += BLOCK) {
    const blockBank = Math.floor(chip / BANK_SIZE);
    if (blockBank !== bank) {
      bank = blockBank;
      yield { kind: 'bank', bank, address: statusAddress(bank) };
    }
    yield { kind: 'data', chip, address: dataAddress(chip) };
  }
}

export function parseStatus(bytes, bank) {
  if (bytes.length !== BLOCK) throw new Error('Status block must be 32 bytes.');
  if (hex(bytes.slice(0, 4)) !== '48 39 52 53') {
    throw new Error(`No resource service at status slot (got ${hex(bytes.slice(0, 4))}); the radio is running other firmware.`);
  }
  if (![1, 2].includes(bytes[4])) throw new Error(`Unsupported service version ${bytes[4]}.`);
  if (bytes[11] !== 0x5a) throw new Error('Status block trailer is wrong.');
  const echoed = bytes[9] | (bytes[10] << 8);
  if (echoed !== bank) throw new Error(`Radio selected bank ${echoed}, expected ${bank}.`);
  const jedec = [...bytes.slice(5, 8)];
  const status = { version: bytes[4], ready: bytes[8] === 1, jedec, bank: echoed };
  if (bytes[4] === 2) {
    status.write = {
      result: bytes[12],
      resultText: RESULTS[bytes[12]] || `unknown result ${bytes[12]}`,
      programming: bytes[13] === 2,
      pointer: bytes[14] | (bytes[15] << 8) | (bytes[16] << 16),
      operations: bytes[17],
      crc: bytes[18] | (bytes[19] << 8),
    };
  }
  return status;
}

// S2 write service (W 0x4000-0x4FFF). Result codes match resource_write.s.
export const SECTOR = 0x1000;
export const CONTROL_ADDRESS = 0x4000;
export const DATA_ADDRESS = 0x4020;
export const OP = { disarm: 0, erase: 1, program: 2 };
export const RESULTS = {
  0: 'ok', 1: 'bad magic', 2: 'flash not ready', 3: 'partition refused', 4: 'misaligned',
  5: 'sector CRC mismatch', 6: 'erase verify failed', 7: 'not armed', 8: 'bad W checksum',
  9: 'out of range', 10: 'target not erased', 11: 'program verify failed', 12: 'bad op',
  0xfe: 'no command since boot',
};
const WRITABLE = new Set(['voice', 'font', 'resource']);

export function partitionAt(chipAddress) {
  return PARTITIONS.find(part => chipAddress >= part.origin && chipAddress < part.origin + part.length);
}

export function writable(chipAddress) {
  return WRITABLE.has(partitionAt(chipAddress)?.name);
}

export function crc16(bytes) {
  let crc = 0;
  for (const byte of bytes) {
    crc ^= byte << 8;
    for (let bit = 0; bit < 8; bit++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
  }
  return crc;
}

export function writeFrame(address, payload) {
  if (payload.length !== BLOCK) throw new Error('W payload must be 32 bytes.');
  const checksum = payload.reduce((sum, byte) => (sum + byte) & 255, 0);
  return Uint8Array.of(0x57, address >> 8, address & 255, BLOCK, ...payload, checksum);
}

export function controlFrame(op, chipAddress = 0, crc = 0) {
  if (!Object.values(OP).includes(op)) throw new Error('Unknown write-service op.');
  if (op !== OP.disarm) {
    if (!Number.isInteger(chipAddress) || chipAddress < 0 || chipAddress >= CHIP_SIZE) throw new Error('Chip address outside 16 MiB.');
    if (!writable(chipAddress)) throw new Error('Only voice, font, and resource are writable.');
    if (op === OP.erase && chipAddress % SECTOR) throw new Error('Erase needs a 4 KiB aligned sector.');
    if (op === OP.program && chipAddress % BLOCK) throw new Error('Programming starts on a 32-byte boundary.');
  }
  const payload = new Uint8Array(BLOCK);
  payload.set([0x48, 0x39, 0x57, 0x52, op, chipAddress & 255, (chipAddress >> 8) & 255, (chipAddress >> 16) & 255, crc & 255, crc >> 8]);
  return writeFrame(CONTROL_ADDRESS, payload);
}

export function dataFrame(bytes) {
  return writeFrame(DATA_ADDRESS, bytes);
}

// Deterministic, non-erased test pattern for one sector.
export function testPattern(sectorAddress) {
  const seed = (sectorAddress >> 12) & 255;
  return Uint8Array.from({ length: SECTOR }, (_, index) => (index * 37 + seed + (index >> 8)) & 255 ^ 0x5a);
}

// Bulk read (S1/S2 bulk slot at R 0x3C00 + 32*k): the radio streams up to
// 2 KiB from its cursor as notifications tagged B7 5A seq len, then sends the
// normal 32-byte reply describing what it sent. Slots k 0-4 pick the maximum
// packet size; k 8-12 also let the radio wait (bounded) for TX room.
export const BULK_BASE = 0x3c00;
export const BULK_SIZES = [16, 48, 112, 176, 240];
export const BULK_STATUS = {
  0: 'ok', 2: 'flash not ready or read failed', 3: 'TX queue full', 4: 'bad slot',
  5: 'no bank selected', 6: 'notification submit failed', 7: 'not connected',
  8: 'link request returned an error',
};

export const BULK_HEADER = 4;
// Slot 16 asks the central for a 15-30 ms, latency-0 link; slot 17 restores
// stock's parameters (20-30 ms with slave latency 10).
export const LINK_FAST_ADDRESS = BULK_BASE + 16 * BLOCK;
export const LINK_STOCK_ADDRESS = BULK_BASE + 17 * BLOCK;
// Slot 18 asks for a fixed 15 ms, latency-0 link (S1 v5 and later).
export const LINK_FIXED_ADDRESS = BULK_BASE + 18 * BLOCK;

export function bulkSlotAddress(sizeIndex, wait = false) {
  if (!Number.isInteger(sizeIndex) || sizeIndex < 0 || sizeIndex >= BULK_SIZES.length) throw new Error('Bad bulk size index.');
  return BULK_BASE + (sizeIndex + (wait ? 8 : 0)) * BLOCK;
}

export function isBulkPacket(bytes) {
  return bytes.length >= 4 && bytes[0] === 0xb7 && bytes[1] === 0x5a;
}

export function parseBulkReply(bytes) {
  if (bytes.length !== BLOCK) throw new Error('Bulk reply must be 32 bytes.');
  if (hex(bytes.slice(0, 4)) !== '48 39 42 4b') return null; // not a bulk-capable image
  const word = offset => bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24) >>> 0;
  const result = word(4) >>> 0;
  const counts = word(8) >>> 0;
  const cursor = word(12) >>> 0;
  return {
    status: result >>> 24,
    statusText: BULK_STATUS[result >>> 24] || `unknown ${result >>> 24}`,
    start: result & 0xffffff,
    count: counts & 0xfff,
    capacity: (counts >>> 12) & 0x3ff,
    waitRecoveries: counts >>> 22,
    cursor: cursor & 0xffffff,
    cursorValid: cursor >>> 24 === 0xb7,
  };
}

// Join packets received before the reply; every byte must be accounted for.
// Each packet is B7 5A seq len + len data bytes, len a multiple of 8 up to size.
export function assembleBulk(packets, reply, size) {
  let length = 0;
  packets.forEach((packet, index) => {
    if (packet[2] !== (index & 255)) throw new Error(`Bulk packet ${index} has sequence ${packet[2]}.`);
    const dataLength = packet[3];
    if (packet.length !== dataLength + BULK_HEADER) throw new Error(`Bulk packet ${index} is ${packet.length} bytes, expected ${dataLength + BULK_HEADER}.`);
    if (!dataLength || dataLength % 8 || dataLength > size) throw new Error(`Bulk packet ${index} carries ${dataLength} bytes (limit ${size}).`);
    length += dataLength;
  });
  if (length !== reply.count) throw new Error(`Received ${length} bulk bytes, radio sent ${reply.count}.`);
  const data = new Uint8Array(length);
  let offset = 0;
  for (const packet of packets) { data.set(packet.subarray(BULK_HEADER), offset); offset += packet[3]; }
  return data;
}

// Boot logo: raw 160 x 128 RGB565, big-endian, no header, at 0xA00000 (10
// sectors, nothing else in them). Found in the resource dump on 2026-11-08.
export const LOGO_ADDRESS = 0xa00000;
export const LOGO_WIDTH = 160;
export const LOGO_HEIGHT = 128;
export const LOGO_BYTES = LOGO_WIDTH * LOGO_HEIGHT * 2;
export const STOCK_LOGO_SHA256 = '54d2077d9d17900e898b3326f744f5678b2ae37e0fb305f5705505665bf04574';

// Where an image of srcWidth x srcHeight lands on the 160 x 128 screen.
// contain: whole image, centred, bars around it; cover: fills the screen,
// centred crop; stretch: fills the screen, aspect ratio not kept.
export function fitRect(srcWidth, srcHeight, fit = 'contain') {
  if (!(srcWidth > 0 && srcHeight > 0)) throw new Error('Image has no pixels.');
  if (fit === 'stretch') return { x: 0, y: 0, width: LOGO_WIDTH, height: LOGO_HEIGHT };
  if (fit !== 'contain' && fit !== 'cover') throw new Error(`Unknown fit "${fit}".`);
  const scale = (fit === 'cover' ? Math.max : Math.min)(LOGO_WIDTH / srcWidth, LOGO_HEIGHT / srcHeight);
  const width = srcWidth * scale;
  const height = srcHeight * scale;
  return { x: (LOGO_WIDTH - width) / 2, y: (LOGO_HEIGHT - height) / 2, width, height };
}

// 160 x 128 RGBA (already composited onto an opaque background) to the
// radio's RGB565 big-endian bytes. Each channel rounds to the nearest of the
// 32/64/32 levels; with dither, Floyd-Steinberg error diffusion spreads the
// rounding error so photo gradients do not band.
export function rgbaToLogo(rgba, { dither = false } = {}) {
  const pixels = LOGO_WIDTH * LOGO_HEIGHT;
  if (rgba.length !== pixels * 4) throw new Error('Image must be 160 x 128 RGBA.');
  const levels = [31, 63, 31];
  const work = new Float32Array(pixels * 3);
  for (let pixel = 0; pixel < pixels; pixel++) {
    for (let channel = 0; channel < 3; channel++) work[pixel * 3 + channel] = rgba[pixel * 4 + channel];
  }
  const out = new Uint8Array(LOGO_BYTES);
  const spread = (x, y, channel, amount) => {
    if (x < 0 || x >= LOGO_WIDTH || y >= LOGO_HEIGHT) return;
    work[(y * LOGO_WIDTH + x) * 3 + channel] += amount;
  };
  for (let y = 0; y < LOGO_HEIGHT; y++) {
    for (let x = 0; x < LOGO_WIDTH; x++) {
      const pixel = y * LOGO_WIDTH + x;
      const q = [0, 0, 0];
      for (let channel = 0; channel < 3; channel++) {
        const value = Math.min(255, Math.max(0, work[pixel * 3 + channel]));
        q[channel] = Math.round(value * levels[channel] / 255);
        if (dither) {
          const error = value - q[channel] * 255 / levels[channel];
          spread(x + 1, y, channel, error * 7 / 16);
          spread(x - 1, y + 1, channel, error * 3 / 16);
          spread(x, y + 1, channel, error * 5 / 16);
          spread(x + 1, y + 1, channel, error / 16);
        }
      }
      const value = (q[0] << 11) | (q[1] << 5) | q[2];
      out[pixel * 2] = value >> 8;
      out[pixel * 2 + 1] = value & 255;
    }
  }
  return out;
}

export function logoToRgba(bytes) {
  if (bytes.length !== LOGO_BYTES) throw new Error(`Logo must be ${LOGO_BYTES} bytes.`);
  const rgba = new Uint8ClampedArray(LOGO_WIDTH * LOGO_HEIGHT * 4);
  for (let pixel = 0; pixel < LOGO_WIDTH * LOGO_HEIGHT; pixel++) {
    const value = (bytes[pixel * 2] << 8) | bytes[pixel * 2 + 1];
    rgba[pixel * 4] = ((value >> 11) & 31) * 255 / 31;
    rgba[pixel * 4 + 1] = ((value >> 5) & 63) * 255 / 63;
    rgba[pixel * 4 + 2] = (value & 31) * 255 / 31;
    rgba[pixel * 4 + 3] = 255;
  }
  return rgba;
}

// A .bin chosen for the logo: either the 40,960-byte logo itself or a dump
// that starts at 0xA00000 (such as the 3 MiB resource backup).
export function logoFromBin(bytes) {
  if (bytes.length === LOGO_BYTES) return bytes.slice();
  if (bytes.length > LOGO_BYTES && bytes.length % SECTOR === 0) return bytes.slice(0, LOGO_BYTES);
  throw new Error('Use a 40,960-byte logo or a dump that starts at 0xA00000.');
}

// Sectors that must be rewritten to turn `current` into `next`.
export function logoSectorPlan(current, next) {
  if (current.length !== LOGO_BYTES || next.length !== LOGO_BYTES) throw new Error('Both logos must be 40,960 bytes.');
  const plan = [];
  for (let offset = 0; offset < LOGO_BYTES; offset += SECTOR) {
    const old = current.subarray(offset, offset + SECTOR);
    const fresh = next.subarray(offset, offset + SECTOR);
    if (old.some((byte, index) => byte !== fresh[index])) {
      plan.push({ address: LOGO_ADDRESS + offset, offset, oldCrc: crc16(old), erasedTarget: fresh.every(byte => byte === 0xff) });
    }
  }
  return plan;
}

export function describeJedec([manufacturer, type, capacity]) {
  const name = MANUFACTURERS[manufacturer] || 'unknown maker';
  const size = capacity >= 0x10 && capacity <= 0x20 ? 2 ** capacity : null;
  const text = `${hex([manufacturer, type, capacity])} (${name}${size ? `, ${size / 0x100000} MiB` : ''})`;
  return { name, size, text, matches16MiB: size === CHIP_SIZE };
}
