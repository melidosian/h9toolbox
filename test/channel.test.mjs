import test from 'node:test';
import assert from 'node:assert/strict';
import { readChannel, writeChannel, clearChannel } from '../dist/channel.mjs';

function image() {
  const bytes = new Uint8Array(0x3148).fill(0xff);
  bytes.set([0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff, 0x4e]);
  return bytes;
}
function form(channel, overrides = {}) {
  return {
    name: channel.name, rx: channel.rx, tx: channel.tx || '',
    offsetDirection: channel.offsetDirection, mode: channel.mode,
    power: channel.power, pttId: channel.pttId,
    scramble: channel.scramble, used: channel.used, scan: channel.scan,
    busyLock: channel.busyLock, hopping: channel.hopping,
    rxTone: channel.rxTone, txTone: channel.txTone, ...overrides,
  };
}

test('decodes TD-H9 channel frequencies and CTCSS/DCS tones', () => {
  const bytes = image();
  bytes.set([0x00,0x20,0x65,0x14, 0x00,0x20,0x65,0x14, 0x25,0x08, 0x45,0x82, 0,0,0x20,0], 8 + 0x10);
  bytes.set([0x32,0x4d,0x2d,0x43,0x41,0x4c,0x4c,0], 8 + 0x0d40);
  bytes[8 + 0x1900] = 1;
  bytes[8 + 0x1920] = 1;
  const channel = readChannel(bytes, 1);
  assert.equal(channel.name, '2M-CALL');
  assert.equal(channel.rx, '146.52000');
  assert.deepEqual(channel.rxTone, { type: 'ctcss', value: '82.5', polarity: 'N' });
  assert.deepEqual(channel.txTone, { type: 'dcs', value: '245', polarity: 'N' });
  assert.equal(channel.power, 2);
  assert.equal(channel.scan, true);
});

test('opening and applying an unchanged channel preserves every byte', () => {
  const bytes = image();
  bytes.set([0x00,0x20,0x65,0x14, 0x00,0x20,0x65,0x14, 0xff,0xff, 0xff,0xff, 0,0x1b,0xe4,0xfe], 8 + 0x10);
  bytes.set([65,66,67,0,0,0,0,0], 8 + 0x0d40);
  bytes[8 + 0x1900] = 1;
  bytes[8 + 0x1920] = 1;
  const channel = readChannel(bytes, 1);
  const next = writeChannel(bytes, 1, form(channel));
  assert.deepEqual(next, bytes);
});

test('edits all documented options while preserving unknown bits', () => {
  const bytes = image();
  bytes.set([0x00,0x20,0x65,0x14, 0x00,0x20,0x65,0x14, 0xff,0xff, 0xff,0xff, 0,0x1b,0xc4,0xfe], 8 + 0x10);
  const channel = readChannel(bytes, 1);
  const next = writeChannel(bytes, 1, form(channel, {
    name: 'TEST', rx: '146.55000', tx: '146.55000',
    offsetDirection: 2, mode: 'NAM', power: 1,
    rxTone: { type: 'ctcss', value: '100.0', polarity: 'N' },
    txTone: { type: 'dcs', value: '023', polarity: 'R' },
    pttId: 3, busyLock: true, hopping: true, scramble: 5,
    scan: true, used: true,
  }));
  const edited = readChannel(next, 1);
  assert.equal(edited.name, 'TEST');
  assert.equal(edited.rx, '146.55000');
  assert.equal(edited.mode, 'NAM');
  assert.equal(edited.power, 1);
  assert.deepEqual(edited.rxTone, { type: 'ctcss', value: '100.0', polarity: 'N' });
  assert.deepEqual(edited.txTone, { type: 'dcs', value: '023', polarity: 'R' });
  assert.equal(edited.pttId, 3);
  assert.equal(edited.hopping, true);
  assert.equal(edited.busyLock, true);
  assert.equal(edited.scramble, 5);
  assert.equal(edited.offsetDirection, 2);
  assert.equal(next[8 + 0x10 + 13] & 0x1b, bytes[8 + 0x10 + 13] & 0x1b);
  assert.equal(next[8 + 0x10 + 14] & 0xc4, bytes[8 + 0x10 + 14] & 0xc4);
  assert.equal(next[8 + 0x10 + 15] & 0xfe, bytes[8 + 0x10 + 15] & 0xfe);
  assert.deepEqual(next.slice(8 + 0x20, 8 + 0x30), bytes.slice(8 + 0x20, 8 + 0x30));
});

test('adds and clears an empty channel with its list and scan flags', () => {
  const bytes = image();
  const blank = readChannel(bytes, 33);
  assert.equal(blank.valid, false);
  const next = writeChannel(bytes, 33, form(blank, {
    name: 'LOCAL', rx: '146.52000', tx: '', mode: 'FM', power: 2,
    offsetDirection: 0, pttId: 0, scramble: 0,
    used: true, scan: true, busyLock: false, hopping: false,
    rxTone: { type: 'off', value: '', polarity: 'N' },
    txTone: { type: 'off', value: '', polarity: 'N' },
  }));
  assert.equal(readChannel(next, 33).rx, '146.52000');
  assert.equal(readChannel(next, 33).tx, null);
  assert.equal(readChannel(next, 33).used, true);
  assert.equal(readChannel(next, 33).scan, true);
  assert.deepEqual(next.slice(8 + 33 * 16 + 8, 8 + 33 * 16 + 12), Uint8Array.of(255, 255, 255, 255));
  const cleared = clearChannel(next, 33);
  assert.equal(readChannel(cleared, 33).valid, false);
  assert.equal(readChannel(cleared, 33).used, false);
  assert.equal(readChannel(cleared, 33).scan, false);
});
