// Stock v1.0.38 heap control block (see main-ram-map.md): 128-byte blocks,
// a bitmap at 0x1D7D0 (bit set = free), bounds at +200/+204, the used-block
// counter at +216 and the total at +218.
export const HEAP_CONTROL = 0x1d7d0;
export const HEAP_READ_START = 0x18000;
export const HEAP_READ_LENGTH = 0x58c0; // through 0x1D8BF
export const HEAP_RANGES = [[0x1e500, 0x2bf00], [0x2d200, 0x2ff80]]; // stock floors 0x2D27F >> 7
const BLOCK = 128;

// bytes: RAM from HEAP_READ_START. Returns counters, the free map, and checks.
export function parseHeap(bytes) {
  if (bytes.length < HEAP_READ_LENGTH) throw new Error('Heap read is too short.');
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const at = address => address - HEAP_READ_START;
  const word = offset => view.getUint32(at(HEAP_CONTROL + offset), true);
  const half = offset => view.getUint16(at(HEAP_CONTROL + offset), true);
  const used = half(216), total = half(218);
  const blocks = [];
  let freeBits = 0;
  for (const [from, to] of HEAP_RANGES) {
    for (let block = from / BLOCK; block < to / BLOCK; block++) {
      const free = Boolean(word((block >> 5) * 4) & (1 << (block & 31)));
      blocks.push({ address: block * BLOCK, free });
      if (free) freeBits++;
    }
  }
  const checks = {
    bounds: word(200) === 0x1e500 && word(204) === 0x2ff80,
    total: total === 527,
    bitmapAgrees: total - used === freeBits,
  };
  return { used, total, freeBlocks: total - used, freeBytes: (total - used) * BLOCK, freeBits, blocks, checks };
}
