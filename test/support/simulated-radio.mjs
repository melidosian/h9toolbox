import { BULK_SIZES, SECTOR, crc16 } from '../../dist/boot-logo/resource.mjs';
import { ResourceRadio } from '../../dist/boot-logo/radio.mjs';

// A simulated TD-H9 for tests (Node, and the browser via a fake Web
// Bluetooth device). With `service` (default) it runs the H9 Toolbox
// resource service, following resource_service.s and resource_write.s: clone
// handshake, R/W framing with a checksum byte, status slots that select a
// bank and set the bulk cursor, bulk streaming split at the link MTU, link
// slots, and guarded sector erase/program. Without it, it behaves like stock
// firmware: every R address is an ordinary clone read. Clone reads come from
// the rw_freq partition at chip 0x900000, as in the stock handler.
// With `ramread` it also implements research slot 19 (stream `this.ram`).
// `deliver(bytes)` receives each notification (default: this.radio).
export class SimulatedRadio {
  constructor({ mtu = 182, bulk = true, service = true, ramread = false, deliver = null } = {}) {
    this.chip = new Uint8Array(0x1000000).fill(0xff);
    this.ram = new Uint8Array(0x30000);
    this.ramread = ramread;
    this.cursorRam = false;
    this.dropBulk = 0; // set to n to drop the reply to the n-th next bulk request
    this.mtu = mtu;
    this.bulk = bulk;
    this.service = service;
    this.reads = 0;
    this.deliver = deliver ?? (bytes => this.radio.receive(bytes));
    this.bank = 0;
    this.cursor = null;
    this.write = { result: 0xfe, programming: false, pointer: 0, end: 0, ops: 0, crc: 0 };
    this.linkRequests = [];
    this.erases = 0;
    this.radio = new ResourceRadio({ write: bytes => this.handle(Uint8Array.from(bytes)), linkDelayMs: 0 });
  }

  notify(bytes) {
    for (let offset = 0; offset < bytes.length; offset += this.mtu) {
      const part = bytes.slice(offset, offset + this.mtu);
      setTimeout(() => this.deliver(part), 0);
    }
  }

  async handle(bytes) {
    if (this.exitAckPending && !(bytes.length === 1 && bytes[0] === 0x45)) { this.exitAckPending = false; this.notify(Uint8Array.of(6)); }
    const magic = [0x50, 0x56, 0x4f, 0x4a, 0x48, 0x5c, 0x14];
    if (bytes.length === 7 && magic.every((b, i) => bytes[i] === b)) return this.notify(Uint8Array.of(6));
    if (bytes.length === 1 && bytes[0] === 0x02) return this.notify(Uint8Array.of(0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff, 0x4e));
    if (bytes.length === 1 && bytes[0] === 0x06) return this.notify(Uint8Array.of(6));
    // The radio acknowledges an exit (0x45) late: the 06 goes out just
    // before its reply to the next command. Seen on hardware 2026-11-09: a
    // session started right after an exit read 06 54 44 48 39 ... as the
    // identity (the stray 06 was taken as the handshake ACK).
    if (bytes.length === 1 && bytes[0] === 0x45) { this.exitAckPending = true; return undefined; }
    const address = (bytes[1] << 8) | bytes[2];
    if (bytes[0] === 0x52 && bytes.length === 4) return this.read(address);
    if (bytes[0] === 0x57 && bytes.length === 37) {
      const payload = bytes.slice(4, 36);
      const sum = payload.reduce((a, b) => (a + b) & 255, 0);
      if (this.service && address >= 0x4000 && address < 0x5000) this.serviceWrite(address, payload, sum === bytes[36]);
      return this.notify(Uint8Array.of(6));
    }
    throw new Error(`Simulator got unexpected bytes ${[...bytes]}`);
  }

  reply(address, data) {
    const frame = new Uint8Array(37);
    frame.set([0x57, address >> 8, address & 255, 32]);
    frame.set(data, 4);
    frame[36] = data.reduce((a, b) => (a + b) & 255, 0);
    this.notify(frame);
  }

