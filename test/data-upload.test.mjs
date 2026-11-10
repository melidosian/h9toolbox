import assert from 'node:assert/strict';
import test from 'node:test';
import {
  AREAS, DIRECTORY, MODULES_DIRECTORY, MUSIC_DIRECTORY, SLOTS, buildDirectory, parseDirectory, planLayout, restore,
  sectorPlan, uploadPack,
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
  const resourceRoom = MODULES_DIRECTORY - (DIRECTORY + SECTOR);  // the modules and music slots follow
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
  assert.equal(manifest.bytesProgrammed, 5 * SECTOR);
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

test('the live write rate counts every programmed sector as it completes', async () => {
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  let clock = 0;
  const rates = [];
  await uploadPack(sim.radio, pack(4 * SECTOR), {
    name: 'rate', now: () => (clock += 250),
    onProgress: (fraction, phase, rate) => { if (rate !== undefined && phase.startsWith('Writing')) rates.push(rate); },
  });
  assert.equal(rates.length, 5);
  assert.ok(rates.every(r => r > 0));
  assert.ok(rates.at(-1) > rates[0] * 0.5, `rate should not decay toward zero: ${rates}`);
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

test('music slot: own directory, last 124 KiB of the resource tail, never touches the game pack', async () => {
  assert.deepEqual(planLayout(10, SLOTS.music), [{ area: 'music', address: MUSIC_DIRECTORY + SECTOR, length: 10 }]);
  assert.throws(() => planLayout(200 << 10, SLOTS.music), /music slot/);
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  const game = pack(2 * SECTOR + 7, 3);
  await uploadPack(sim.radio, game, { name: 'game' });
  const song = pack(SECTOR + 33, 9);
  const manifest = await uploadPack(sim.radio, song, { name: 'E1M1', slot: SLOTS.music });
  assert.equal(manifest.slot, 'music');
  assert.equal(parseDirectory(sim.chip.subarray(MUSIC_DIRECTORY, MUSIC_DIRECTORY + SECTOR)).complete, true);
  assert.deepEqual(sim.chip.subarray(MUSIC_DIRECTORY + SECTOR, MUSIC_DIRECTORY + SECTOR + song.length), song);
  // the game pack is untouched by the music upload and by a music restore
  assert.deepEqual(sim.chip.subarray(0xa20000, 0xa20000 + game.length), game);
  await restore(sim.radio, { slot: SLOTS.music });
  assert.ok(sim.chip.subarray(MUSIC_DIRECTORY, 0xd00000).every(b => b === 0xff));
  assert.deepEqual(sim.chip.subarray(0xa20000, 0xa20000 + game.length), game);
  assert.equal(parseDirectory(sim.chip.subarray(DIRECTORY, DIRECTORY + SECTOR)).complete, true);
});

test('modules slot: own directory between the game pack and music, matches the loader stub', async () => {
  // firmware-research/module-loader/src/stub.s reads the directory at 0xCC0000
  // and the bundle at 0xCC1000 (one segment, at most 124 KiB).
  assert.equal(MODULES_DIRECTORY, 0xcc0000);
  assert.deepEqual(planLayout(10, SLOTS.modules), [{ area: 'modules', address: 0xcc1000, length: 10 }]);
  assert.equal(planLayout(124 << 10, SLOTS.modules).length, 1);
  assert.throws(() => planLayout((124 << 10) + 1, SLOTS.modules), /modules slot/);
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  const game = pack(2 * SECTOR + 7, 3);
  const song = pack(SECTOR + 33, 9);
  await uploadPack(sim.radio, game, { name: 'game' });
  await uploadPack(sim.radio, song, { name: 'E1M1', slot: SLOTS.music });
  const bundle = pack(3468, 5);
  assert.equal((await uploadPack(sim.radio, bundle, { name: 'H9 modules', slot: SLOTS.modules })).slot, 'modules');
  const dir = parseDirectory(sim.chip.subarray(MODULES_DIRECTORY, MODULES_DIRECTORY + SECTOR));
  assert.equal(dir.complete, true);
  assert.deepEqual(dir.segments, [{ address: 0xcc1000, length: bundle.length }]);
  assert.deepEqual(sim.chip.subarray(0xcc1000, 0xcc1000 + bundle.length), bundle);
  await restore(sim.radio, { slot: SLOTS.modules });
  assert.ok(sim.chip.subarray(MODULES_DIRECTORY, MUSIC_DIRECTORY).every(b => b === 0xff));
  assert.deepEqual(sim.chip.subarray(0xa20000, 0xa20000 + game.length), game);
  assert.deepEqual(sim.chip.subarray(MUSIC_DIRECTORY + SECTOR, MUSIC_DIRECTORY + SECTOR + song.length), song);
});

test('an upload that is already complete reads only its directory sector', async () => {
  const sim = new SimulatedRadio();
  stockLikeChip(sim);
  const data = pack(5 * SECTOR + 9, 4);
  let before = sim.reads;
  await uploadPack(sim.radio, data, { name: 'game' });
  const fullUpload = sim.reads - before;
  before = sim.reads;
  const again = await uploadPack(sim.radio, data, { name: 'game' });
  assert.equal(again.alreadyComplete, true);
  assert.equal(again.sectorsWritten, 0);
  // only the 4 KiB directory: a small fraction of the reads a real upload makes
  assert.ok((sim.reads - before) * 5 < fullUpload, `read ${sim.reads - before} times (upload: ${fullUpload})`);
  // a different pack still goes the long way and replaces it
  const other = await uploadPack(sim.radio, pack(3 * SECTOR, 8), { name: 'other' });
  assert.equal(other.alreadyComplete, undefined);
  assert.equal(parseDirectory(sim.chip.subarray(DIRECTORY, DIRECTORY + SECTOR)).name, 'other');
});
