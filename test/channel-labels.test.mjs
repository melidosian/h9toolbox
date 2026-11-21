import assert from 'node:assert/strict';
import test from 'node:test';
import { clearChannel, writeChannel } from '../dist/channel.mjs';
import { checkLongName, longNameChanges, readCloneCapabilities, readLongName, supportsLongNames, writeLongName } from '../dist/channel-labels.mjs';

const full = ' 123456789012345678901234567890 ';
const off = { type: 'off', value: '', polarity: 'N' };
const emptyImage = () => new Uint8Array(0x3148).fill(0xff);
const channel = { name: '8CHARS!!', rx: '146.52000', tx: '', used: true, scan: false,
  rxTone: off, txTone: off, scramble: 0, pttId: 0, hopping: false, busyLock: false,
  power: 1, mode: 'FM', offsetDirection: 0 };

function cloneRadio({ legacy = false, serialFlags = 0, result = 0, writeResult = 0, badReadback = false } = {}) {
  const magic = [0x48, 0x39, 0x4c, 0x4e, 1];
  let selected = 0;
  const records = new Map();
  const calls = [];
  const radio = {
    async readBlock(address) {
      calls.push(['read', address]);
      if (address === 0x3f00) {
        const status = new Uint8Array(32);
        if (!legacy) status.set([...magic, result, selected, serialFlags]);
        return status;
      }
      assert.equal(address, 0x3f40);
      assert.ok(selected >= 1 && selected <= 199);
      return (records.get(selected) ?? new Uint8Array(32)).slice();
    },
    async writeBlock(address, bytes) {
      calls.push(['write', address, bytes]);
      assert.equal(bytes.length, 32);
      if (address === 0x3f20) {
        assert.deepEqual([...bytes.slice(0, 5)], magic);
        assert.ok(bytes.slice(6).every(byte => byte === 0));
        selected = bytes[5];
        result = 0;
      } else {
        assert.equal(address, 0x3f40);
        assert.ok(selected);
        result = writeResult;
        if (!badReadback) records.set(selected, bytes.slice());
      }
    },
  };
  return { radio, calls, records };
}

test('old firmware receives only the read-only long-name capability probe', async () => {
  const {radio, calls} = cloneRadio({legacy: true});
  assert.equal(await supportsLongNames(radio), false);
  assert.deepEqual(calls, [['read', 0x3f00]]);
});

test('USB-C writes require the sector-aware clone capability', async () => {
  for (const [options, expected] of [
    [{legacy: true}, {longNames: false, sparseSerialWrites: false}],
    [{serialFlags: 0}, {longNames: true, sparseSerialWrites: false}],
    [{serialFlags: 1}, {longNames: true, sparseSerialWrites: true}],
  ]) {
    const {radio, calls} = cloneRadio(options);
    assert.deepEqual(await readCloneCapabilities(radio), expected);
    assert.deepEqual(calls, [['read', 0x3f00]]);
  }
});

test('long-name clone upload selects channel, reads status, verifies all 32 bytes and clears', async () => {
  assert.equal(full.length, 32);
  const {radio, calls, records} = cloneRadio({result: 4});
  assert.equal(await supportsLongNames(radio), true);
  assert.equal(await readLongName(radio, 199), '');
  await writeLongName(radio, 199, full);
  assert.equal(await readLongName(radio, 199), full);
  assert.deepEqual([...records.get(199)], Array.from(full, c => c.charCodeAt(0)));
  await writeLongName(radio, 1, 'NEXT');
  assert.deepEqual(records.get(1).slice(4), new Uint8Array(28));
  await writeLongName(radio, 199, '');
  assert.deepEqual(records.get(199), new Uint8Array(32));
  assert.equal(await readLongName(radio, 1), 'NEXT');
  assert.equal(calls.find(call => call[0] === 'write' && call[1] === 0x3f20)[2][5], 199);
});

test('long-name wire operations report firmware and readback failures', async () => {
  await assert.rejects(writeLongName(cloneRadio({writeResult: 5}).radio, 1, 'Test'), /status 5/);
  await assert.rejects(writeLongName(cloneRadio({badReadback: true}).radio, 1, 'Test'), /readback differs/);
  await assert.rejects(readLongName(cloneRadio().radio, 200), /1–199/);
  assert.throws(() => checkLongName('A'.repeat(33)), /32 printable/);
  assert.throws(() => checkLongName('NO♥'), /printable ASCII/);
  assert.throws(() => checkLongName('RADIO\n'), /printable ASCII/);
});

test('channel deletion clears old long names with a known backup and probes for unknown labels', () => {
  const before = writeChannel(emptyImage(), 8, channel);
  const after = clearChannel(before, 8);
  assert.deepEqual(longNameChanges(new Map([[8, 'Keep this']]), new Map([[8, 'Keep this']]), before, after),
    [{channel: 8, previous: 'Keep this', next: '', implicit: false}]);
  assert.deepEqual(longNameChanges(null, new Map(), before, after),
    [{channel: 8, previous: '', next: '', implicit: true}]);
  assert.deepEqual(longNameChanges(new Map([[8, '']]), new Map(), before, after), []);
});
