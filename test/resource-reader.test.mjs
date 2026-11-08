import test from 'node:test';
import assert from 'node:assert/strict';
import { BLOCK, BULK_SIZES, CHIP_SIZE, LOGO_ADDRESS, LOGO_BYTES, OP, fitRect, logoFromBin, logoSectorPlan, logoToRgba, rgbaToLogo, assembleBulk, bulkSlotAddress, isBulkPacket, parseBulkReply, PARTITIONS, SECTOR, controlFrame, crc16, dataAddress, dataFrame, describeJedec, parseStatus, readPlan, statusAddress, testPattern, writable } from '../dist/boot-logo/resource.mjs';

function status(bank, overrides = {}) {
  const bytes = new Uint8Array(BLOCK);
  bytes.set([0x48, 0x39, 0x52, 0x53, 1, 0xc8, 0x40, 0x18, 1, bank & 255, bank >> 8, 0x5a]);
  for (const [index, value] of Object.entries(overrides)) bytes[index] = value;
  return bytes;
}

test('partitions tile the 16 MiB chip without gaps', () => {
  let next = 0;
  for (const part of PARTITIONS) {
    assert.equal(part.origin, next);
    next += part.length;
  }
  assert.equal(next, CHIP_SIZE);
});

test('status and data addresses stay in the service windows', () => {
  assert.equal(statusAddress(0), 0x4000);
  assert.equal(statusAddress(511), 0x7fe0);
  assert.throws(() => statusAddress(512), /0–511/);
  assert.equal(dataAddress(0), 0x8000);
  assert.equal(dataAddress(0xa07fe0), 0xffe0);
  assert.equal(dataAddress(0xa08000), 0x8000);
  assert.throws(() => dataAddress(0x10), /aligned/);
  assert.throws(() => dataAddress(CHIP_SIZE), /inside 16 MiB/);
});

test('read plan selects each bank before its first block', () => {
  const plan = [...readPlan(0x7fe0, 0x40)];
  assert.deepEqual(plan, [
    { kind: 'bank', bank: 0, address: 0x4000 },
    { kind: 'data', chip: 0x7fe0, address: 0xffe0 },
    { kind: 'bank', bank: 1, address: 0x4020 },
    { kind: 'data', chip: 0x8000, address: 0x8000 },
  ]);
  const whole = [...readPlan(0, CHIP_SIZE)];
  assert.equal(whole.filter(step => step.kind === 'bank').length, 512);
  assert.equal(whole.filter(step => step.kind === 'data').length, CHIP_SIZE / BLOCK);
  assert.throws(() => [...readPlan(0, 0)], /nonempty/);
  assert.throws(() => [...readPlan(CHIP_SIZE - 32, 64)], /inside 16 MiB/);
});

test('status parser checks magic, version, bank echo, and trailer', () => {
  assert.deepEqual(parseStatus(status(320), 320), { version: 1, ready: true, jedec: [0xc8, 0x40, 0x18], bank: 320 });
  assert.equal(parseStatus(status(2, { 8: 0 }), 2).ready, false);
  assert.throws(() => parseStatus(new Uint8Array(BLOCK), 0), /other firmware/);
  assert.throws(() => parseStatus(status(1), 2), /expected 2/);
  assert.throws(() => parseStatus(status(1, { 4: 3 }), 1), /version 3/);
  assert.throws(() => parseStatus(status(1, { 11: 0 }), 1), /trailer/);
});

test('JEDEC description recognizes a 16 MiB part', () => {
  assert.equal(describeJedec([0xef, 0x40, 0x18]).matches16MiB, true);
  assert.match(describeJedec([0xef, 0x40, 0x18]).text, /Winbond, 16 MiB/);
  assert.equal(describeJedec([0x85, 0x60, 0x14]).matches16MiB, false);
  assert.match(describeJedec([0, 0, 0]).text, /unknown maker/);
});

test('version 2 status exposes write-service state', () => {
  const bytes = status(5, { 4: 2, 12: 10, 13: 2, 14: 0x20, 15: 0x00, 16: 0xa0, 17: 3, 18: 0x3e, 19: 0xe0 });
  assert.deepEqual(parseStatus(bytes, 5).write, {
    result: 10, resultText: 'target not erased', programming: true, pointer: 0xa00020, operations: 3, crc: 0xe03e,
  });
  assert.equal(parseStatus(status(5), 5).write, undefined);
});

test('CRC-16/XMODEM matches the firmware erased-sector constant', () => {
  assert.equal(crc16(new TextEncoder().encode('123456789')), 0x31c3);
  assert.equal(crc16(new Uint8Array(SECTOR).fill(0xff)), 0xe03e);
});