  read(address) {
    const data = new Uint8Array(32);
    this.reads++;
    if (!this.service || address < 0x3c00) {
      data.set(this.chip.subarray(0x900000 + address, 0x900000 + address + 32));
      return this.reply(address, data);
    }
    if (address >= 0x3c00 && address < 0x4000) {
      if (!this.bulk) return this.reply(address, data);
      const k = (address - 0x3c00) >> 5;
      let status = 0, start = 0, count = 0;
      if (k === 17 || k === 18) {
        this.linkRequests.push(k);
      } else if (k === 19 && this.ramread) {
        if (this.cursor === null) status = 5;
        else { this.cursorRam = true; start = this.cursor; }
      } else if ((k < 5 || (k >= 8 && k < 13)) && this.cursor !== null && this.cursorRam && this.cursor >= 0x30000) {
        status = 0;
      } else if ((k < 5 || (k >= 8 && k < 13)) && this.dropBulk > 0 && --this.dropBulk === 0) {
        return undefined; // fault injection: this bulk request gets no reply at all
      } else if (k < 5 || (k >= 8 && k < 13)) {
        const size = BULK_SIZES[k % 8];
        const source = this.cursorRam ? this.ram : this.chip;
        if (this.cursor === null) status = 5;
        else {
          start = this.cursor;
          let seq = 0;
          while (count < 2048) {
            const n = Math.min(size, 2048 - count);
            const packet = new Uint8Array(4 + n);
            packet.set([0xb7, 0x5a, seq++ & 255, n]);
            packet.set(source.subarray(start + count, start + count + n), 4);
            this.notify(packet);
            count += n;
          }
          this.cursor = start + count;
        }
      } else status = 4;
      const words = new DataView(data.buffer);
      data.set([0x48, 0x39, 0x42, 0x4b]);
      words.setUint32(4, (start | (status << 24)) >>> 0, true);
      words.setUint32(8, (count | (494 << 12)) >>> 0, true);
      words.setUint32(12, ((this.cursor ?? 0) | ((this.cursorRam ? 0xb8 : 0xb7) << 24)) >>> 0, true);
      return this.reply(address, data);
    }
    if (address >= 0x4000 && address < 0x8000) {
      this.bank = (address - 0x4000) >> 5;
      this.cursor = this.bank << 15;
      this.cursorRam = false;
      const w = this.write;
      data.set([0x48, 0x39, 0x52, 0x53, 2, 0xc8, 0x40, 0x18, 1, this.bank & 255, this.bank >> 8, 0x5a,
        w.result, w.programming ? 2 : 0, w.pointer & 255, (w.pointer >> 8) & 255, w.pointer >> 16, w.ops & 255, w.crc & 255, w.crc >> 8]);
      return this.reply(address, data);
    }
    if (address >= 0x8000) {
      const chip = this.bank * 0x8000 + (address - 0x8000);
      data.set(this.chip.subarray(chip, chip + 32));
    }
    return this.reply(address, data);
  }

  serviceWrite(address, payload, checksumOk) {
    const w = this.write;
    const fail = code => { w.result = code; w.programming = false; w.end = 0; };
    if (!checksumOk) return fail(8);
    if (address === 0x4000) {
      if (![0x48, 0x39, 0x57, 0x52].every((b, i) => payload[i] === b)) return fail(1);
      const op = payload[4];
      const chip = payload[5] | (payload[6] << 8) | (payload[7] << 16);
      const crc = payload[8] | (payload[9] << 8);
      if (op === 0) { w.programming = false; w.end = 0; }
      else if (op === 1) {
        if (chip % SECTOR) return fail(4);
        w.crc = crc16(this.chip.subarray(chip, chip + SECTOR));
        if (w.crc !== crc) return fail(5);
        this.chip.fill(0xff, chip, chip + SECTOR);
        this.erases++;
        w.crc = crc16(this.chip.subarray(chip, chip + SECTOR));
        w.programming = false; w.end = 0;
      } else if (op === 2) {
        if (chip % 32) return fail(4);
        w.pointer = chip; w.end = (Math.floor(chip / SECTOR) + 1) * SECTOR; w.programming = true;
      } else return fail(12);
    } else {
      if (!w.programming) return fail(7);
      if (!this.chip.subarray(w.pointer, w.pointer + 32).every(b => b === 0xff)) return fail(10);
      this.chip.set(payload, w.pointer);
      w.pointer += 32;
      if (w.pointer === w.end) { w.programming = false; w.end = 0; }
    }
    w.result = 0;
    w.ops++;
  }
}
