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

test('a session started right after an exit tolerates the late exit acknowledgement', async () => {
  const sim = new SimulatedRadio();
  await sim.radio.enterProgramming();
  await sim.radio.send(Uint8Array.of(0x45)); // exit without waiting, as the programmer did
  await sim.radio.enterProgramming(); // the late exit 06 arrives ahead of the handshake ACK
  const status = await sim.radio.selectBank(0);
  assert.equal(status.version, 2);
  await sim.radio.leaveProgramming();
});

test('writes use the fast link and check status every 16 blocks', async () => {
  const sim = new SimulatedRadio();
  sim.chip.set(logoPattern(9), LOGO_ADDRESS);
  const current = (await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES)).bytes;
  const next = current.slice();
  next.fill(0x42, 0x3000, 0x4000); // exactly one sector changes
  sim.linkRequests = [];
  const before = sim.statusReads;
  await sim.radio.writeLogo(current, next);
  // erase check + begin check + 8 block checks, plus the read-back's bank selects
  const readBack = LOGO_BYTES / 0x8000 + 1;
  assert.ok(sim.statusReads - before <= 10 + readBack + 1, `status reads ${sim.statusReads - before}`);
  assert.equal(sim.linkRequests[0], 18);
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES), next);
});

test('a block that fails mid-sector stops the write at the next checkpoint', async () => {
  const sim = new SimulatedRadio();
  sim.chip.set(logoPattern(10), LOGO_ADDRESS);
  const current = (await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES)).bytes;
  const next = logoPattern(11);
  sim.failProgramAt = LOGO_ADDRESS + 5 * 32; // sixth block of the first sector
  sim.linkRequests = [];
  await assert.rejects(sim.radio.writeLogo(current, next), /Program 0xa00000\+0x0\.\.0x200: radio reports (not armed|program verify failed)/);
  assert.deepEqual(sim.linkRequests, [18, 17], 'fast link requested, then restored after the failure');
  // Nothing after the failed block was written, and later sectors were untouched.
  assert.ok(sim.chip.subarray(LOGO_ADDRESS + 6 * 32, LOGO_ADDRESS + 0x1000).every(b => b === 0xff));
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS + 0x1000, LOGO_ADDRESS + LOGO_BYTES), logoPattern(10).subarray(0x1000));
});
