import assert from 'node:assert/strict';
import test from 'node:test';
import {
  AREAS, DIRECTORY, buildDirectory, parseDirectory, planLayout, restore, sectorPlan, uploadPack,
} from '../prototypes/data-upload/upload.mjs';
import { SECTOR } from '../dist/boot-logo/resource.mjs';
import { SimulatedRadio } from './support/simulated-radio.mjs';

const pack = (length, seed = 1) => Uint8Array.from({ length }, (_, i) => (i * 131 + seed + (i >> 9)) & 255);

// A chip like the user's: stock data right up to each tail, tails erased.
function stockLikeChip(sim) {
  for (const area of AREAS) sim.chip.fill(0x42, area.start - SECTOR, area.start);
  sim.chip.fill(0x24, 0xa00000, 0xa1f000); // stock images and the boot logo
}

test('layout fills resource after the directory, then font, then voice', () => {
  assert.deepEqual(planLayout(10), [{ area: 'resource', address: DIRECTORY + SECTOR, length: 10 }]);
  const resourceRoom = 0xd00000 - (DIRECTORY + SECTOR);
  const two = planLayout(resourceRoom + 5);
  assert.deepEqual(two.map(s => [s.area, s.address, s.length]), [['resource', 0xa20000, resourceRoom], ['font', 0x717000, 5]]);
  assert.throws(() => planLayout(5 << 20), /larger than the stock-safe space/);
});

test('directory round-trips and rejects corruption', async () => {
  const { sector } = await buildDirectory(pack(5000), 'freedoom E1M1S');
  const dir = parseDirectory(sector);
  assert.equal(dir.name, 'freedoom E1M1S');
  assert.equal(dir.length, 5000);
  sector[13] ^= 1;
  assert.equal(parseDirectory(sector), null);
});

test('sector plans never leave the stock-safe areas or overwrite foreign data', () => {
  const blank = new Uint8Array(SECTOR).fill(0xff);
  assert.throws(() => sectorPlan(0xa1e000, blank, blank), /outside the stock-safe areas/);   // boot-logo/stock images
  assert.throws(() => sectorPlan(0x900000, blank, blank), /outside the stock-safe areas/);   // rw_freq
  const foreign = new Uint8Array(SECTOR).fill(7);
  assert.throws(() => sectorPlan(0xa20000, foreign, pack(SECTOR)), /did not write/);
  assert.equal(sectorPlan(0xa20000, blank, pack(SECTOR))[0].erase, false);              // erased: program only
});

test('upload writes the pack and directory, verifies, and leaves stock data alone', async () => {
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  const before = sim.chip.slice();
  const data = pack(3 * SECTOR + 100);
  const manifest = await uploadPack(sim.radio, data, { name: 'test pack' });
  assert.equal(manifest.verified, true);
  assert.equal(manifest.sectorsWritten, 5);                 // directory + 4 data sectors
  assert.equal(manifest.complete, true);
  assert.equal(parseDirectory(sim.chip.subarray(DIRECTORY, DIRECTORY + SECTOR)).complete, true);
  assert.deepEqual(sim.chip.subarray(0xa20000, 0xa20000 + data.length), data);
  assert.equal(parseDirectory(sim.chip.subarray(DIRECTORY, DIRECTORY + SECTOR)).name, 'test pack');
  assert.equal(sim.erases, 0);                              // erased targets need no erase
  const changed = [];
  for (let a = 0; a < sim.chip.length; a += SECTOR) {
    if (sim.chip.subarray(a, a + SECTOR).some((b, i) => b !== before[a + i])) changed.push(a);
  }
  assert.deepEqual(changed, [0xa1f000, 0xa20000, 0xa21000, 0xa22000, 0xa23000]);
});

test('re-upload of the same pack writes nothing; a new pack replaces the old one', async () => {
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  await uploadPack(sim.radio, pack(2 * SECTOR), { name: 'one' });
  assert.equal((await uploadPack(sim.radio, pack(2 * SECTOR), { name: 'one' })).sectorsWritten, 0);
  const second = pack(2 * SECTOR, 9);
  const manifest = await uploadPack(sim.radio, second, { name: 'two' });
  assert.equal(manifest.verified, true);
  assert.equal(parseDirectory(sim.chip.subarray(DIRECTORY, DIRECTORY + SECTOR)).name, 'two');
  assert.deepEqual(sim.chip.subarray(0xa20000, 0xa20000 + second.length), second);
});

test('an interrupted upload is marked in progress and resumes where it stopped', async () => {
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  const data = pack(4 * SECTOR);
  sim.failProgramAt = 0xa22000 + 0x200;
  await assert.rejects(uploadPack(sim.radio, data, { name: 'x' }));
  const half = parseDirectory(sim.chip.subarray(DIRECTORY, DIRECTORY + SECTOR));
  assert.equal(half.complete, false);                       // readers must ignore it
  const erasesBefore = sim.erases;
  const manifest = await uploadPack(sim.radio, data, { name: 'x' });
  assert.equal(manifest.complete, true);
  assert.equal(manifest.sectorsWritten, 2);                 // the failed sector and the one after
  assert.equal(sim.erases - erasesBefore, 1);               // only the half-written sector is erased
  assert.equal(parseDirectory(sim.chip.subarray(DIRECTORY, DIRECTORY + SECTOR)).complete, true);
  assert.deepEqual(sim.chip.subarray(0xa20000, 0xa20000 + data.length), data);
});

test('a deep restore clears the tails whatever is there', async () => {
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  const before = sim.chip.slice();
  sim.chip.fill(0x11, 0x3a8000, 0x3a9000);                   // stray data in the voice tail
  await uploadPack(sim.radio, pack(SECTOR), { name: 'd' });
  await restore(sim.radio, { deep: true });
  assert.deepEqual(sim.chip, before);
});

test('restore returns every written sector to erased, stock data untouched', async () => {
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  const before = sim.chip.slice();
  await uploadPack(sim.radio, pack(5 * SECTOR + 7), { name: 'r' });
  const result = await restore(sim.radio);
  assert.equal(result.erased, 7);
  assert.deepEqual(sim.chip, before);
  assert.equal((await restore(sim.radio)).erased, 0);
});
