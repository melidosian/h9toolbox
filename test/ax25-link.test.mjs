import test from 'node:test';
import assert from 'node:assert/strict';
import { AX25, encodeAx25, decodeAx25 } from '../prototypes/kiss-tnc/ax25.mjs';
import { Ax25Link } from '../prototypes/kiss-tnc/ax25-link.mjs';
import { encodeKiss } from '../prototypes/kiss-tnc/kiss.mjs';

class FakeTnc extends EventTarget {
  transport = 'ble';
  sent = [];
  async sendFrame(bytes) { this.sent.push(bytes); }
  receive(bytes) { this.dispatchEvent(new CustomEvent('packet', { detail: { port: 0, command: 0, payload: bytes } })); }
}
const tick = () => new Promise(resolve => setImmediate(resolve));
const peer = (control, data = new Uint8Array(), pid = AX25.PID) =>
  encodeAx25({ source: 'GATE-10', destination: 'N0CALL', control, command: false, data, pid });

test('AX.25 control frame uses shifted addresses and omits PID', () => {
  const raw = encodeAx25({ source: 'N0CALL', destination: 'GATE-10', control: AX25.SABM | AX25.PF });
  assert.equal(raw.length, 15);
  assert.deepEqual(raw.slice(0, 7), Uint8Array.of(0x8e,0x82,0xa8,0x8a,0x40,0x40,0xf4));
  const parsed = decodeAx25(raw);
  assert.equal(parsed.kind, AX25.SABM);
  assert.equal(parsed.command, true);
  assert.equal(parsed.source, 'N0CALL');
  assert.equal(parsed.destination, 'GATE-10');
  assert.equal(parsed.poll, true);
});

test('opens, exchanges numbered data, acknowledges and closes a connected session', async () => {
  const tnc = new FakeTnc();
  const link = new Ax25Link(tnc, { local: 'N0CALL', remote: 'GATE-10', allowTransmit: true, t1: 1000, t2: 5, t3: 1000 });
  const received = [];
  link.addEventListener('data', event => received.push(event.detail.data));
  link.start(); await tick();
  assert.equal(decodeAx25(tnc.sent[0]).kind, AX25.SABM);
  tnc.receive(peer(AX25.UA | AX25.PF));
  assert.equal(link.state, 'open');
  const informationSent = new Promise(resolve => link.addEventListener('tx', event => {
    if (!(event.detail.frame.control & 1)) resolve();
  }));
  link.enqueue(Uint8Array.of(0x41, 0xc0, 0xdb));
  await informationSent; await tick();
  const information = decodeAx25(tnc.sent.at(-1));
  assert.equal(information.kind, AX25.I);
  assert.equal(information.ns, 0);
  assert.deepEqual(information.data, Uint8Array.of(0x41, 0xc0, 0xdb));
  assert.deepEqual(encodeKiss(tnc.sent.at(-1)).slice(-6), Uint8Array.of(0x41,0xdb,0xdc,0xdb,0xdd,0xc0));
  tnc.receive(peer(AX25.RR | (1 << 5)));
  assert.equal(link.unacked, null);
  const acknowledgementSent = new Promise(resolve => link.addEventListener('tx', event => {
    if ((event.detail.frame.control & 0x0f) === AX25.RR) resolve();
  }));
  tnc.receive(peer(0, Uint8Array.of(0x4f, 0x4b)));
  assert.deepEqual(received, [Uint8Array.of(0x4f, 0x4b)]);
  await acknowledgementSent; await tick();
  assert.equal(decodeAx25(tnc.sent.at(-1)).kind, AX25.RR);
  link.disconnect(); await tick();
  assert.equal(decodeAx25(tnc.sent.at(-1)).kind, AX25.DISC);
  tnc.receive(peer(AX25.UA | AX25.PF));
  assert.equal(link.state, 'closed');
});

test('ignores unrelated stations and rejects an unsanctioned RF connection', () => {
  const tnc = new FakeTnc();
  const link = new Ax25Link(tnc, { local: 'N0CALL', remote: 'GATE-10' });
  assert.throws(() => link.start(), /Enable RF/);
  assert.equal(tnc.sent.length, 0);
  link.finish();
});
