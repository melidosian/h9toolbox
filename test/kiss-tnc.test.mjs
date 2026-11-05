import test from 'node:test';
import assert from 'node:assert/strict';
import { encodeKiss, KissDecoder, encodeAprs, decodeAprs } from '../prototypes/kiss-tnc/kiss.mjs';

test('decodes an independently specified KISS frame with escapes across notifications', () => {
  const decoder = new KissDecoder();
  assert.deepEqual(decoder.feed([99, 0xc0, 0x20, 0x41, 0xdb]), []);
  assert.deepEqual(decoder.feed([0xdc, 0xdb, 0xdd, 0xc0]), [{ port: 2, command: 0, payload: Uint8Array.of(0x41, 0xc0, 0xdb) }]);
  assert.deepEqual(encodeKiss(Uint8Array.of(0x41, 0xc0, 0xdb), 2), Uint8Array.of(0xc0, 0x20, 0x41, 0xdb, 0xdc, 0xdb, 0xdd, 0xc0));
});
test('handles coalesced frames, shared delimiters and command frames', () => {
  assert.deepEqual(new KissDecoder().feed([0xc0, 0xc0, 1, 10, 0xc0, 0x10, 42, 0xc0]), [
    { port: 0, command: 1, payload: Uint8Array.of(10) }, { port: 1, command: 0, payload: Uint8Array.of(42) },
  ]);
});
test('drops malformed, dangling-escape and oversized frames and resynchronizes', () => {
  const decoder = new KissDecoder(3);
  assert.deepEqual(decoder.feed([0xc0, 0, 0xdb, 1, 0xc0, 0, 0xdb, 0xc0, 0, 1, 2, 3, 0xc0, 0, 7, 0xc0]), [
    { port: 0, command: 0, payload: Uint8Array.of(7) },
  ]);
});
test('AX.25 UI encoder matches an independent address/control/PID vector without FCS', () => {
  const expected = Uint8Array.from([0x82,0xa0,0xa4,0xa6,0x40,0x40,0xe0, 0x9c,0x60,0x86,0x82,0x98,0x98,0x63, 3,0xf0,0x3e,0x48,0x69]);
  assert.deepEqual(encodeAprs('N0CALL-1>APRS:>Hi'), expected);
  assert.equal(decodeAprs(expected), 'N0CALL-1>APRS:>Hi');
});
test('preserves SSIDs and repeated digipeater bits', () => {
  const line = 'N0CALL-15>APRS,WIDE1-1*,WIDE2-2:!4903.50N/07201.75W-Test';
  assert.equal(decodeAprs(encodeAprs(line)), line);
});
test('rejects invalid addresses, paths, information, and non-UI frames', () => {
  for (const line of ['BADLONG>APRS:hi', 'N0CALL-16>APRS:hi', 'N0CALL>APRS:', 'N0CALL>APRS:hi\nthere', 'N0CALL>APRS:é', 'N0CALL>APRS,' + Array(9).fill('WIDE1').join(',') + ':hi']) assert.throws(() => encodeAprs(line));
  const frame = encodeAprs('N0CALL>APRS:>test'); frame[14] = 0;
  assert.throws(() => decodeAprs(frame), /UI frame/);
  assert.throws(() => decodeAprs(Uint8Array.of(1,2,3)));
  assert.throws(() => encodeKiss([], 16));
});

const { KissTnc } = await import('../prototypes/kiss-tnc/tnc.mjs');

test('demo emits decoded RX and simulated TX without a hardware transport', async () => {
  const tnc = new KissTnc(), received = [], sent = [];
  tnc.addEventListener('packet', e => received.push(e.detail));
  tnc.addEventListener('sent', e => sent.push(e.detail));
  tnc.connectDemo();
  assert.equal(received.length, 1);
  assert.equal(received[0].demo, true);
  assert.match(received[0].aprs, /^N0CALL-1>APRS,WIDE1-1:!/);
  await tnc.sendAprs('N0CALL>APRS:>test');
  assert.equal(sent[0].demo, true);
  await tnc.disconnect();
  await assert.rejects(tnc.sendAprs('N0CALL>APRS:>test'), /Connect/);
});

