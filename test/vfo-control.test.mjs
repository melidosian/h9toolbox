import test from 'node:test';
import assert from 'node:assert/strict';
import { analyzeVolumeSnapshots, changedBytes, compareVfoSnapshots, vfoWriteBlock, bChannelWriteBlock, volumeWriteBlock } from '../prototypes/vfo-control/core.mjs';

test('stable A/B byte survives an A/B/A comparison while drift is excluded', () => {
  const result = compareVfoSnapshots(
    Uint8Array.of(1, 10, 30),
    Uint8Array.of(1, 11, 31),
    Uint8Array.of(1, 10, 32),
  );
  assert.deepEqual(result.stable, [{ offset: 1, a: 10, b: 11 }]);
  assert.deepEqual(result.unstable, [2]);
});

test('comparison rejects truncated snapshots', () => {
  assert.throws(() => compareVfoSnapshots(Uint8Array.of(1), Uint8Array.of(1, 2), Uint8Array.of(1)), /different lengths/);
  assert.throws(() => changedBytes(Uint8Array.of(1), Uint8Array.of(1, 2)), /different lengths/);
});

test('VFO write frame changes only selector and has the required checksum', () => {
  const current = Uint8Array.from({ length: 32 }, (_, index) => index === 4 ? 0 : index);
  const frame = vfoWriteBlock(current, 1);
  assert.deepEqual([...frame.slice(0, 4)], [0x57, 0x30, 0x00, 0x20]);
  assert.equal(frame.length, 37);
  assert.equal(frame[8], 1);
  assert.equal(frame[36], frame.slice(4, 36).reduce((sum, byte) => (sum + byte) & 255, 0));
  assert.equal(current[4], 0);
  assert.deepEqual([...frame.slice(4, 36)].filter((byte, index) => byte !== current[index]), [1]);
  assert.throws(() => vfoWriteBlock(current, 2), /A=0 or B=1/);
});

test('B channel write frame changes only the channel byte', () => {
  const current = Uint8Array.from({ length: 32 }, (_, index) => index === 5 ? 27 : index);
  const frame = bChannelWriteBlock(current, 26);
  assert.deepEqual([...frame.slice(0, 4)], [0x57, 0x0c, 0xa0, 0x20]);
  assert.equal(frame[9], 26);
  assert.deepEqual(changedBytes(current, frame.slice(4, 36)), [{ offset: 5, from: 27, to: 26 }]);
  assert.equal(frame[36], frame.slice(4, 36).reduce((sum, byte) => (sum + byte) & 255, 0));
  assert.throws(() => bChannelWriteBlock(current, 0), /1–199/);
});

test('RX volume candidate requires an isolated reversible 0–30 step', () => {
  const low = Uint8Array.of(5, 10, 9);
  const high = Uint8Array.of(5, 11, 9);
  assert.deepEqual(analyzeVolumeSnapshots(low, high, low).candidate, { offset: 1, low: 10, high: 11 });
  assert.equal(analyzeVolumeSnapshots(low, Uint8Array.of(6, 11, 9), low).candidate, null);
  assert.equal(analyzeVolumeSnapshots(low, high, Uint8Array.of(5, 10, 8)).candidate, null);
  assert.equal(analyzeVolumeSnapshots(low, Uint8Array.of(5, 12, 9), low).candidate, null);
  assert.equal(analyzeVolumeSnapshots(Uint8Array.of(30), Uint8Array.of(31), Uint8Array.of(30)).candidate, null);
  const nearLow = analyzeVolumeSnapshots(Uint8Array.of(5, 9, 7), Uint8Array.of(5, 11, 7), Uint8Array.of(5, 10, 7));
  assert.deepEqual(nearLow.volumeField, { offset: 1, firstLow: 9, high: 11, returnedLow: 10 });
  assert.deepEqual(nearLow.candidate, { offset: 1, low: 10, high: 11 });
  const unchanged = analyzeVolumeSnapshots(low, low, low);
  assert.equal(unchanged.changes.length, 0);
  assert.equal(unchanged.candidate, null);
  assert.equal(analyzeVolumeSnapshots(Uint8Array.of(5, 9, 7), Uint8Array.of(5, 11, 7), Uint8Array.of(6, 10, 7)).candidate, null);
});

test('RX volume frame writes only the selected byte with a matching checksum', () => {
  const current = Uint8Array.from({ length: 32 }, (_, index) => index === 7 ? 10 : 0);
  const frame = volumeWriteBlock(current, 0x1f20, 7, 11);
  assert.deepEqual([...frame.slice(0, 4)], [0x57, 0x1f, 0x20, 0x20]);
  assert.deepEqual(changedBytes(current, frame.slice(4, 36)), [{ offset: 7, from: 10, to: 11 }]);
  assert.equal(frame[36], frame.slice(4, 36).reduce((sum, byte) => (sum + byte) & 255, 0));
  assert.throws(() => volumeWriteBlock(current, 0x1f21, 7, 11), /aligned/);
  assert.throws(() => volumeWriteBlock(current, 0x1f20, 7, 31), /0–30/);
});
