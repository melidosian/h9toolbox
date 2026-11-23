import assert from 'node:assert/strict';
import test from 'node:test';
import { CAPTURE_ADDRESS, captureCsv, readCapture } from './read-capture.mjs';

function fixture({ status = 2, preHead = 3, preCount = 16, count = 3 } = {}) {
  const bytes = new Uint8Array(1632);
  bytes.set([0x48, 0x39, 0x53, 0x57, 1, status, count, preHead, preCount, 16]);
  const view = new DataView(bytes.buffer);
  const trigger = 2;
  function record(index, fromStart, txFlag) {
    const offset = 32 + index * 16;
    view.setUint32(offset, trigger + fromStart * 4, true);
    [index + 100, 222, 400, 0xffff, 0].forEach((value, adc) => {
      view.setUint16(offset + 4 + adc * 2, value, true);
    });
    view.setUint8(offset + 14, txFlag);
    view.setUint8(offset + 15, index % 5);
  }
  for (let index = 0; index < preCount; index++) {
    record((preHead - preCount + 16 + index) % 16, index - preCount, 0);
  }
  for (let index = 0; index < count; index++) record(index + 16, index, index < 2 ? 1 : 0);
  return bytes;
}

function radioFor(bytes, amend = block => block) {
  const reads = [];
  const calls = [];
  return {
    reads,
    calls,
    async enterProgramming() { calls.push('enter'); },
    async readBlock(address) {
      assert.equal((address - CAPTURE_ADDRESS) % 32, 0);
      assert.ok(address >= CAPTURE_ADDRESS && address + 32 <= CAPTURE_ADDRESS + 1632);
      reads.push(address);
      const offset = address - CAPTURE_ADDRESS;
      return amend(bytes.slice(offset, offset + 32), reads);
    },
    async leaveProgramming() { calls.push('leave'); },
  };
}

test('reads a frozen capture in chronological order across a tick wrap', async () => {
  const radio = radioFor(fixture());
  const { rows } = await readCapture(radio);
  assert.equal(rows.length, 19);
  assert.deepEqual(rows.map(row => row.relativeMs), Array.from({ length: 19 }, (_, i) => (i - 16) * 40));
  assert.equal(rows[0].adc[0], 103);
  assert.equal(rows[15].adc[0], 102);
  assert.deepEqual(rows[16], { tick: 2, relativeMs: 0, adc: [116, 222, 400, null, 0], txFlag: 1, cursor: 1 });
  assert.match(captureCsv(rows), /^relative_ms,tick_100hz,tx_flag,adc_cursor,adc_0,adc_3,adc_4,adc_10,adc_13\r\n/);
  assert.match(captureCsv(rows), /\r\n0,2,1,1,116,222,400,,0\r\n/);
  assert.deepEqual(radio.calls, ['enter', 'leave']);
  assert.equal(radio.reads.length, 12);
  assert.equal(radio.reads.at(-1), CAPTURE_ADDRESS);
});

test('a short pretrigger exports only initialized rows', async () => {
  const { rows } = await readCapture(radioFor(fixture({ preHead: 2, preCount: 2 })));
  assert.deepEqual(rows.map(row => row.relativeMs), [-80, -40, 0, 40, 80]);
  assert.equal(rows[0].adc[0], 100);
});

test('arming and recording states require only the first block', async () => {
  for (const status of [0, 1]) {
    const radio = radioFor(fixture({ status, count: status }), block => {
      // The live pretrigger cursor advanced before the count was read.
      block[7] = 3;
      block[8] = 2;
      return block;
    });
    const result = await readCapture(radio);
    assert.equal(result.status, status);
    assert.deepEqual(result.rows, []);
    assert.deepEqual(radio.reads, [CAPTURE_ADDRESS]);
    assert.deepEqual(radio.calls, ['enter', 'leave']);
  }
});

test('a full capture stays inside the export window', async () => {
  const radio = radioFor(fixture({ status: 3, count: 84 }));
  const { status, rows } = await readCapture(radio);
  assert.equal(status, 3);
  assert.equal(rows.length, 100);
  assert.equal(radio.reads.length, 52); // Includes a second header read.
  assert.equal(Math.max(...radio.reads), 0x4240);
});

test('rejects incompatible, changed and corrupt replies and leaves programming mode', async () => {
  for (const [mutate, message] of [
    [(block, reads) => { if (reads.length === 1) block[0] = 0; return block; }, /bench header/],
    [(block, reads) => { if (reads.length === 1) block[6] = 85; return block; }, /Invalid/],
    [(block, reads) => { if (reads.length === 2) return block.subarray(0, 13); return block; }, /Incomplete/],
    [(block, reads) => { if (reads.length === 12) block[5] = 0; return block; }, /changed/],
    [(block, reads) => { if (reads.at(-1) === 0x3d20) block[14] = 0; return block; }, /inconsistent/],
  ]) {
    const radio = radioFor(fixture(), mutate);
    await assert.rejects(readCapture(radio), message);
    assert.deepEqual(radio.calls, ['enter', 'leave']);
  }
});