test('BLE is passive at connect, gates TX, chunks writes, reassembles notifications and cleans up', async () => {
  const previousNavigator = Object.getOwnPropertyDescriptor(globalThis, 'navigator');
  const writes = [];
  const tx = { properties: { write: true }, async writeValueWithResponse(bytes) { writes.push(bytes); } };
  const rx = new EventTarget();
  rx.startNotifications = async () => {};
  rx.stopNotifications = async () => {};
  const device = new EventTarget();
  device.name = 'Mock KISS TNC';
  device.gatt = {
    connected: false,
    async connect() { this.connected = true; return this; },
    async getPrimaryService() { return { getCharacteristic: async uuid => uuid === 'tx' ? tx : rx }; },
    disconnect() { this.connected = false; },
  };
  Object.defineProperty(globalThis, 'navigator', { configurable: true, value: { bluetooth: { requestDevice: async () => device } } });
  const tnc = new KissTnc();
  try {
    await tnc.connectBle({ service: 'svc', write: 'tx', notify: 'rx' });
    assert.equal(writes.length, 0, 'connecting must never transmit initialization commands');
    await assert.rejects(tnc.sendAprs('N0CALL>APRS:>test'), /Enable transmission/);
    const line = 'N0CALL>APRS:>a message long enough to need multiple BLE writes';
    await tnc.sendAprs(line, { allowTransmit: true });
    assert.ok(writes.length > 1);
    assert.ok(writes.every(b => b.length <= 20));
    assert.deepEqual(Uint8Array.from(writes.flatMap(b => [...b])), encodeKiss(encodeAprs(line)));
    const packets = [];
    tnc.addEventListener('packet', e => packets.push(e.detail));
    for (const chunk of writes) {
      // Exercise a DataView with a nonzero buffer offset as browser BLE can return.
      const buffer = Uint8Array.from([99, ...chunk, 99]);
      rx.value = new DataView(buffer.buffer, 1, chunk.length);
      rx.dispatchEvent(new Event('characteristicvaluechanged'));
    }
    assert.equal(packets[0].aprs, line);
    await tnc.disconnect();
    rx.dispatchEvent(new Event('characteristicvaluechanged'));
    assert.equal(packets.length, 1, 'notification listeners must be removed');
    assert.equal(device.gatt.connected, false);
  } finally {
    await tnc.disconnect();
    if (previousNavigator) Object.defineProperty(globalThis, 'navigator', previousNavigator);
    else delete globalThis.navigator;
  }
});

test('serial reads KISS, writes only explicitly authorized packets, and closes cleanly', async () => {
  const previousNavigator = Object.getOwnPropertyDescriptor(globalThis, 'navigator');
  const writes = [], packets = [];
  let input, closed = false;
  const serialPort = {
    readable: new ReadableStream({ start(controller) { input = controller; } }),
    writable: new WritableStream({ write(chunk) { writes.push(chunk); } }),
    async open(options) { assert.equal(options.baudRate, 9600); },
    async close() { assert.equal(this.readable.locked, false); assert.equal(this.writable.locked, false); closed = true; },
  };
  Object.defineProperty(globalThis, 'navigator', { configurable: true, value: { serial: { requestPort: async () => serialPort } } });
  const tnc = new KissTnc();
  try {
    tnc.addEventListener('packet', e => packets.push(e.detail));
    await tnc.connectSerial();
    assert.equal(writes.length, 0);
    const line = 'N0CALL>APRS:>serial test';
    input.enqueue(encodeKiss(encodeAprs(line)));
    await new Promise(resolve => setImmediate(resolve));
    assert.equal(packets[0].aprs, line);
    await assert.rejects(tnc.sendAprs(line), /Enable transmission/);
    await tnc.sendAprs(line, { allowTransmit: true });
    assert.deepEqual(writes[0], encodeKiss(encodeAprs(line)));
    await tnc.disconnect();
    assert.equal(closed, true);
  } finally {
    await tnc.disconnect();
    if (previousNavigator) Object.defineProperty(globalThis, 'navigator', previousNavigator);
    else delete globalThis.navigator;
  }
});
