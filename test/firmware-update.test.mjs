import test from 'node:test';
import assert from 'node:assert/strict';
import { APP_START, IMAGE_SIZE, SECTOR_SIZE, crc16, expectedCrcs, H9Hid, loadBackup, makePacket, parseReply, planUpdate, scanRadio, writeSector } from '../dist/firmware-update/firmware.mjs';
import { createHidArchive, knownAppMatches, loadHidArchive, planFirmwareWithoutBackup, readAccessibleFlash, readFirmwareBackup, recoverNextSector, recoverPreviousSector, UnverifiedRadioError } from '../dist/firmware-update/readback.mjs';
import { backedUpFar, FAR_START, FAR_END } from '../dist/firmware-update/far-region.mjs';

test('HID init packet matches captured vendor request', () => {
  const body = new Uint8Array(17);
  body.set(new TextEncoder().encode('app_dir_head'));
  assert.equal(Buffer.from(makePacket(0xc0, body).slice(0, 25)).toString('hex'), 'aa5513008e61dd703e3ed8693c3ed4652f05bc004e61bcb538');
});

test('reply checks CRC, command, and radio status', () => {
  const packet = makePacket(0xc4, new Uint8Array([0x34, 0x12]));
  assert.deepEqual([...parseReply(packet, 0xc4)], [0x34, 0x12]);
  const corrupt = packet.slice(); corrupt[4] ^= 1;
  assert.throws(() => parseReply(corrupt, 0xc4), /CRC/);
  assert.throws(() => parseReply(packet, 0xc2), /command/);
});

test('HID reboot sends command 0xCA and accepts a disconnect that races the send', async () => {
  const events = new EventTarget();
  const device = {
    async sendReport(reportId, report) {
      assert.equal(reportId, 0);
      assert.equal(parseReply(report, 0xca).length, 0);
      events.dispatchEvent(Object.assign(new Event('disconnect'), { device: this }));
      throw new Error('USB closed while sending');
    },
  };
  assert.equal(await new H9Hid(device).reboot({ events, timeoutMs: 100 }), true);
});

test('HID reboot waits for this radio and preserves the send failure', async () => {
  const events = new EventTarget();
  const device = {
    async sendReport() {
      events.dispatchEvent(Object.assign(new Event('disconnect'), { device: {} }));
    },
  };
  const hid = new H9Hid(device);
  assert.equal(await hid.reboot({ events, timeoutMs: 0 }), false);
  device.sendReport = async () => { throw new Error('USB permission lost'); };
  await assert.rejects(hid.reboot({ events, timeoutMs: 0 }), /USB permission lost/);
});

test('WebHID transport sends one report ID zero and accepts identified geometry', async () => {
  const listeners = new Set();
  const device = {
    addEventListener(_name, callback) { listeners.add(callback); },
    removeEventListener(_name, callback) { listeners.delete(callback); },
    async sendReport(reportId, report) {
      assert.equal(reportId, 0);
      assert.equal(report.length, 64);
      const response = new Uint8Array(16), view = new DataView(response.buffer);
      view.setUint32(0, APP_START, true);
      view.setUint32(4, 0x20, true); // named-file length is not the flash capacity
      view.setUint32(8, 0, true);
      view.setUint32(12, SECTOR_SIZE, true);
      queueMicrotask(() => { for (const callback of listeners) callback({ reportId: 0, data: new DataView(makePacket(0xc0, response).buffer) }); });
    },
  };
  assert.deepEqual(await new H9Hid(device).initialize(), { address: APP_START, length: 0x20, eraseOffset: 0, blockSize: SECTOR_SIZE });
  assert.equal(listeners.size, 0);
});

test('backup must preserve protected boot region', async () => {
  const target = new Uint8Array(IMAGE_SIZE), backup = target.slice();
  backup[APP_START] = 1;
  assert.equal((await loadBackup(backup, target)).image.length, IMAGE_SIZE);
  backup[APP_START - 1] = 1;
  await assert.rejects(loadBackup(backup, target), /Boot region differs/);
});

test('plan blocks unknown radio contents and never updates retained VM', () => {
  const original = new Uint8Array(IMAGE_SIZE).fill(0xff);
  const target = original.slice();
  target[APP_START] = 0x12;
  target[IMAGE_SIZE - 1] = 0x34;
  const current = expectedCrcs(original).map(value => value ?? 0);
  assert.deepEqual(planUpdate(current, original, target), [IMAGE_SIZE - SECTOR_SIZE, APP_START]);
  current[0] ^= 1;
  assert.throws(() => planUpdate(current, original, target), /No writes allowed/);
  assert.deepEqual(planUpdate(current, original, target, APP_START), [IMAGE_SIZE - SECTOR_SIZE, APP_START]);
});