test('control frames carry magic, op, address, CRC, and the W checksum', () => {
  const frame = controlFrame(OP.erase, 0xa3f000, 0xbeef);
  assert.equal(frame.length, 37);
  assert.deepEqual([...frame.slice(0, 4)], [0x57, 0x40, 0x00, 0x20]);
  assert.deepEqual([...frame.slice(4, 14)], [0x48, 0x39, 0x57, 0x52, 1, 0x00, 0xf0, 0xa3, 0xef, 0xbe]);
  assert.equal(frame[36], frame.slice(4, 36).reduce((sum, byte) => (sum + byte) & 255, 0));
  assert.deepEqual([...controlFrame(OP.disarm).slice(4, 9)], [0x48, 0x39, 0x57, 0x52, 0]);
});

test('control frames refuse live-data partitions and misalignment', () => {
  for (const name of ['filesystem', 'sys_data', 'rw_freq', 'record']) {
    const part = PARTITIONS.find(entry => entry.name === name);
    assert.equal(writable(part.origin), false);
    assert.throws(() => controlFrame(OP.erase, part.origin, 0), /writable/);
  }
  for (const name of ['voice', 'font', 'resource']) assert.equal(writable(PARTITIONS.find(entry => entry.name === name).origin), true);
  assert.throws(() => controlFrame(OP.erase, 0xa00020, 0), /4 KiB/);
  assert.throws(() => controlFrame(OP.program, 0xa00010, 0), /32-byte/);
  assert.throws(() => controlFrame(OP.erase, CHIP_SIZE, 0), /16 MiB/);
});

test('data frames target 0x4020 and the test pattern is never erased-looking', () => {
  const pattern = testPattern(0xa3f000);
  assert.equal(pattern.length, SECTOR);
  for (let offset = 0; offset < SECTOR; offset += BLOCK) {
    assert.ok(pattern.subarray(offset, offset + BLOCK).some(byte => byte !== 0xff));
  }
  assert.notEqual(crc16(pattern), 0xe03e);
  assert.deepEqual([...dataFrame(pattern.subarray(0, BLOCK)).slice(0, 4)], [0x57, 0x40, 0x20, 0x20]);
});

function bulkReply({ status = 0, start = 0, count = 0, capacity = 0, recoveries = 0, cursor = start + count, magic = 'H9BK', tag = 0xb7 }) {
  const bytes = new Uint8Array(BLOCK);
  bytes.set(new TextEncoder().encode(magic));
  const view = new DataView(bytes.buffer);
  view.setUint32(4, ((status << 24) | start) >>> 0, true);
  view.setUint32(8, ((recoveries << 22) | (capacity << 12) | count) >>> 0, true);
  view.setUint32(12, ((tag << 24) | (cursor & 0xffffff)) >>> 0, true);
  return bytes;
}

function packet(seq, data) {
  return Uint8Array.of(0xb7, 0x5a, seq, data.length, ...data);
}

test('bulk slots and packet recognition', () => {
  assert.equal(bulkSlotAddress(0), 0x3c00);
  assert.equal(bulkSlotAddress(4), 0x3c80);
  assert.equal(bulkSlotAddress(3, true), 0x3d60);
  assert.throws(() => bulkSlotAddress(5), /size index/);
  assert.equal(isBulkPacket(packet(0, [1])), true);
  assert.equal(isBulkPacket(Uint8Array.of(0x57, 0x3c, 0x00, 0x20)), false);
  assert.deepEqual(BULK_SIZES, [16, 48, 112, 176, 240]);
});

test('bulk reply parsing', () => {
  const reply = parseBulkReply(bulkReply({ status: 0, start: 0xa00000, count: 2048, capacity: 494, recoveries: 9 }));
  assert.deepEqual(reply, { status: 0, statusText: 'ok', start: 0xa00000, count: 2048, capacity: 494, waitRecoveries: 9, cursor: 0xa00800, cursorValid: true });
  assert.equal(parseBulkReply(new Uint8Array(BLOCK)), null);
  assert.equal(parseBulkReply(bulkReply({ status: 6 })).statusText, 'notification submit failed');
});

test('bulk assembly checks sequence, length byte, size, and total', () => {
  const size = 16;
  const a = Array.from({ length: size }, (_, i) => i);
  const b = Array.from({ length: 8 }, (_, i) => 100 + i);
  const data = assembleBulk([packet(0, a), packet(1, b)], { count: 24 }, size);
  assert.deepEqual([...data], [...a, ...b]);
  assert.throws(() => assembleBulk([packet(1, a)], { count: 16 }, size), /sequence/);
  const short = packet(0, a).slice(0, 12);
  assert.throws(() => assembleBulk([short], { count: 16 }, size), /expected 20/);
  assert.throws(() => assembleBulk([packet(0, Array(24).fill(0))], { count: 24 }, size), /limit 16/);
  assert.throws(() => assembleBulk([packet(0, a.slice(0, 5))], { count: 5 }, size), /carries 5/);
  assert.throws(() => assembleBulk([packet(0, a)], { count: 32 }, size), /radio sent 32/);
});

