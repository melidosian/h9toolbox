import test from 'node:test';
import assert from 'node:assert/strict';
import { APP_START, IMAGE_SIZE, SECTOR_SIZE, crc16, expectedCrcs, H9Hid, loadBackup, makePacket, parseReply, planUpdate, writeSector } from '../dist/firmware-update/firmware.mjs';
import { createHidArchive, knownAppMatches, loadHidArchive, readAccessibleFlash, recoverNextSector, recoverPreviousSector } from '../dist/firmware-update/readback.mjs';

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
  assert.equal(result.method, 'verified-official-image-seed');
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
