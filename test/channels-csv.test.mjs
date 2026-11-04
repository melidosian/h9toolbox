import test from 'node:test';
import assert from 'node:assert/strict';
import { readChannel, writeChannel } from '../dist/channel.mjs';
import { exportChannels, inspectChannelImport, importChannels } from '../dist/channels-file.mjs';
import { CSV_HEADERS, serializeChannelsCsv, parseChannelsCsv } from '../dist/channels-csv.mjs';

const off = {type:'off', value:'', polarity:'N'};
function image() {
  const bytes = new Uint8Array(0x3148).fill(0xff);
  bytes.set([0x54,0x44,0x48,0x39,0xff,0xff,0xff,0x4e]);
  return bytes;
}
function channel(n, name) {
  return {n, name, rx:'146.52000', tx:'145.92000', used:true, scan:false,
    rxTone:{type:'ctcss', value:'127.3', polarity:'N'},
    txTone:{type:'dcs', value:'023', polarity:'R'}, scramble:3, pttId:2,
    hopping:true, busyLock:true, power:2, mode:'NFM', offsetDirection:1};
}

test('CSV round-trips every editable channel field and quoted names', () => {
  const before = writeChannel(image(), 1, channel(1, 'A,"B'));
  const csv = serializeChannelsCsv(exportChannels(before));
  assert.match(csv, /"A,""B"/);
  assert.equal(csv.split('\r\n')[0].split(',').length, CSV_HEADERS.length);
  const parsed = parseChannelsCsv(csv, before);
  assert.deepEqual(inspectChannelImport(before, parsed).unchanged, [1]);
  assert.deepEqual(importChannels(before, parsed).image, before);
  assert.deepEqual(parsed.channels[0], exportChannels(before).channels[0]);
});

test('CSV guards spreadsheet formulas without changing radio names', () => {
  const before = writeChannel(image(), 1, {...channel(1, '=1+2'), rxTone:off, txTone:off});
  const csv = serializeChannelsCsv(exportChannels(before));
  assert.match(csv, /"'=1\+2"/);
  const parsed = parseChannelsCsv(csv, before);
  assert.equal(parsed.channels[0].name, '=1+2');
  assert.deepEqual(importChannels(before, parsed).image, before);
});

test('CSV clear rows and replace-all remove channels only from the working image', () => {
  let before = writeChannel(image(), 1, channel(1, 'ONE'));
  before = writeChannel(before, 2, channel(2, 'TWO'));
  before[8 + 0x3000] = 0x25;
  const csv = serializeChannelsCsv({...exportChannels(before), channels:[channel(1, 'ONE')]});
  const parsed = parseChannelsCsv(csv, before);
  assert.deepEqual(inspectChannelImport(before, parsed).removals, []);
  parsed.replaceAll = true;
  assert.deepEqual(inspectChannelImport(before, parsed).removals, [2]);
  const after = importChannels(before, parsed).image;
  assert.equal(readChannel(after, 2).valid, false);
  assert.equal(after[8 + 0x3000], 0x25);
  assert.equal(readChannel(before, 2).valid, true);

  const clearCsv = serializeChannelsCsv({...exportChannels(before), channels:[{n:2, clear:true}]});
  const clear = parseChannelsCsv(clearCsv, before);
  assert.deepEqual(inspectChannelImport(before, clear).removals, [2]);
});

test('CSV rejects wrong radio modes, duplicates, and malformed rows', () => {
  const before = writeChannel(image(), 1, channel(1, 'ONE'));
  const csv = serializeChannelsCsv(exportChannels(before));
  assert.throws(() => parseChannelsCsv(csv.replace('"Normal"', '"GMRS"'), before), /radio_mode/);
  assert.throws(() => parseChannelsCsv(csv + csv.split('\r\n')[1] + '\r\n', before), /duplicate/);
  assert.throws(() => parseChannelsCsv('channel,name\r\n1,A\r\n', before), /needs these columns/);
  assert.throws(() => parseChannelsCsv(csv.replace('"146.52000"', '"oops"'), before), /RX must be/);
  assert.throws(() => parseChannelsCsv(csv + '"unfinished', before), /unclosed quoted field/);
});