test('logo conversion is big-endian RGB565 and round-trips primaries', () => {
  const rgba = new Uint8ClampedArray(160 * 128 * 4);
  const colors = [[255, 0, 0], [0, 255, 0], [0, 0, 255], [255, 255, 255], [0, 0, 0]];
  colors.forEach(([r, g, b], pixel) => rgba.set([r, g, b, 255], pixel * 4));
  const logo = rgbaToLogo(rgba);
  assert.equal(logo.length, LOGO_BYTES);
  assert.deepEqual([...logo.slice(0, 10)], [0xf8, 0x00, 0x07, 0xe0, 0x00, 0x1f, 0xff, 0xff, 0x00, 0x00]);
  const back = logoToRgba(logo);
  colors.forEach(([r, g, b], pixel) => assert.deepEqual([...back.slice(pixel * 4, pixel * 4 + 3)], [r, g, b]));
  assert.throws(() => rgbaToLogo(new Uint8ClampedArray(10)), /160 x 128/);
});

test('logo .bin accepts the logo or a dump starting at 0xA00000', () => {
  const dump = new Uint8Array(3 * 1024 * 1024).fill(7);
  assert.equal(logoFromBin(dump).length, LOGO_BYTES);
  assert.equal(logoFromBin(new Uint8Array(LOGO_BYTES)).length, LOGO_BYTES);
  assert.throws(() => logoFromBin(new Uint8Array(1000)), /40,960/);
});

test('logo plan lists only changed sectors with their current CRC', () => {
  const current = new Uint8Array(LOGO_BYTES);
  const next = current.slice();
  next[5000] = 1;           // sector 1
  next.fill(0xff, 36864);   // sector 9 becomes erased
  const plan = logoSectorPlan(current, next);
  assert.deepEqual(plan.map(entry => entry.address), [LOGO_ADDRESS + 0x1000, LOGO_ADDRESS + 0x9000]);
  assert.equal(plan[0].oldCrc, plan[1].oldCrc);
  assert.deepEqual(plan.map(entry => entry.erasedTarget), [false, true]);
  assert.deepEqual(logoSectorPlan(current, current), []);
});

test('image fit modes place the picture on the 160 x 128 screen', () => {
  assert.deepEqual(fitRect(320, 128, 'contain'), { x: 0, y: 32, width: 160, height: 64 });
  assert.deepEqual(fitRect(320, 128, 'cover'), { x: -80, y: 0, width: 320, height: 128 });
  assert.deepEqual(fitRect(4032, 3024, 'stretch'), { x: 0, y: 0, width: 160, height: 128 });
  const square = fitRect(1000, 1000, 'contain');
  assert.equal(square.width, 128);
  assert.equal(square.x, 16);
  assert.throws(() => fitRect(0, 10), /no pixels/);
  assert.throws(() => fitRect(10, 10, 'zoom'), /Unknown fit/);
});

test('logo conversion rounds to the nearest radio colour and dithers gradients', () => {
  const rgba = new Uint8ClampedArray(160 * 128 * 4).fill(255);
  rgba.set([5, 3, 5, 255], 0); // just above half a level (5*31/255 = 0.61): rounds up; truncation gave 0
  const exact = rgbaToLogo(rgba);
  assert.equal((exact[0] << 8) | exact[1], (1 << 11) | (1 << 5) | 1);
  // A flat colour between two levels: plain rounding gives one value
  // everywhere; dithering mixes neighbours so the average stays close.
  const grey = new Uint8ClampedArray(160 * 128 * 4);
  for (let i = 0; i < grey.length; i += 4) grey.set([100, 100, 100, 255], i);
  const red = bytes => { const values = []; for (let i = 0; i < bytes.length; i += 2) values.push((bytes[i] >> 3) * 255 / 31); return values; };
  const plain = red(rgbaToLogo(grey));
  const dithered = red(rgbaToLogo(grey, { dither: true }));
  assert.equal(new Set(plain).size, 1);
  assert.ok(new Set(dithered).size > 1);
  const mean = values => values.reduce((a, b) => a + b, 0) / values.length;
  assert.ok(Math.abs(mean(dithered) - 100) < Math.abs(mean(plain) - 100));
  assert.ok(Math.abs(mean(dithered) - 100) < 0.5);
});
