import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import test from 'node:test';
import {
  APP_START, BUILD, FLASH_SIZE, IMAGE_SIZE, PREVIOUS, SECTOR_SIZE, STOCK_IMAGE_SHA256,
  classifyRadio, crc16, expectedCrcs, loadBackup, loadBuildPackage, loadPreviousImage,
  loadStockPackage, loadStockSeed, planUpdate, sha256, verifyStockBackup, writeSector,
} from '../../dist/firmware-update/firmware.mjs';
import { createHidArchive, knownAppMatches, loadHidArchive } from '../../dist/firmware-update/readback.mjs';
import { extractPinnedStockFirmware } from '../../dist/firmware-update/stock-source.mjs';

const site = new URL('../../dist/firmware-update/', import.meta.url);
const bytes = async name => new Uint8Array(await readFile(fileURLToPath(new URL(name, site))));
const localZip = new URL('./input/TDH9-Type-C-1.0.38.zip', import.meta.url);
const descending = [...BUILD.sectors].sort((a, b) => b - a);
const blank = crc16(new Uint8Array(SECTOR_SIZE).fill(0xff));
const crcsOf = image => expectedCrcs(image).map(value => value ?? blank);

async function hosted() {
  const build = await loadBuildPackage(await bytes(BUILD.packageFile));
  const stock = await loadStockSeed(await bytes(BUILD.stockSeedFile), build.image);
  const previous = await loadPreviousImage(await bytes(PREVIOUS.seedFile), build.image);
  return { build: build.image, stock, previous };
}

// Sliding-window CRCs over a simulated radio flash, as HID command C4 returns.
function windowHid(radio) {
  return { async windowCrcs(relative, count) {
    return Array.from({ length: count }, (_, index) => {
      const start = APP_START + relative + index * SECTOR_SIZE;
      return crc16(radio.subarray(start, start + SECTOR_SIZE));
    });
  } };
}

test('pinned GitHub Type-C ZIP extracts the exact stock package', { skip: !existsSync(fileURLToPath(localZip)) }, async () => {
  const zip = new Uint8Array(await readFile(fileURLToPath(localZip)));
  const firmware = await extractPinnedStockFirmware(zip);
  const stock = await loadStockPackage(firmware);
  assert.equal(stock.kind, 'stock');
  zip[0] ^= 1;
  await assert.rejects(extractPinnedStockFirmware(zip), /pinned SHA-256/);
});

test('hosted build and seeds rebuild exact stock and the original Brick Breaker', async () => {
  const { build, stock, previous } = await hosted();
  assert.equal(await sha256(stock), STOCK_IMAGE_SHA256);
  assert.equal(await sha256(previous), PREVIOUS.imageSha256);
  await verifyStockBackup(stock);
  assert.deepEqual(stock.subarray(0, APP_START), build.subarray(0, APP_START));
  // The original game, launcher and hook bytes are unchanged in the build.
  for (const [start, length] of [[0xa77f0, 1628], [0xa9d9e, 94], [0x88906, 4]]) {
    const at = start + 0x5100;
    assert.deepEqual(build.subarray(at, at + length), previous.subarray(at, at + length));
  }
});

test('install plan from stock is exactly the eight pinned sectors', async () => {
  const { build, stock, previous } = await hosted();
  const current = crcsOf(stock);
  assert.equal(classifyRadio(current, [['stock', stock], ['build', build], ['previous', previous]]), 'stock');
  assert.deepEqual(planUpdate(current, stock, build, null, [previous]), descending);
  assert.deepEqual(planUpdate(current, stock, build), descending);
});

test('a radio running the original Brick Breaker upgrades directly', async () => {
  const { build, stock, previous } = await hosted();
  const current = crcsOf(previous);
  assert.equal(classifyRadio(current, [['stock', stock], ['build', build], ['previous', previous]]), 'previous');
  assert.throws(() => planUpdate(current, stock, build), /Radio differs from the supplied backup/);
  assert.deepEqual(planUpdate(current, stock, build, null, [previous]), descending);
  const radio = new Uint8Array(FLASH_SIZE).fill(0xff);
  radio.set(previous);
  assert.equal(await knownAppMatches(windowHid(radio), current, previous), true);
  assert.equal(await knownAppMatches(windowHid(radio), current, build), false);
});

