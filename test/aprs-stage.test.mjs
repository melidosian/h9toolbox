import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import { stageAprs } from '../dist/firmware-update/aprs-stage.mjs';
import { APP_START, APRS_STAGE_START, APRS_STAGE_END, BUILD, RESIDENT_START, RESIDENT_END, SECTOR_SIZE, H9Hid, crc16 } from '../dist/firmware-update/firmware.mjs';

const bundle = new Uint8Array(await readFile(new URL(`../dist/firmware-update/${BUILD.aprsStageFile}`, import.meta.url)));
const size = APRS_STAGE_END - APRS_STAGE_START;

class Radio {
  stage = new Uint8Array(size).fill(0xff);
  writes = [];
  failAfter = Infinity;

  async windowCrcs(relative, count) {
    const start = APP_START + relative - APRS_STAGE_START;
    assert.ok(start >= 0 && start + count * SECTOR_SIZE <= size);
    return Array.from({ length: count }, (_, i) => crc16(this.stage.subarray(start + i * SECTOR_SIZE, start + (i + 1) * SECTOR_SIZE)));
  }
  async crcs(relative, count) {
    assert.equal(relative % SECTOR_SIZE, 0);
    return this.windowCrcs(relative, count);
  }
  async sectorCrc(physical) {
    return (await this.crcs(physical - APP_START, 1))[0];
  }
  async writeAprsStage(physical, offset, bytes) {
    assert.ok(physical >= APRS_STAGE_START && physical < APRS_STAGE_END);
    assert.ok(offset + bytes.length <= SECTOR_SIZE && bytes.length <= 40);
    if (this.writes.length === this.failAfter) throw new Error('test USB disconnect');
    const start = physical - APRS_STAGE_START + offset;
    assert.ok(this.stage.subarray(start, start + bytes.length).every(byte => byte === 0xff));
    this.stage.set(bytes, start);
    this.writes.push([physical, offset]);
  }
}

test('stage the release over HID and put the executable last', async () => {
  const radio = new Radio();
  assert.equal(await stageAprs(radio, bundle), 'ready');
  assert.deepEqual(radio.stage, bundle);
  assert.equal(radio.writes[0][0], APRS_STAGE_END - SECTOR_SIZE);
  assert.equal(radio.writes.at(-1)[0], APRS_STAGE_START);
  const count = radio.writes.length;
  assert.equal(await stageAprs(radio, bundle), 'ready');
  assert.equal(radio.writes.length, count);
});

test('resume a staging sector after writes already acknowledged', async () => {
  const radio = new Radio();
  radio.failAfter = 4;
  await assert.rejects(stageAprs(radio, bundle), /USB disconnect/);
  radio.failAfter = Infinity;
  assert.equal(await stageAprs(radio, bundle), 'ready');
  assert.deepEqual(radio.stage, bundle);
  assert.equal(new Set(radio.writes.map(pair => pair.join(':'))).size, radio.writes.length);
});

test('leave an occupied staging slot and refuse a corrupt download', async () => {
  const radio = new Radio();
  radio.stage[123] = 0x13;
  assert.equal(await stageAprs(radio, bundle), 'occupied');
  assert.equal(radio.writes.length, 0);
  assert.equal(radio.stage[123], 0x13);
  const corrupted = bundle.slice();
  corrupted[0] ^= 1;
  await assert.rejects(stageAprs(radio, corrupted), /did not match/);
  assert.equal(radio.writes.length, 0);
});

test('staging writes are limited to their reserved main flash sectors', async () => {
  const hid = new H9Hid(null);
  const sent = [];
  hid.exchange = async (command, body) => sent.push([command, body]);
  const bytes = Uint8Array.of(1, 2);
  await assert.rejects(hid.writeAprsStage(APRS_STAGE_START - SECTOR_SIZE, 0, bytes), /outside APRS/);
  await assert.rejects(hid.writeAprsStage(APRS_STAGE_END, 0, bytes), /outside APRS/);
  await assert.rejects(hid.write(APRS_STAGE_START, 0, bytes), /outside firmware/);
  await assert.rejects(hid.erase(APRS_STAGE_START), /outside firmware/);
  await hid.writeAprsStage(APRS_STAGE_START, 80, bytes);
  assert.equal(sent.length, 1);
  const [command, body] = sent[0];
  assert.equal(command, 0xc3);
  assert.equal(new DataView(body.buffer).getUint32(0, true), APRS_STAGE_START - APP_START + 80);
  assert.deepEqual(body.slice(8), bytes);
});

test('SmartBeacon scheduler writes stay inside the two reserved sectors', async () => {
  const hid = new H9Hid(null);
  const sent = [];
  hid.exchange = async (command, body) => sent.push([command, body]);
  const bytes = Uint8Array.of(1, 2);
  await assert.rejects(hid.writeSmartBeaconResident(RESIDENT_START - SECTOR_SIZE, 0, bytes), /outside SmartBeacon/);
  await assert.rejects(hid.writeSmartBeaconResident(RESIDENT_END, 0, bytes), /outside SmartBeacon/);
  await assert.rejects(hid.erase(RESIDENT_START), /outside firmware/);
  await hid.writeSmartBeaconResident(RESIDENT_END - SECTOR_SIZE, 80, bytes);
  assert.equal(sent.length, 1);
  const [command, body] = sent[0];
  assert.equal(command, 0xc3);
  assert.equal(new DataView(body.buffer).getUint32(0, true), RESIDENT_END - SECTOR_SIZE + 80 - APP_START);
  assert.deepEqual(body.slice(8), bytes);
});
