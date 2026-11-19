import { AX25, decodeAx25, encodeAx25, normalizeCall } from './ax25.mjs';
import { encodeKiss } from './kiss.mjs';

// A single AX.25 v2.0 stop-and-wait connection. Pat's H9 bridge uses the
// same conservative one-frame window and 80-byte information limit.
export class Ax25Link extends EventTarget {
  constructor(tnc, { local, remote, via = [], allowTransmit = false, t1 = 5700,
    t2 = 1000, t3 = 18000, maxInfo = 80, maxKiss = 182, retries = 3 } = {}) {
    super();
    this.tnc = tnc;
    this.local = normalizeCall(local);
    this.remote = normalizeCall(remote);
    this.via = via.map(normalizeCall);
    this.allowTransmit = allowTransmit;
    Object.assign(this, { t1, t2, t3, maxInfo, maxKiss, retries });
    this.state = 'new';
    this.vs = this.vr = this.tries = this.epoch = 0;
    this.unacked = null;
    this.queue = [];
    this.queuedBytes = 0;
    this.busy = this.polling = this.reject = false;
    this.timers = new Map();
    this.onPacket = event => this.receivePacket(event.detail);
    tnc.addEventListener('packet', this.onPacket);
  }
  emit(type, detail) { this.dispatchEvent(new CustomEvent(type, { detail })); }
  setState(value) { this.state = value; this.emit('state', value); }
  cancel(name) { clearTimeout(this.timers.get(name)); this.timers.delete(name); }
  schedule(name, ms, fn) { this.cancel(name); this.timers.set(name, setTimeout(() => { this.timers.delete(name); fn(); }, ms)); }
  invalidate() { this.cancel('timer'); this.epoch++; }
  frame(control, command = true, data = new Uint8Array(), pid = AX25.PID) {
    return { source: this.local, destination: this.remote, via: this.via, control, command, data, pid };
  }
  transmit(frame, timer = null, terminal = false) {
    const epoch = this.epoch;
    const bytes = encodeAx25(frame);
    this.emit('tx', { frame, bytes });
    void this.tnc.sendFrame(bytes, { allowTransmit: this.allowTransmit }).then(() => {
      if (timer && (terminal || this.state !== 'closed' && epoch === this.epoch)) this.schedule('timer', this.t1, timer);
    }, error => this.fail(error));
  }
  start() {
    if (this.state !== 'new') throw new Error('This AX.25 session has already started.');
    if (!this.allowTransmit && this.tnc.transport !== 'demo') throw new Error('Enable RF transmission to connect.');
    this.setState('opening');
    this.controlRequest(AX25.SABM);
  }
  controlRequest(kind) {
    this.invalidate();
    this.transmit(this.frame(kind | AX25.PF), () => this.timeout());
  }
  timeout() {
    if (this.state === 'closed') return;
    if (this.tries >= this.retries) {
      if (this.state === 'open') this.disconnect();
      else this.finish('No response from gateway.');
      return;
    }
    this.tries++;
    if (this.state === 'opening') this.controlRequest(AX25.SABM);
    else if (this.state === 'closing') this.controlRequest(AX25.DISC);
    else if (this.unacked && this.tries > 1 && !this.busy) this.retryData();
    else this.pollPeer();
  }
  pollPeer() { this.invalidate(); this.polling = true; this.supervisory(AX25.RR, true, true, () => this.timeout()); }
  retryData() { this.invalidate(); this.polling = true; this.dataFrame(true); }
  idle() { this.schedule('timer', this.t3, () => { this.tries = 0; this.pollPeer(); }); }
  enqueue(data, pid = AX25.PID) {
    if (this.state !== 'open') throw new Error('Connect to the gateway first.');
    if (!(data instanceof Uint8Array) || !data.length) return;
    if (this.queuedBytes + data.length > 65536) throw new Error('AX.25 send queue full.');
    this.queue.push({ pid, data: Uint8Array.from(data), offset: 0 });
    this.queuedBytes += data.length;
    if (!this.unacked && !this.busy && !this.timers.has('batch')) this.schedule('batch', 100, () => this.pump());
  }
  pump() {
    this.cancel('batch');
    if (this.state !== 'open' || this.unacked || this.busy || !this.queue.length) return;
    const item = this.queue[0];
    const control = (this.vs << 1) | (this.vr << 5) | AX25.PF;
    let data;
    for (let size = Math.min(this.maxInfo, item.data.length - item.offset); size > 0; size--) {
      const part = item.data.slice(item.offset, item.offset + size);
      if (encodeKiss(encodeAx25(this.frame(control, true, part, item.pid))).length <= this.maxKiss) { data = part; break; }
    }
    if (!data) return this.fail(new Error('BLE write limit cannot fit one AX.25 data byte.'));
    item.offset += data.length;
    this.queuedBytes -= data.length;
    if (item.offset === item.data.length) this.queue.shift();
    this.unacked = { ns: this.vs, pid: item.pid, data };
    this.vs = (this.vs + 1) & 7;
    this.tries = 0;
    this.polling = false;
    this.invalidate();
    this.dataFrame(false);
  }
  dataFrame(poll) {
    this.cancel('ack');
    const { ns, pid, data } = this.unacked;
    this.transmit(this.frame((ns << 1) | (this.vr << 5) | (poll ? AX25.PF : 0), true, data, pid), () => this.timeout());
  }
  supervisory(kind, command = false, poll = false, timer = null) {
    this.cancel('ack');
    this.transmit(this.frame(kind | (this.vr << 5) | (poll ? AX25.PF : 0), command), timer);
  }
  receivePacket(packet) {
    if (packet.port !== 0 || packet.command !== 0) return;
    let frame;
    try { frame = decodeAx25(packet.payload); } catch { return; }
    if (frame.source !== this.remote || frame.destination !== this.local) return;
    this.emit('rx', frame);
    if (frame.kind === AX25.DISC) {
      this.transmit(this.frame(AX25.UA | (frame.poll ? AX25.PF : 0), false), null, true);
      this.finish('Gateway disconnected.');
    } else if (frame.kind === AX25.DM || frame.kind === AX25.FRMR) this.finish('Gateway refused the connection.');
    else if (this.state === 'opening' && frame.kind === AX25.UA && frame.command !== true) {
      this.invalidate(); this.setState('open'); this.emit('connected', frame); this.idle();
    } else if (this.state === 'closing' && frame.kind === AX25.UA && frame.command !== true) this.finish('Disconnected.');
    else if (this.state === 'open' && [AX25.I, AX25.RR, AX25.RNR, AX25.REJ].includes(frame.kind)) this.receiveLink(frame);
  }
  receiveLink(frame) {
    const final = frame.poll && frame.command === false && this.polling;
    let polled = frame.poll && frame.command !== false;
    const advanced = this.unacked && frame.nr === this.vs;
    if (advanced) { this.invalidate(); this.unacked = null; this.polling = false; this.tries = 0; this.emit('ack', frame.nr); }
    if (frame.kind !== AX25.I) this.busy = frame.kind === AX25.RNR;
    if (frame.kind === AX25.I) {
      if (frame.ns === this.vr) {
        this.vr = (this.vr + 1) & 7; this.reject = false;
        this.emit('data', { data: frame.data, pid: frame.pid });
        if (!polled) this.schedule('ack', this.t2, () => this.acknowledge());
      } else if (frame.ns === ((this.vr - 1) & 7)) {
        if (!polled) this.acknowledge();
      } else if (!this.reject) {
        this.reject = true; this.supervisory(AX25.REJ, false, polled); polled = false;
      }
    }
    const resend = this.unacked && !this.busy && frame.nr === this.unacked.ns;
    if (resend && (final || frame.kind === AX25.REJ)) {
      if (this.tries >= this.retries) this.disconnect();
      else { this.tries++; this.retryData(); }
    } else if (!this.unacked && !this.busy) {
      this.polling = false;
      if (this.timers.has('batch') || !this.queue.length) this.idle();
      else this.pump();
    } else if (final || advanced) this.schedule('timer', this.t1, () => this.timeout());
    if (polled && this.state === 'open') this.supervisory(AX25.RR, false, true);
  }
  acknowledge() { if (this.state === 'open') this.supervisory(AX25.RR); }
  disconnect() {
    if (this.state === 'new' || this.state === 'closed' || this.state === 'closing') return;
    this.setState('closing'); this.tries = 0;
    this.cancel('batch'); this.cancel('ack'); this.controlRequest(AX25.DISC);
  }
  fail(error) { this.finish(`AX.25 link failed: ${error.message}`); }
  finish(reason = 'Disconnected.') {
    if (this.state === 'closed') return;
    this.setState('closed'); this.invalidate();
    this.cancel('batch'); this.cancel('ack');
    this.tnc.removeEventListener('packet', this.onPacket);
    this.emit('closed', reason);
  }
}
