import test from 'node:test';
import assert from 'node:assert/strict';
import { SETTINGS, readSetting, writeSetting } from '../dist/settings.mjs';

const find = id => SETTINGS.find(s => s.id === id);
const image = () => new Uint8Array(0x3148).fill(0xff);

test('decodes known TD-H9 radio and APRS settings', () => {
  const bytes = image();
  bytes[0x0ca9] = 0x44;
  bytes[0x0cb1] = 3;
  bytes[0x306e] = 1;
  bytes.set([78,48,67,65,76,76], 0x3085);
  bytes[0x30f8] = 60; bytes[0x30f9] = 0;
  assert.equal(readSetting(bytes, find('scanMode')), '1');
  assert.equal(readSetting(bytes, find('beep')), '1');
  assert.equal(readSetting(bytes, find('squelch')), '3');
  assert.equal(readSetting(bytes, find('gnssOn')), '1');
  assert.equal(readSetting(bytes, find('call')), 'N0CALL');
  assert.equal(readSetting(bytes, find('beaconInterval')), '60');
});

test('setting edits change only mapped bytes and preserve neighboring bit fields', () => {
  const original = image();
  original[0x0ca9] = 0x44;
  const changed = writeSetting(original, find('beep'), '0');
  assert.equal(changed[0x0ca9], 0x40);
  assert.equal(readSetting(changed, find('scanMode')), '1');
  assert.equal(original[0x0ca9], 0x44);
  assert.deepEqual(changed.slice(0x0ca8, 0x0ca9), original.slice(0x0ca8, 0x0ca9));
});

test('unchanged text retains original padding and unknown values', () => {
  const original = image();
  original.set([65,66,0,0xff,0xff,0xff], 0x3085);
  assert.deepEqual(writeSetting(original, find('call'), readSetting(original, find('call'))), original);
  assert.deepEqual(writeSetting(original, find('symbolTable'), readSetting(original, find('symbolTable'))), original);
});

test('uninitialized Morse bytes are preserved and editing them is blocked', () => {
  const original = image();
  assert.equal(readSetting(original, find('morseOn')), '0');
  assert.deepEqual(writeSetting(original, find('morseOn'), '0'), original);
  const initialized = writeSetting(original, find('morseOn'), '1');
  assert.equal(initialized[0x302a], 0x25);
  assert.equal(initialized[0x302b], 0xff);
  original[0x302a] = 0x25;
  assert.equal(readSetting(original, find('morseOn')), '1');
  assert.equal(readSetting(original, find('morseSpeed')), '2');
  assert.equal(readSetting(original, find('morseTone')), '2');
  assert.equal(readSetting(original, find('morseKey')), 'Uninitialized');
  assert.throws(() => writeSetting(original, find('morseKey'), '0'), /uninitialized/i);
});

test('APRS interval, callsign, and options validate before editing', () => {
  const original = image();
  const interval = writeSetting(original, find('beaconInterval'), '3600');
  assert.equal(interval[0x30f8], 0x10);
  assert.equal(interval[0x30f9], 0x0e);
  const call = writeSetting(original, find('call'), 'N0CALL');
  assert.equal(readSetting(call, find('call')), 'N0CALL');
  assert.throws(() => writeSetting(original, find('beaconInterval'), '0'), /expected/);
  assert.throws(() => writeSetting(original, find('call'), 'TOOLONG'), /ASCII/);
  assert.throws(() => writeSetting(original, find('ssid'), '17'), /listed/);
});
