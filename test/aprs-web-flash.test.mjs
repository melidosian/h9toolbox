import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import { flashAprs, IMAGES, RADIO_FILTER, writeFrame } from '../prototypes/aprs-update/protocol.mjs';

const release = new URL('../firmware-research/kiss-tnc/', import.meta.url);
const image = await readFile(new URL(IMAGES.packet.filename, release));
const manifest = JSON.parse(await readFile(new URL('APRS-1.0.15-rx15-rxff.release.json', release), 'utf8'));

class FakePort {
  constructor({ failAt = -1, reply = 'bad' } = {}) {
    this.failAt = failAt;
    this.reply = reply;
    this.options = null;
    this.signals = null;
    this.closed = false;
    this.writes = [];
  }

  getInfo() { return RADIO_FILTER; }

  async open(options) {
    this.options = options;
    this.readable = new ReadableStream({ start: controller => { this.input = controller; } });
    this.writable = new WritableStream({ write: frame => {
      assert.deepEqual(this.signals, { dataTerminalReady: true, requestToSend: true });
      const index = this.writes.length;
      this.writes.push(frame.slice());
      if (index === this.failAt) {
        if (this.reply === 'silent') return;
        if (this.reply === 'disconnect') {
          this.input.close();
          return;
        }
        if (this.reply === 'extra') {
          this.input.enqueue(Uint8Array.of(0x54, 0x49, 0x44, 0, 1, 0x13, 0x14, 0xff));
          return;
        }
        this.input.enqueue(Uint8Array.of(0x54, 0x49, 0x44, 0, 1, 0x10, 0x11));
        return;
      }
      const status = frame[5] === 0xff ? 0x10 : 0x13;
      this.input.enqueue(Uint8Array.of(0x54, 0x49, 0x44));
      this.input.enqueue(Uint8Array.of(0, 1, status, status + 1));
    } });
  }

  async setSignals(signals) {
    this.signals = signals;
    this.input.enqueue(Uint8Array.of(0x00, 0xff, 0x54));
  }

  async close() {
    assert.equal(this.readable.locked, false);
    assert.equal(this.writable.locked, false);
    this.closed = true;
  }
}

test('APRS frame layout uses big-endian image offset and the vendor checksum', () => {
  assert.equal(Buffer.from(writeFrame(0x100, Uint8Array.of(0xfc, 1))).toString('hex'),
    '54494400070300000100fc0108');
});

test('the packaged APRS image transfers with a startup drain and fragmented acknowledgments', async () => {
  assert.equal(image.length, manifest.image_bytes);
  assert.equal(IMAGES.packet.sha256, manifest.image_sha256);
  assert.equal(IMAGES.stock.sha256, manifest.base_image_sha256);
  const port = new FakePort();
  const events = [];
  await flashAprs(port, image, 'packet', { onEvent: row => events.push(row) });
  assert.deepEqual(port.options, {
    baudRate: 38400, dataBits: 8, stopBits: 1, parity: 'none', flowControl: 'none', bufferSize: 65536,
  });
  assert.equal(port.writes.length, 137);
  for (let index = 0; index < 136; index++) {
    const start = index * 128;
    const frame = Buffer.from(port.writes[index]);
    const payload = image.subarray(start, start + 128);
    assert.equal(frame.readUInt32BE(6), start);
    assert.equal(frame.length, payload.length + 11);
    assert.deepEqual(frame.subarray(10, -1), payload);
  }
  assert.equal(Buffer.from(port.writes.at(-1)).toString('hex'), '5449440001ff00');
  assert.deepEqual(events[1], { event: 'startup_drained', discarded_bytes: 3, quiet_ms: 20 });
  assert.deepEqual(events.at(-1), {
    event: 'exit_acknowledged', acknowledged_end: image.length, reply_hex: '54 49 44 00 01 10 11',
  });
  assert.equal(port.closed, true);
});

test('a wrong image or USB device prevents the port from opening', async () => {
  const port = new FakePort();
  const corrupt = Uint8Array.from(image);
  corrupt[64] ^= 1;
  await assert.rejects(flashAprs(port, corrupt, 'packet'), /verified release file/);
  await assert.rejects(flashAprs(port, image, 'stock'), /verified release file/);
  port.getInfo = () => ({ usbVendorId: 0x1234, usbProductId: 0x0001 });
  await assert.rejects(flashAprs(port, image, 'packet'), /TD-H9 USB serial port/);
  assert.equal(port.options, null);
});

for (const [reply, message] of [
  ['bad', /Unexpected APRS acknowledgment/],
  ['extra', /Unexpected extra bytes/],
  ['silent', /Timed out waiting for an APRS acknowledgment/],
  ['disconnect', /radio closed its USB serial connection/],
]) {
  test(`a ${reply} reply stops the write at the last acknowledged block`, async () => {
    const port = new FakePort({ failAt: 1, reply });
    const events = [];
    await assert.rejects(flashAprs(port, image, 'packet', {
      timeoutMs: 120, onEvent: row => events.push(row),
    }), message);
    assert.equal(port.writes.length, 2);
    assert.equal(events.at(-1).event, 'attempt_failed');
    assert.equal(events.at(-1).acknowledged_end, 128);
    assert.equal(port.closed, true);
  });
}
