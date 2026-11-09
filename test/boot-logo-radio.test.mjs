import test from 'node:test';
import assert from 'node:assert/strict';
import { BULK_SIZES, LOGO_ADDRESS, LOGO_BYTES, SECTOR } from '../dist/boot-logo/resource.mjs';
import { Stopped } from '../dist/boot-logo/radio.mjs';
import { SimulatedRadio } from './support/simulated-radio.mjs';

const logoPattern = seed => Uint8Array.from({ length: LOGO_BYTES }, (_, i) => (i * 13 + seed + (i >> 7)) & 255);

test('checks the service: version 2, flash ready, 16 MiB JEDEC', async () => {
  const sim = new SimulatedRadio();
  const status = await sim.radio.checkService();
  assert.equal(status.version, 2);
  assert.equal(status.ready, true);
  assert.deepEqual(status.jedec, [0xc8, 0x40, 0x18]);
});

test('fast read returns the logo exactly, requesting and restoring the link', async () => {
  const sim = new SimulatedRadio();
  const logo = logoPattern(1);
  sim.chip.set(logo, LOGO_ADDRESS);
  const fractions = [];
  const result = await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES, fraction => fractions.push(fraction));
  assert.equal(result.method, 'fast');
  assert.deepEqual(result.bytes, logo);
  assert.deepEqual(sim.linkRequests, [18, 17]);
  assert.equal(fractions.at(-1), 1);
});

test('fast read reassembles packets split by a small link MTU and shrinks packets', async () => {
  const sim = new SimulatedRadio({ mtu: 100 });
  const logo = logoPattern(2);
  sim.chip.set(logo, LOGO_ADDRESS);
  const result = await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES);
  assert.deepEqual(result.bytes, logo);
  assert.ok(BULK_SIZES[sim.radio.bulkSizeIndex] + 4 <= 100);
});

test('firmware without bulk reads falls back to 32-byte reads', async () => {
  const sim = new SimulatedRadio({ bulk: false });
  const logo = logoPattern(3);
  sim.chip.set(logo, LOGO_ADDRESS);
  const result = await sim.radio.readRange(LOGO_ADDRESS, 0x2000);
  assert.equal(result.method, '32-byte');
  assert.deepEqual(result.bytes, logo.subarray(0, 0x2000));
});

test('writes only differing sectors, verifies, and leaves other data alone', async () => {
  const sim = new SimulatedRadio();
  const old = logoPattern(4);
  sim.chip.set(old, LOGO_ADDRESS);
  sim.chip.fill(0x11, LOGO_ADDRESS + LOGO_BYTES, LOGO_ADDRESS + LOGO_BYTES + SECTOR); // next resource
  const next = old.slice();
  next.fill(0x22, 0x1000, 0x1800);   // sector 1 changes
  next.fill(0xff, 0x9000, 0xa000);   // sector 9 becomes erased (erase only)
  const current = (await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES)).bytes;
  const phases = [];
  const result = await sim.radio.writeLogo(current, next, (fraction, text) => phases.push([fraction, text]));
  assert.equal(result.sectors, 2);
  assert.equal(sim.erases, 2);
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES), next);
  assert.ok(sim.chip.subarray(LOGO_ADDRESS + LOGO_BYTES, LOGO_ADDRESS + LOGO_BYTES + SECTOR).every(b => b === 0x11));
  assert.deepEqual(current, next);
  assert.ok(phases.every(([a], i) => i === 0 || a >= phases[i - 1][0]), 'progress never goes backwards');
  assert.ok(phases.at(-1)[0] > 0.999);
  assert.equal((await sim.radio.writeLogo(current, next)).sectors, 0);
});

test('refuses to erase when the radio no longer holds what was read', async () => {
  const sim = new SimulatedRadio();
  sim.chip.set(logoPattern(5), LOGO_ADDRESS);
  const current = (await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES)).bytes;
  sim.chip[LOGO_ADDRESS + 10] ^= 1; // changed behind our back
  const before = sim.chip.slice(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES);
  await assert.rejects(sim.radio.writeLogo(current, logoPattern(6)), /sector CRC mismatch/);
  assert.equal(sim.erases, 0);
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES), before);
});

test('a stopped write resumes from the sectors already done', async () => {
  const sim = new SimulatedRadio();
  sim.chip.set(logoPattern(7), LOGO_ADDRESS);
  const current = (await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES)).bytes;
  const next = logoPattern(8);
  let sectorsSeen = 0;
  await assert.rejects(sim.radio.writeLogo(current, next, (_, text) => {
    if (text.endsWith('erasing') && ++sectorsSeen === 4) sim.radio.stopRequested = true;
  }), Stopped);
  sim.radio.stopRequested = false;
  // Stop takes effect between sectors: the sector in progress (the 4th)
  // finishes, so four are written and six remain.
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + 4 * SECTOR), next.subarray(0, 4 * SECTOR));
  assert.deepEqual(current.subarray(0, 4 * SECTOR), next.subarray(0, 4 * SECTOR));
  const result = await sim.radio.writeLogo(current, next);
  assert.equal(result.sectors, 6);
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES), next);
});

test('research RAM read streams RAM, re-selects RAM after a retry, and is refused by normal builds', async () => {
  const sim = new SimulatedRadio({ ramread: true });
  for (let i = 0; i < sim.ram.length; i++) sim.ram[i] = (i * 5 + (i >> 9)) & 255;
  sim.chip.fill(0x77, 0x18000, 0x20000); // chip bytes at the same addresses must not leak in
  const result = await sim.radio.readRam(0x18000, 0x8000);
  assert.deepEqual(result.bytes, sim.ram.subarray(0x18000, 0x20000));
  sim.dropBulk = 3; // the third request times out; the retry must return to RAM mode
  const whole = await sim.radio.readRam(0, 0x30000);
  assert.deepEqual(whole.bytes, sim.ram);
  assert.equal(sim.dropBulk, 0);
  // A normal build answers slot 19 with "bad slot": RAM read is refused.
  const normal = new SimulatedRadio();
  await assert.rejects(normal.radio.readRam(0, 0x8000), /no RAM read/);
  await assert.rejects(sim.radio.readRam(0x2c000, 0x8000), /32 KiB-aligned/);
});