test('sector write erases, checks blank CRC, writes header last, and verifies final CRC', async () => {
  const target = new Uint8Array(IMAGE_SIZE).fill(0xff);
  target.fill(0x30, APP_START, APP_START + 100);
  let sector = new Uint8Array(SECTOR_SIZE).fill(0);
  const writes = [];
  const hid = {
    async erase(physical) { assert.equal(physical, APP_START); sector.fill(0xff); },
    async sectorCrc() { return crc16(sector); },
    async write(_physical, offset, bytes) { writes.push(offset); sector.set(bytes, offset); },
  };
  await writeSector(hid, APP_START, target);
  assert.deepEqual(writes, [32, 72, 0]);
  assert.deepEqual(sector, target.slice(APP_START, APP_START + SECTOR_SIZE));
});

test('sliding CRCs recover every byte in both directions', () => {
  const previous = Uint8Array.from({ length: SECTOR_SIZE }, (_, index) => (index * 31 + 17) & 0xff);
  const next = Uint8Array.from({ length: SECTOR_SIZE }, (_, index) => (index * 47 + 93) & 0xff);
  const joined = new Uint8Array(SECTOR_SIZE * 2);
  joined.set(previous); joined.set(next, SECTOR_SIZE);
  const windows = Uint16Array.from({ length: SECTOR_SIZE }, (_, shift) => crc16(joined.subarray(shift, shift + SECTOR_SIZE)));
  assert.deepEqual(recoverNextSector(previous, windows, crc16(next)), next);
  assert.deepEqual(recoverPreviousSector(next, windows, crc16(previous)), previous);
  const evenWindows = windows.map((crc, shift) => shift % 2 ? 0 : crc);
  assert.deepEqual(recoverNextSector(previous, evenWindows, crc16(next), 2), next);
  assert.deepEqual(recoverPreviousSector(next, evenWindows, crc16(previous), 2), previous);
  const corrupt = windows.slice(); corrupt[1024] ^= 1;
  assert.throws(() => recoverNextSector(previous, corrupt, crc16(next)), /CRC recovery failed|CRC mismatch/);
});

test('HID backup archive is verified and supplies application bytes without claiming boot bytes', async () => {
  const accessible = Uint8Array.from({ length: 0x100000 - APP_START }, (_, index) => index % 251);
  const target = new Uint8Array(IMAGE_SIZE).fill(0xaa);
  const archive = await createHidArchive(accessible, 'test-seed');
  const parsed = await loadHidArchive(archive, target);
  assert.deepEqual(parsed.image.subarray(0, APP_START), target.subarray(0, APP_START));
  assert.deepEqual(parsed.image.subarray(APP_START), accessible.subarray(0, IMAGE_SIZE - APP_START));
  assert.deepEqual(parsed.accessible, accessible);
  const corrupt = archive.slice(); corrupt[corrupt.length - 1] ^= 1;
  await assert.rejects(loadHidArchive(corrupt, target), /checksum mismatch/);
});

test('browser readback reconstructs and verifies accessible flash from a known application', async () => {
  const target = new Uint8Array(IMAGE_SIZE).fill(0xff);
  const accessible = new Uint8Array(0x100000 - APP_START).fill(0xff);
  const appSectors = (IMAGE_SIZE - APP_START) / SECTOR_SIZE;
  for (let offset = 0; offset < SECTOR_SIZE; offset++) accessible[appSectors * SECTOR_SIZE + offset] = (offset * 29 + 7) & 0xff;
  const sectorCrcs = Array.from({ length: accessible.length / SECTOR_SIZE }, (_, index) => crc16(accessible.subarray(index * SECTOR_SIZE, (index + 1) * SECTOR_SIZE)));
  const blankCrc = crc16(new Uint8Array(SECTOR_SIZE).fill(0xff));
  const crossing = new Map();
  for (const boundary of [appSectors - 1, appSectors]) {
    crossing.set(boundary, Uint16Array.from({ length: SECTOR_SIZE }, (_, shift) => crc16(accessible.subarray(boundary * SECTOR_SIZE + shift, (boundary + 1) * SECTOR_SIZE + shift))));
  }
  const hid = {
    async crcs(relative, count) { return sectorCrcs.slice(relative / SECTOR_SIZE, relative / SECTOR_SIZE + count); },
    async windowCrcs(relative, count) {
      const boundary = Math.floor(relative / SECTOR_SIZE), shift = relative % SECTOR_SIZE;
      return Array.from({ length: count }, (_, index) => crossing.get(boundary + index)?.[shift] ?? blankCrc);
    },
  };
  const result = await readAccessibleFlash(hid, sectorCrcs, target);
  assert.equal(result.method, 'verified-known-image-seed');
  assert.deepEqual(result.accessible, accessible);
});

