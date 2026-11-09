import test from 'node:test';
import assert from 'node:assert/strict';
import { HEAP_CONTROL, HEAP_READ_LENGTH, HEAP_READ_START, parseHeap } from '../prototypes/ram-probe/heap.mjs';

// The heap control block as stock app+0x05FB32 leaves it (same steps as
// replacement-source/main/h9_main_heap_bitmap.c).
function stockHeap() {
  const ram = new Uint8Array(HEAP_READ_LENGTH);
  const view = new DataView(ram.buffer);
  const word = index => HEAP_CONTROL - HEAP_READ_START + index * 4;
  const mark = (first, last) => {
    for (let block = first; block < last; block++) {
      view.setUint32(word(block >> 5), view.getUint32(word(block >> 5), true) | (1 << (block & 31)), true);
      view.setUint32(word(48 + (block >> 10)), view.getUint32(word(48 + (block >> 10)), true) | (1 << ((block >> 5) & 31)), true);
    }
  };
  mark((0x1e4d8 + 127) >> 7, 0x2bf00 >> 7);
  mark(0x2d27f >> 7, 0x2ff80 >> 7);
  view.setUint32(word(50), 0x1e500, true);
  view.setUint32(word(51), 0x2ff80, true);
  view.setUint32(word(52), 0x10780, true);
  view.setUint32(word(53), (0x5ff << 16) | 0x3ca, true);
  view.setUint32(word(54), 0x20f << 16, true);
  return { ram, view, word };
}

test('a freshly set-up stock heap reads as 0 of 527 blocks used, all checks passing', () => {
  const heap = parseHeap(stockHeap().ram);
  assert.equal(heap.used, 0);
  assert.equal(heap.total, 527);
  assert.equal(heap.freeBytes, 527 * 128);
  assert.deepEqual(heap.checks, { bounds: true, total: true, bitmapAgrees: true });
  assert.equal(heap.blocks.length, 527);
});

test('allocations show as used blocks and the counter must agree with the bitmap', () => {
  const { ram, view, word } = stockHeap();
  for (const block of [0x3ca, 0x3cb, 0x5a4]) view.setUint32(word(block >> 5), view.getUint32(word(block >> 5), true) & ~(1 << (block & 31)), true);
  view.setUint16(HEAP_CONTROL - HEAP_READ_START + 216, 3, true);
  const heap = parseHeap(ram);
  assert.equal(heap.used, 3);
  assert.equal(heap.freeBytes, 524 * 128);
  assert.equal(heap.checks.bitmapAgrees, true);
  assert.deepEqual(heap.blocks.filter(b => !b.free).map(b => b.address), [0x1e500, 0x1e580, 0x2d200]);
  view.setUint16(HEAP_CONTROL - HEAP_READ_START + 216, 9, true);
  assert.equal(parseHeap(ram).checks.bitmapAgrees, false);
});