test('an interrupted upgrade (old and new sectors mixed) still plans and verifies', async () => {
  const { build, stock, previous } = await hosted();
  const radio = new Uint8Array(FLASH_SIZE).fill(0xff);
  radio.set(previous);
  for (const physical of descending.slice(0, 3)) radio.set(build.subarray(physical, physical + SECTOR_SIZE), physical);
  const current = expectedCrcs(radio.subarray(0, IMAGE_SIZE)).map(value => value ?? blank);
  for (let index = current.length; index < (FLASH_SIZE - APP_START) / SECTOR_SIZE; index++) current.push(blank);
  const scan = Array.from({ length: (FLASH_SIZE - APP_START) / SECTOR_SIZE },
    (_, index) => crc16(radio.subarray(APP_START + index * SECTOR_SIZE, APP_START + (index + 1) * SECTOR_SIZE)));
  assert.deepEqual(planUpdate(scan, stock, build, null, [previous]), descending);
  assert.throws(() => classifyRadio(scan, [['stock', stock], ['build', build], ['previous', previous]]), /neither exact stock/);
  assert.equal(await knownAppMatches(windowHid(radio), scan, radio.slice(0, IMAGE_SIZE)), true);
});

test('unknown radio contents are refused', async () => {
  const { build, stock, previous } = await hosted();
  const other = stock.slice();
  other[0x30000] ^= 1;
  const current = crcsOf(other);
  assert.throws(() => planUpdate(current, stock, build, null, [previous]), /Radio differs from the supplied backup at 0x30000/);
  assert.throws(() => classifyRadio(current, [['stock', stock], ['build', build], ['previous', previous]]), /neither exact stock/);
});

test('HID archives: stock is a restore point, installed builds are not', async () => {
  const { build, stock, previous } = await hosted();
  for (const [image, isStock] of [[stock, true], [build, false], [previous, false]]) {
    const accessible = new Uint8Array(FLASH_SIZE - APP_START).fill(0xff);
    accessible.set(image.subarray(APP_START));
    const archive = await createHidArchive(accessible, 'verified-known-image-seed');
    const loaded = await loadHidArchive(archive, build);
    assert.deepEqual(loaded.image, image);
    if (isStock) await verifyStockBackup(loaded.image);
    else await assert.rejects(verifyStockBackup(loaded.image), /exact stock/);
  }
  await assert.rejects(loadBackup(stock, new Uint8Array(IMAGE_SIZE)), /Boot region differs/);
});

test('sector writer installs, upgrades, and restores byte for byte', async () => {
  const { build, stock, previous } = await hosted();
  for (const start of [stock, previous]) {
    const radio = start.slice();
    const hid = {
      async erase(physical) { radio.fill(0xff, physical, physical + SECTOR_SIZE); },
      async write(physical, offset, part) { radio.set(part, physical + offset); },
      async sectorCrc(physical) { return crc16(radio.subarray(physical, physical + SECTOR_SIZE)); },
    };
    for (const physical of planUpdate(crcsOf(radio), stock, build, null, [previous])) await writeSector(hid, physical, build);
    assert.deepEqual(radio, build);
    for (const physical of descending) await writeSector(hid, physical, stock);
    assert.deepEqual(radio, stock);
  }
});

test('altered package and seeds are rejected before planning', async () => {
  const packageBytes = await bytes(BUILD.packageFile);
  packageBytes[0x210] ^= 1;
  await assert.rejects(loadBuildPackage(packageBytes), /verified build/);
  const build = await loadBuildPackage(await bytes(BUILD.packageFile));
  const seed = await bytes(BUILD.stockSeedFile);
  seed[0] ^= 1;
  await assert.rejects(loadStockSeed(seed, build.image), /corrupt or unexpected/);
  const previous = await bytes(PREVIOUS.seedFile);
  previous[100] ^= 1;
  await assert.rejects(loadPreviousImage(previous, build.image), /corrupt or unexpected/);
});

test('a radio already running the build reinstalls (nothing to write) and restores to rebuilt stock', async () => {
  const { build, stock, previous } = await hosted();
  // Restore point: exact official stock rebuilt from the hosted files.
  await verifyStockBackup(stock);
  const onBuild = crcsOf(build);
  const changed = planUpdate(onBuild, stock, build, null, [previous]);
  assert.deepEqual(changed, descending);
  const target = expectedCrcs(build), restore = expectedCrcs(stock);
  const index = physical => (physical - APP_START) / SECTOR_SIZE;
  assert.deepEqual(changed.filter(p => onBuild[index(p)] !== target[index(p)]), []);
  assert.deepEqual(changed.filter(p => onBuild[index(p)] !== restore[index(p)]), descending);
  // Updating from the original Brick Breaker with the same restore point.
  const onPrevious = crcsOf(previous);
  assert.equal(planUpdate(onPrevious, stock, build, null, [previous])
    .filter(p => onPrevious[index(p)] !== target[index(p)]).length, PREVIOUS.sectors.length);
});
