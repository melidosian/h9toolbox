import test from 'node:test';
import assert from 'node:assert/strict';
import { readChannel, writeChannel } from '../dist/channel.mjs';
import { exportChannels, inspectChannelImport, importChannels } from '../dist/channels-file.mjs';

const off = {type:'off', value:'', polarity:'N'};
function image() {
  const bytes = new Uint8Array(0x3148).fill(0xff);
  bytes.set([0x54,0x44,0x48,0x39,0xff,0xff,0xff,0x4e]);
  return bytes;
}
function channel(n, name, rx) {
  return {n, name, rx, tx:rx, used:true, scan:true, rxTone:off, txTone:off,
    scramble:0, pttId:0, hopping:false, busyLock:false, power:1, mode:'FM', offsetDirection:0};
}

test('channels-only export imports without changing other radio bytes', () => {
  const before = writeChannel(image(), 2, channel(2, 'OLD', '146.52000'));
  before[8 + 0x3000] = 0x25;
  const exported = exportChannels(before);
  assert.equal(exported.replaceAll, false);
  assert.equal(exported.channels.length, 1);
  assert.equal(Object.hasOwn(exported, 'memoryBlocks'), false);
  assert.deepEqual(inspectChannelImport(before, exported).unchanged, [2]);
  const imported = importChannels(before, exported);
  assert.deepEqual(imported.image, before);
});

test('replaceAll clears omitted channels and resets replaced records', () => {
  let before = writeChannel(image(), 1, channel(1, 'OLD1', '146.52000'));
  before = writeChannel(before, 2, channel(2, 'OLD2', '147.00000'));
  before[8 + 0x10 + 13] |= 0x1b;
  before[8 + 0x3000] = 0x25;
  const document = {...exportChannels(before), replaceAll:true,
    channels:[channel(1, 'NEW1', '443.32500'), channel(3, 'NEW3', '145.13000')]};
  const plan = inspectChannelImport(before, document);
  assert.deepEqual(plan.conflicts, [1]);
  assert.deepEqual(plan.additions, [3]);
  assert.deepEqual(plan.removals, [2]);
  const {image:after} = importChannels(before, document);
  assert.equal(readChannel(after, 1).name, 'NEW1');
  assert.equal(readChannel(after, 2).valid, false);
  assert.equal(readChannel(after, 3).name, 'NEW3');
  assert.equal(after[8 + 0x10 + 13] & 0x1b, 0);
  assert.equal(after[8 + 0x3000], 0x25);
  assert.equal(readChannel(before, 2).name, 'OLD2');
});

test('rejects the wrong mode and duplicate channel numbers', () => {
  const before = image();
  const document = exportChannels(before);
  assert.throws(() => inspectChannelImport(before, {...document, mode:'GMRS'}), /same radio mode/);
  assert.throws(() => inspectChannelImport(before, {...document, channels:[channel(1, 'A', '146.52000'), channel(1, 'B', '147.00000')]}), /duplicate/);
});
