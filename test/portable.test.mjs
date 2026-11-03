import test from 'node:test';
import assert from 'node:assert/strict';
import { exportPortable, importPortable } from '../dist/portable.mjs';
import { readSetting, SETTINGS } from '../dist/settings.mjs';
import { readChannel, writeChannel } from '../dist/channel.mjs';

const setting = id => SETTINGS.find(s => s.id === id);
function image() {
  const bytes = new Uint8Array(0x3148).fill(0xff);
  bytes.set([0x54,0x44,0x48,0x39,0xff,0xff,0xff,0x4e]);
  bytes[0x0cb1] = 3;
  bytes[0x306e] = 1;
  bytes[0x302a] = 0x25;
  return bytes;
}

test('configuration JSON omits raw blocks and imports onto a loaded image', () => {
  const original = image();
  original[0x311a] = 0x43; // undocumented byte
  const json = JSON.parse(JSON.stringify(exportPortable(original), null, 2));
  const result = importPortable(json, original);
  assert.equal(json.formatVersion, 2);
  assert.equal(json.kind, 'configuration');
  assert.equal(Object.hasOwn(json, 'memoryBlocks'), false);
  assert.equal(Object.hasOwn(json, 'identityHex'), false);
  assert.deepEqual(result.original, original);
  assert.deepEqual(result.edited, original);
  assert.equal(json.settings['SMS & Morse'].morseTone, '700 Hz');
  assert.equal(json.settings['SMS & Morse'].morseKey, 'Uninitialized');
  assert.throws(() => importPortable(json), /Read the destination radio/);
});

test('lossless archive JSON round-trips every original byte', () => {
  const original = image();
  original[0x311a] = 0x43;
  const json = exportPortable(original, original, {archive:true});
  const result = importPortable(json);
  assert.equal(json.formatVersion, 1);
  assert.equal(json.memoryBlocks.length, 394);
  assert.deepEqual(result.original, original);
  assert.deepEqual(result.edited, original);
});

test('readable JSON setting edits overlay the original image', () => {
  const original = image();
  const json = exportPortable(original);
  json.settings.Radio.squelch = '5';
  json.settings['APRS beacon'].beaconInterval = 90;
  const result = importPortable(json, original);
  assert.deepEqual(result.original, original);
  assert.equal(readSetting(result.edited, setting('squelch')), '5');
  assert.equal(readSetting(result.edited, setting('beaconInterval')), '90');
  assert.equal(result.edited[0x302a], 0x25);
});

test('unknown source settings do not overwrite known destination values', () => {
  const source = image();
  source[0x1f31] = 0xff;
  const destination = image();
  destination[0x1f31] = 1;
  const json = exportPortable(source);
  assert.equal(json.settings['Display & buttons'].display, 'Unknown (255)');
  assert.equal(importPortable(json, destination).edited[0x1f31], 1);
});

test('channel edits and explicit clears survive JSON export/import', () => {
  const original = image();
  const form = {
    name:'CALL', rx:'146.52000', tx:'146.52000', mode:'FM', power:2,
    offsetDirection:0, pttId:0, scramble:0, used:true, scan:true,
    busyLock:false, hopping:false,
    rxTone:{type:'off',value:'',polarity:'N'}, txTone:{type:'off',value:'',polarity:'N'},
  };
  const populated = writeChannel(original, 1, form);
  const edited = writeChannel(populated, 1, {...form, name:'CALL2'});
  const json = exportPortable(populated, edited);
  assert.equal(readChannel(importPortable(json, populated).edited, 1).name, 'CALL2');
  json.channels = [{n:1,clear:true}];
  const cleared = importPortable(json, populated);
  assert.equal(readChannel(cleared.edited, 1).valid, false);
  assert.deepEqual(cleared.original, populated);
});

test('malformed or incomplete JSON is rejected', () => {
  const json = exportPortable(image());
  const archive = exportPortable(image(),image(),{archive:true});
  assert.throws(() => importPortable({...json,formatVersion:3},image()), /Unsupported/);
  assert.throws(() => importPortable({...archive,memoryBlocks:[]}), /every 32-byte/);
  assert.throws(() => importPortable({...archive,memoryBlocks:[...archive.memoryBlocks.slice(0,1),'zz'.repeat(32),...archive.memoryBlocks.slice(2)]}), /hex/);
});
