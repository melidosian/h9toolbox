import assert from 'node:assert/strict';
import test from 'node:test';
import {
  APP_START, FLASH_SIZE, IMAGE_SIZE, SECTOR_SIZE, crc16,
} from '../dist/firmware-update/firmware.mjs';
import {
  FAR_END, FAR_START, checkDoomCode, checkPack, checkSong, doomTarget, inWritableArea, planInstall,
} from '../dist/doom/install.mjs';

const crcsOf = whole => {
  const out = [];
  for (let physical = APP_START; physical < FLASH_SIZE; physical += SECTOR_SIZE) {
    const sector = physical < whole.length ? whole.subarray(physical, physical + SECTOR_SIZE) : new Uint8Array(SECTOR_SIZE).fill(0xff);
    out.push(crc16(sector));
  }
  return out;
};

test('the Doom code blob is padded to sectors and limited to the code region', () => {
  assert.equal(checkDoomCode(new Uint8Array(5000)).length, 2 * SECTOR_SIZE);
  assert.equal(checkDoomCode(new Uint8Array(5000))[4999 + 1], 0xff);
  assert.throws(() => checkDoomCode(new Uint8Array(FAR_END - FAR_START + 1)), /must be 1 to/);
  assert.throws(() => checkDoomCode(new Uint8Array()), /must be 1 to/);
});

test('pack and song headers are checked', () => {
  assert.ok(checkPack(new TextEncoder().encode('DMCDOOM\0rest')));
  assert.throws(() => checkPack(new Uint8Array(16)), /DMCDOOM/);
  assert.ok(checkSong(new TextEncoder().encode('H9MU' + ' '.repeat(40))));
  assert.throws(() => checkSong(new TextEncoder().encode('MThd' + ' '.repeat(40))), /H9MU/);
});

test('writes stay in the firmware and Doom code areas, never boot or settings', () => {
  assert.equal(inWritableArea(APP_START), true);
  assert.equal(inWritableArea(IMAGE_SIZE - SECTOR_SIZE), true);
  assert.equal(inWritableArea(FAR_START), true);
  assert.equal(inWritableArea(FAR_END - SECTOR_SIZE), true);
  assert.equal(inWritableArea(0x4000), false);          // boot region
  assert.equal(inWritableArea(0xda000), false);         // settings VM
  assert.equal(inWritableArea(0xe9000), false);
  assert.equal(inWritableArea(FAR_END), false);
  assert.equal(inWritableArea(FAR_START + 1), false);   // unaligned
});

test('the plan writes changed sectors only: Doom code first, firmware high to low', () => {
  const stock = new Uint8Array(IMAGE_SIZE).fill(0x11);
  const image = stock.slice();
  image.fill(0x22, 0x5000, 0x6000);                     // header sector
  image.fill(0x33, 0x80000, 0x81000);
  const code = checkDoomCode(new Uint8Array(2 * SECTOR_SIZE + 10).fill(0x44));
  const whole = doomTarget(image, code);
  const radio = doomTarget(stock, new Uint8Array(0));   // stock, Doom code area erased
  const plan = planInstall(crcsOf(radio), whole, code.length);
  assert.deepEqual(plan.code, [FAR_START, FAR_START + SECTOR_SIZE, FAR_START + 2 * SECTOR_SIZE]);
  assert.deepEqual(plan.firmware, [0x80000, 0x5000]);
  assert.deepEqual(plan.sectors.slice(0, 3), plan.code);
  // a radio that already holds the build needs nothing
  assert.equal(planInstall(crcsOf(whole), whole, code.length).sectors.length, 0);
});