test('quick backup of a known application skips the settings area and round-trips', async () => {
  const target = new Uint8Array(IMAGE_SIZE);
  for (let i = 0; i < target.length; i++) target[i] = (i * 31 + (i >> 9)) & 255;
  const radio = new Uint8Array(0x100000).fill(0xff);
  radio.set(target);
  radio.fill(0x42, 0xe0000, 0xe1000); // settings data the quick backup must not need
  const sectorCrcs = Array.from({ length: (0x100000 - APP_START) / SECTOR_SIZE },
    (_, i) => crc16(radio.subarray(APP_START + i * SECTOR_SIZE, APP_START + (i + 1) * SECTOR_SIZE)));
  let windowRequests = 0;
  const hid = {
    async windowCrcs(relative, count) {
      windowRequests++;
      return Array.from({ length: count }, (_, i) => {
        const start = APP_START + relative + i * SECTOR_SIZE;
        return crc16(radio.subarray(start, start + SECTOR_SIZE));
      });
    },
    async crcs(relative, count) { return sectorCrcs.slice(relative / SECTOR_SIZE, relative / SECTOR_SIZE + count); },
  };
  const quick = await readAccessibleFlash(hid, sectorCrcs, target, () => {}, () => false, { settings: false });
  assert.equal(quick.settings, false);
  assert.equal(quick.accessible.length, IMAGE_SIZE - APP_START);
  assert.ok(windowRequests < 120, `quick backup used ${windowRequests} window requests`);
  const archive = await createHidArchive(quick.accessible, quick.method);
  const loaded = await loadHidArchive(archive, target);
  assert.deepEqual(loaded.image, target);
  const corrupt = archive.slice(); corrupt[corrupt.length - 1] ^= 1;
  await assert.rejects(loadHidArchive(corrupt, target), /checksum mismatch/);
  // An unknown application cannot take the shortcut: the known-image check
  // fails, so readAccessibleFlash falls back to full recovery.
  const other = target.slice(); other[0x40000] ^= 1;
  assert.equal(await knownAppMatches(hid, sectorCrcs, other), false);
});

test('combined backup verifies main and far firmware without reconstructing settings', async () => {
  const target = Uint8Array.from({length: IMAGE_SIZE}, (_, i) => (i * 31 + (i >> 9)) & 255);
  const far = new Uint8Array(FAR_END - FAR_START).fill(0xff);
  const radio = new Uint8Array(0x100000).fill(0xff);
  radio.set(target);
  radio.fill(0x42, IMAGE_SIZE, FAR_START);
  radio.fill(0x17, FAR_END);
  let windowRequests = 0;
  const hid = {
    async crcs(relative, count) {
      return Array.from({length: count}, (_, i) => {
        const start = APP_START + relative + i * SECTOR_SIZE;
        return crc16(radio.subarray(start, start + SECTOR_SIZE));
      });
    },
    async windowCrcs(relative, count) {
      windowRequests++;
      const start = APP_START + relative;
      const end = start + count * SECTOR_SIZE;
      assert.ok(end <= IMAGE_SIZE || (start >= FAR_START && end <= FAR_END), 'shifted read outside firmware');
      return this.crcs(relative, count);
    },
  };
  const scan = await scanRadio(hid);
  const quick = await readFirmwareBackup(hid, scan, [target], []);
  assert.equal(windowRequests, 96);
  assert.deepEqual(quick.accessible, target.subarray(APP_START));
  assert.deepEqual(quick.far, far);
  const archive = await createHidArchive(quick.accessible, quick.method, quick.far);
  const loaded = await loadHidArchive(archive, target);
  assert.deepEqual(loaded.image, target);
  assert.deepEqual(backedUpFar(loaded.accessible), far);
  const corrupt = archive.slice(); corrupt[corrupt.length - 1] ^= 1;
  await assert.rejects(loadHidArchive(corrupt, target), /checksum mismatch/);

  // A changed first sector needs a reverse read. A changed last far sector
  // needs a forward read. Both must survive in the actual recovery bytes.
  radio[APP_START + 13] ^= 0x52;
  radio[FAR_END - 7] ^= 0x19;
  windowRequests = 0;
  const changedScan = await scanRadio(hid);
  const partial = await readFirmwareBackup(hid, changedScan, [target], []);
  assert.deepEqual(partial.accessible, radio.subarray(APP_START, IMAGE_SIZE));
  assert.deepEqual(partial.far, radio.subarray(FAR_START, FAR_END));
  assert.ok(windowRequests < 4300, `partial backup used ${windowRequests} window requests`);

  await assert.rejects(readFirmwareBackup(hid, changedScan, [target], [], () => {}, () => true),
    /backup cancelled/i);
  await assert.rejects(readFirmwareBackup(hid, scan, [target], []), /CRCs changed during backup/);
});

