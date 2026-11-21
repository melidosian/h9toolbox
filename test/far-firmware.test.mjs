import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import { APP_START, APRS_STAGE_START, FLASH_SIZE, RESIDENT_START, SECTOR_SIZE, crc16 } from '../dist/firmware-update/firmware.mjs';
import { FAR_END, FAR_START, backedUpFar, planFarFirmware, planFarFirmwareWithoutBackup, writeFarFirmware } from '../dist/firmware-update/far-region.mjs';

const rc = new URL('../firmware-research/module-loader/combined-preview-release/', import.meta.url);
const old = new URL('../firmware-research/module-loader/smartbeacon-release/', import.meta.url);

async function radio() {
  const candidate = new Uint8Array(await readFile(new URL('h9-toolbox-v0.1.6-rc.5-far.bin', rc)));
  const flash = new Uint8Array(FLASH_SIZE).fill(0xff);
  flash.set(await readFile(new URL('h9-toolbox-aprs-stage.bin', old)), APRS_STAGE_START);
  flash.set(await readFile(new URL('h9-toolbox-aprs-scheduler.bin', old)), RESIDENT_START);
  const backup = backedUpFar(flash.slice(APP_START));
  const writes = [];
  const crcAt = address => crc16(flash.subarray(address, address + SECTOR_SIZE));
  const scan = async (relative, count) => Array.from({ length: count }, (_, i) => crcAt(APP_START + relative + i * SECTOR_SIZE));
  const hid = {
    crcs: scan,
    windowCrcs: scan,
    sectorCrc: async address => crcAt(address),
    erase: async address => {
      assert.ok(address >= FAR_START && address < FAR_END);
      writes.push(address);
      flash.fill(0xff, address, address + SECTOR_SIZE);
    },
    write: async (address, offset, bytes) => flash.set(bytes, address + offset),
  };
  return { candidate, flash, backup, writes, hid };
}

test('combined preview replaces a v0.1.5 stage and restores it from the same archive', async () => {
  const { candidate, flash, backup, writes, hid } = await radio();
  const plan = await planFarFirmware(hid, backup, candidate, 'update');
  await writeFarFirmware(hid, plan, () => {}, () => {});
  assert.deepEqual(flash.slice(FAR_START, FAR_END), candidate);
  assert.ok(writes.some(address => address >= APRS_STAGE_START));
  const restore = await planFarFirmware(hid, backup, candidate, 'restore');
  await writeFarFirmware(hid, restore, () => {}, () => {});
  assert.deepEqual(flash.slice(FAR_START, FAR_END), backup);
});

test('far installer rejects a foreign sector and resumes only the recorded interrupted sector', async () => {
  const { candidate, flash, backup, writes, hid } = await radio();
  const interrupted = FAR_START + SECTOR_SIZE;
  flash[interrupted + 100] = 0;
  await assert.rejects(planFarFirmware(hid, backup, candidate, 'update'), /differs/);
  assert.deepEqual(writes, []);
  const plan = await planFarFirmware(hid, backup, candidate, 'update', interrupted);
  assert.ok(plan.work.includes(interrupted));
  await writeFarFirmware(hid, plan, () => {}, () => {});
  assert.deepEqual(flash.slice(FAR_START, FAR_END), candidate);
});

test('skip-backup flashes the bounded far region and recognizes a verified reinstall', async () => {
  const { candidate, flash, writes, hid } = await radio();
  const plan = await planFarFirmwareWithoutBackup(hid, candidate);
  assert.equal(plan.work.length, (FAR_END - FAR_START) / SECTOR_SIZE);
  await writeFarFirmware(hid, plan, () => {}, () => {});
  assert.deepEqual(flash.slice(FAR_START, FAR_END), candidate);
  assert.equal(writes.length, plan.work.length);
  const reinstall = await planFarFirmwareWithoutBackup(hid, candidate);
  assert.deepEqual(reinstall.work, []);
});
