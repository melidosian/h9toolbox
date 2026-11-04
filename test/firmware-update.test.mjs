import test from 'node:test';
import assert from 'node:assert/strict';
import { APP_START, IMAGE_SIZE, SECTOR_SIZE, crc16, expectedCrcs, H9Hid, loadBackup, makePacket, parseReply, planUpdate, writeSector } from '../dist/firmware-update/firmware.mjs';

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