test('unrecognised firmware falls back to erased-sector recovery with two-byte shifts', async () => {
  const accessible = new Uint8Array(0x100000 - APP_START).fill(0x71);
  // Keep the sole erased pair outside the managed regions.
  const blank = (IMAGE_SIZE - APP_START) / SECTOR_SIZE;
  accessible.fill(0xff, blank * SECTOR_SIZE, (blank + 2) * SECTOR_SIZE);
  const regularCrc = crc16(accessible.subarray(0, SECTOR_SIZE));
  const blankCrc = crc16(accessible.subarray(blank * SECTOR_SIZE, (blank + 1) * SECTOR_SIZE));
  const sectorCrcs = Array.from({length: accessible.length / SECTOR_SIZE}, (_, i) =>
    i === blank || i === blank + 1 ? blankCrc : regularCrc);
  let windowRequests = 0;
  const hid = {
    async crcs(relative, count) {
      const index = relative / SECTOR_SIZE;
      return sectorCrcs.slice(index, index + count);
    },
    async windowCrcs(relative, count) {
      windowRequests++;
      const first = Math.floor(relative / SECTOR_SIZE);
      return Array.from({length: count}, (_, i) => {
        const boundary = first + i;
        const start = relative + i * SECTOR_SIZE;
        return boundary === blank - 1 || boundary === blank + 1
          ? crc16(accessible.subarray(start, start + SECTOR_SIZE))
          : sectorCrcs[boundary];
      });
    },
  };
  const backup = await readFirmwareBackup(hid, sectorCrcs, [], []);
  assert.equal(backup.method, 'verified-erased-sector-seed');
  assert.deepEqual(backup.accessible, accessible.subarray(0, IMAGE_SIZE - APP_START));
  assert.deepEqual(backup.far, accessible.subarray(FAR_START - APP_START, FAR_END - APP_START));
  assert.equal(windowRequests, 32854);
});

test('quick install and stock recovery need only the model windows when other sectors differ', async () => {
  const { readFile } = await import('node:fs/promises');
  const { BUILD, decodePackage, loadBuildPackage, loadStockSeed, stockDifferences } = await import('../dist/firmware-update/firmware.mjs');
  const file = async url => new Uint8Array(await readFile(new URL(url, import.meta.url)));
  const build = await loadBuildPackage(await file(`../dist/firmware-update/${BUILD.packageFile}`));
  const stock = await loadStockSeed(await file(`../dist/firmware-update/${BUILD.stockSeedFile}`), build.image);
  const toolbox = decodePackage(await file('../firmware-research/module-loader/combined-preview-release/h9-toolbox-firmware-v0.1.6-rc.5.fw'));
  const flash = new Uint8Array(0x100000).fill(0xff);
  flash.set(toolbox);
  flash[0x40010] ^= 0x19; // A sector no published image can supply.
  flash.fill(0xAB, 0x5000, 0x6000); // Interrupted write to the app header.
  const crcs = async (relative, count) => Array.from({length: count}, (_, i) => {
    const start = APP_START + relative + i * SECTOR_SIZE;
    return crc16(flash.subarray(start, start + SECTOR_SIZE));
  });
  let shifted = 0;
  const hid = { crcs, async windowCrcs(relative, count) { shifted++; return crcs(relative, count); } };
  const current = await scanRadio(hid);
  for (const target of [toolbox, stock]) {
    shifted = 0;
    const plan = await planFirmwareWithoutBackup(hid, current, target);
    assert.deepEqual(plan.update, stockDifferences(current, target).reverse());
    assert.equal(plan.update.at(-1), APP_START);
    assert.ok(plan.update.includes(0x40000));
    assert.equal(plan.verifyShiftedWindows, true);
    assert.equal(shifted, 6);
  }
  await assert.rejects(planFirmwareWithoutBackup({ windowCrcs: async () => [undefined] }, current, stock), UnverifiedRadioError);
  flash[0xb672a] ^= 1;
  await assert.rejects(planFirmwareWithoutBackup(hid, await scanRadio(hid), stock), UnverifiedRadioError);
});

