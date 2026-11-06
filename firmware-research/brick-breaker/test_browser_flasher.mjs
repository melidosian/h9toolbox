import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
import {
  APP_START, BRICK_BREAKER_SECTORS, FLASH_SIZE, IMAGE_SIZE, SECTOR_SIZE,
  crc16, expectedCrcs, loadBackup, loadBrickBreakerPackage,
  loadBrickBreakerStockSeed, planUpdate,
  verifyBrickBreakerBackup, writeSector,
} from '../../dist/firmware-update/firmware.mjs';
import { createHidArchive, knownAppMatches, loadHidArchive } from '../../dist/firmware-update/readback.mjs';

const site = new URL('../../dist/firmware-update/', import.meta.url);
const bytes = async url => new Uint8Array(await readFile(fileURLToPath(url)));
const expectedSectors = [...BRICK_BREAKER_SECTORS].sort((a, b) => b - a);

test('hosted package and stock seed produce exactly the pinned five-sector plan', async () => {
  const game = await loadBrickBreakerPackage(await bytes(new URL('brick-breaker-v1.0.38.fw', site)));
  const seed = await loadBrickBreakerStockSeed(await bytes(new URL('brick-breaker-stock-sectors.bin', site)), game.image);
  await verifyBrickBreakerBackup(seed);
  const current = expectedCrcs(seed).map(value => value ?? crc16(new Uint8Array(SECTOR_SIZE).fill(0xff)));
  assert.deepEqual(planUpdate(current, seed, game.image), expectedSectors);
  const radio = new Uint8Array(FLASH_SIZE).fill(0xff);
  radio.set(seed);
  const hid = { async windowCrcs(relative, count) {
    return Array.from({ length: count }, (_, index) => {
      const start = APP_START + relative + index * SECTOR_SIZE;
      return crc16(radio.subarray(start, start + SECTOR_SIZE));
    });
  } };
  assert.equal(await knownAppMatches(hid, current, seed), true);
  assert.equal(await knownAppMatches(hid, current, game.image), false);
  const changed = BRICK_BREAKER_SECTORS[0];
  radio.set(game.image.subarray(changed, changed + SECTOR_SIZE), changed);
  const mixed = seed.slice(); mixed.set(game.image.subarray(changed, changed + SECTOR_SIZE), changed);
  current[(changed - APP_START) / SECTOR_SIZE] = crc16(radio.subarray(changed, changed + SECTOR_SIZE));
  assert.equal(await knownAppMatches(hid, current, mixed), true);
});

test('HID backup archive restores a stock backup against the hosted game target', async () => {
  const game = await loadBrickBreakerPackage(await bytes(new URL('brick-breaker-v1.0.38.fw', site)));
  const stock = await loadBrickBreakerStockSeed(await bytes(new URL('brick-breaker-stock-sectors.bin', site)), game.image);
  const accessible = new Uint8Array(FLASH_SIZE - APP_START).fill(0xff);
  accessible.set(stock.subarray(APP_START), 0);
  const archive = await createHidArchive(accessible, 'verified-known-image-seed');
  const backup = await loadHidArchive(archive, game.image);
  await verifyBrickBreakerBackup(backup.image);
  assert.deepEqual(backup.image, stock);
  await assert.rejects(verifyBrickBreakerBackup(game.image), /exact stock/);
  const mismatched = stock.slice(); mismatched[0x8000] ^= 1;
  await assert.rejects(verifyBrickBreakerBackup(mismatched), /exact stock/);
  await assert.rejects(loadBackup(stock, new Uint8Array(IMAGE_SIZE)), /Boot region differs/);
});

test('browser sector writer installs and restores only the five game sectors', async () => {
  const game = await loadBrickBreakerPackage(await bytes(new URL('brick-breaker-v1.0.38.fw', site)));
  const stock = await loadBrickBreakerStockSeed(await bytes(new URL('brick-breaker-stock-sectors.bin', site)), game.image);
  const radio = stock.slice();
  const hid = {
    async erase(physical) { radio.fill(0xff, physical, physical + SECTOR_SIZE); },
    async write(physical, offset, part) { radio.set(part, physical + offset); },
    async sectorCrc(physical) { return crc16(radio.subarray(physical, physical + SECTOR_SIZE)); },
  };
  for (const physical of expectedSectors) await writeSector(hid, physical, game.image);
  assert.deepEqual(radio, game.image);
  for (const physical of expectedSectors) await writeSector(hid, physical, stock);
  assert.deepEqual(radio, stock);
});

test('altered package and stock seed are rejected before planning', async () => {
  const gameBytes = await bytes(new URL('brick-breaker-v1.0.38.fw', site));
  gameBytes[0x210] ^= 1;
  await assert.rejects(loadBrickBreakerPackage(gameBytes), /verified brick-breaker/);
  const game = await loadBrickBreakerPackage(await bytes(new URL('brick-breaker-v1.0.38.fw', site)));
  const seed = await bytes(new URL('brick-breaker-stock-sectors.bin', site));
  seed[0] ^= 1;
  await assert.rejects(loadBrickBreakerStockSeed(seed, game.image), /corrupt or unexpected/);
});
