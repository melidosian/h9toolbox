import assert from 'node:assert/strict';
import test from 'node:test';
import { buildBundle } from '../dist/apps/bundle.mjs';
import { connectUsb } from '../dist/boot-logo/radio.mjs';
import { BANK_SIZE, BLOCK, LOGO_ADDRESS, LOGO_BYTES, MissingResourceServiceError, SECTOR } from '../dist/boot-logo/resource.mjs?v=20261127status';
import { SLOTS, readPack, restore, uploadPack } from '../dist/doom/upload.mjs';
import { SimulatedRadio } from './support/simulated-radio.mjs';

function usbRadio(t, { service = true, dataReadsPerSession = Infinity } = {}) {
  let incoming;
  let dataReads = 0;
  const sim = new SimulatedRadio({ service, mtu: 7, deliver: bytes => incoming.enqueue(bytes) });
  const port = {
    async open(options) {
      assert.equal(options.baudRate, 115200);
      this.readable = new ReadableStream({ start: controller => { incoming = controller; } });
      this.writable = new WritableStream({ write: bytes => {
        if (bytes.length === 7 && bytes[0] === 0x50) dataReads = 0;
        if (bytes[0] === 0x52) assert.ok(bytes[1] < 0x3c || bytes[1] >= 0x40, 'USB skips Bluetooth bulk and link commands');
        if (bytes[0] === 0x52 && bytes[1] >= 0x80 && ++dataReads > dataReadsPerSession) return;
        return sim.handle(bytes);
      } });
    },
    async close() {
      assert.equal(this.readable.locked, false);
      assert.equal(this.writable.locked, false);
      this.closed = true;
    },
  };
  t.mock.getter(globalThis, 'navigator', () => ({ serial: { getPorts: async () => [], requestPort: async () => port } }));
  return { sim, port, unplug: () => incoming.error(new Error('USB unplugged')) };
}

test('App Manager installs, reads, and removes a bundle over USB serial', async t => {
  const { sim, port } = usbRadio(t);
  const session = await connectUsb();
  t.after(() => session.disconnect());
  const { radio } = session;
  assert.equal((await radio.checkService()).version, 2);
  assert.equal((await readPack(radio, SLOTS.modules)).pack, null);

  const bundle = buildBundle([{ name: 'MENU', body: Uint8Array.of(1, 2, 3, 4), bss: 0, entry: 0 }]);
  const manifest = await uploadPack(radio, bundle, { slot: SLOTS.modules, name: 'H9 modules' });
  assert.equal(manifest.verified, true);
  assert.deepEqual((await readPack(radio, SLOTS.modules)).pack, bundle);
  await restore(radio, { slot: SLOTS.modules });
  assert.equal((await readPack(radio, SLOTS.modules)).pack, null);
  assert.deepEqual(sim.linkRequests, []);
  await session.disconnect();
  assert.equal(port.closed, true);
});

test('the shared boot-logo tool writes and verifies over USB serial', async t => {
  const { sim } = usbRadio(t);
  const session = await connectUsb();
  t.after(() => session.disconnect());
  const { radio } = session;
  const old = new Uint8Array(LOGO_BYTES).fill(0xaa);
  sim.chip.set(old, LOGO_ADDRESS);
  const next = old.slice();
  next.fill(0xbb, SECTOR, 2 * SECTOR);
  const current = await radio.readRange(LOGO_ADDRESS, LOGO_BYTES);
  assert.deepEqual(current.bytes, old);
  const result = await radio.writeLogo(current.bytes, next);
  assert.equal(result.sectors, 1);
  assert.deepEqual(result.bytes, next);
  assert.deepEqual(sim.linkRequests, []);
});

test('USB reads resume at the missing block even when each session drops later replies', async t => {
  const { sim } = usbRadio(t, { dataReadsPerSession: 3 });
  const warnings = [];
  const session = await connectUsb({ log: (message, level) => { if (level === 'warn') warnings.push(message); } });
  t.after(() => session.disconnect());
  const { radio } = session;
  const readExact = radio.readExact.bind(radio);
  t.mock.method(radio, 'readExact', (length, timeout = 100) => readExact(length, timeout));
  const start = LOGO_ADDRESS + BANK_SIZE - 5 * BLOCK;
  const bytes = Uint8Array.from({ length: 10 * BLOCK }, (_, i) => (i * 7 + (i >> 5)) & 255);
  sim.chip.set(bytes, start);

  assert.deepEqual((await radio.readRange(start, bytes.length)).bytes, bytes);
  assert.deepEqual(warnings.map(message => message.split(':')[0]), [3, 6, 9].map(
    block => `Retry 1 reading flash at 0x${(start + block * BLOCK).toString(16)}`));
});

test('USB transfers keep complete clone frames together at the browser buffer boundary', async t => {
  let rotations = 0;
  let written = 0;
  const port = {
    getInfo: () => ({ usbVendorId: 0x4c4a, usbProductId: 0x4155 }),
    async open({ bufferSize }) {
      this.readable = new ReadableStream();
      const newWriter = () => {
        let offset = 0;
        return new WritableStream({
          write(bytes) {
            if (offset + bytes.length > bufferSize) throw new Error('The browser would split this frame.');
            offset += bytes.length;
            written += bytes.length;
          },
          close() { rotations++; port.writable = newWriter(); },
        });
      };
      this.writable = newWriter();
    },
    async close() {},
  };
  t.mock.getter(globalThis, 'navigator', () => ({ serial: {
    getPorts: async () => [port],
    requestPort: async () => assert.fail('The known USB port already has permission.'),
  } }));
  const session = await connectUsb();
  t.after(() => session.disconnect());
  const frame = new Uint8Array(37);
  for (let i = 0; i < 2000; i++) await session.radio.send(frame);
  assert.equal(rotations, 1);
  assert.equal(written, 2000 * frame.length);
});

test('a USB port closes on unplug and stock firmware fails the service check', async t => {
  const { port, unplug } = usbRadio(t, { service: false });
  const disconnected = Promise.withResolvers();
  const session = await connectUsb({ onDisconnect: disconnected.resolve });
  t.after(() => session.disconnect());
  await assert.rejects(session.radio.checkService(), { constructor: MissingResourceServiceError, message: /No resource service.*ff ff ff ff/ });
  unplug();
  await disconnected.promise;
  assert.equal(port.closed, true);
  await assert.rejects(session.radio.enterProgramming(), /USB-C radio disconnected/);
});