test('hosted H9 Toolbox Firmware files rebuild every known image and plan only pinned sectors', async () => {
  const { readFile } = await import('node:fs/promises');
  const { BUILD, PREVIOUS, STOCK_IMAGE_SHA256, loadBuildPackage, loadPreviousImage, loadStockSeed, classifyRadio, describeMismatch, stockCompatible, verifyStockCompatible, looksLikeH9, sha256 } = await import('../dist/firmware-update/firmware.mjs');
  const file = name => readFile(new URL(`../dist/firmware-update/${name}`, import.meta.url)).then(b => new Uint8Array(b));
  const build = await loadBuildPackage(await file(BUILD.packageFile));
  const stock = await loadStockSeed(await file(BUILD.stockSeedFile), build.image);
  assert.equal(await sha256(stock), STOCK_IMAGE_SHA256);
  const named = [['stock', stock], ['build', build.image]];
  for (const info of PREVIOUS) named.push([info.id, await loadPreviousImage(await file(info.seedFile), build.image, info)]);
  const known = named.slice(2).map(([, image]) => image);
  for (const [name, image] of named) {
    const crcs = expectedCrcs(image);
    const scan = crcs.map(crc => crc ?? 0);
    assert.equal(classifyRadio(scan, named), name);
    if (name === 'stock') {   // a stock radio with one stray sector: named, with that sector
      const odd = scan.slice(); odd[(0xd9000 - APP_START) / SECTOR_SIZE] ^= 1;
      assert.throws(() => classifyRadio(odd, named), /Closest: stock v1\.0\.38, 1 of \d+ firmware sectors differ \(0xd9000\)/);
      assert.match(describeMismatch(odd, named), /0xd9000/);
      // permissive: stock apart from pinned (rewritten) sectors still installs; a stray sector elsewhere does not
      const pinned = scan.slice(); pinned[(BUILD.sectors[0] - APP_START) / SECTOR_SIZE] ^= 1; pinned[(BUILD.sectors.at(-1) - APP_START) / SECTOR_SIZE] ^= 1;
      assert.deepEqual(stockCompatible(pinned, stock), [BUILD.sectors[0], BUILD.sectors.at(-1)]);
      const stray = scan.slice(); stray[(0x30000 - APP_START) / SECTOR_SIZE] ^= 1;
      assert.equal(stockCompatible(stray, stock), null);
      assert.doesNotThrow(() => verifyStockCompatible(stock, stock));
      // full-image install of an unrecognised H9: the plan is exactly the differing sectors
      assert.ok(looksLikeH9(stock) && !looksLikeH9(new Uint8Array(stock.length)));
      const odd2 = stock.slice(); odd2[0x30010] ^= 1; odd2[0x80010] ^= 1;
      const plan2 = planUpdate(expectedCrcs(odd2).map(c => c ?? 0), odd2, build.image, null, []);
      for (const physical of [0x30000, 0x80000]) assert.ok(plan2.includes(physical), `plan misses 0x${physical.toString(16)}`);
      assert.ok(plan2.length <= BUILD.sectors.length + 2, 'plan rewrites only the pinned sectors plus the two differing ones');
      const leftover = stock.slice(); leftover[BUILD.sectors.at(-1)] ^= 0x55;
      assert.doesNotThrow(() => verifyStockCompatible(leftover, stock));
      const hacked = stock.slice(); hacked[0x30000] ^= 0x55;
      assert.throws(() => verifyStockCompatible(hacked, stock), /stock v1\.0\.38 main-firmware backup/);
    }
    const plan = planUpdate(scan, image, build.image, null, known);
    assert.ok(plan.every(physical => BUILD.sectors.includes(physical)), `${name}: plan leaves the pinned sectors`);
    const writes = plan.filter(physical => crcs[(physical - APP_START) / SECTOR_SIZE] !== expectedCrcs(build.image)[(physical - APP_START) / SECTOR_SIZE]);
    assert.equal(writes.length > 0, name !== 'build', `${name}: ${writes.length} sector(s) to write`);
  }
});
