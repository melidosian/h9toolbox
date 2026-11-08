import test from 'node:test';
import assert from 'node:assert/strict';
import { BULK_SIZES, LOGO_ADDRESS, LOGO_BYTES, SECTOR, crc16 } from '../dist/boot-logo/resource.mjs';
import { ResourceRadio, Stopped } from '../dist/boot-logo/radio.mjs';

// A simulated TD-H9 running the S2 service, following resource_service.s and
// resource_write.s: clone handshake, R/W framing with a checksum byte, status
// slots that select a bank and set the bulk cursor, bulk streaming split at
// the link MTU, link slots, and guarded sector erase/program.
class SimulatedRadio {
  constructor({ mtu = 182, bulk = true } = {}) {
    this.chip = new Uint8Array(0x1000000).fill(0xff);
    this.mtu = mtu;
    this.bulk = bulk;
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
      setTimeout(() => this.radio.receive(part), 0);
    }
  }

  async handle(bytes) {
    const magic = [0x50, 0x56, 0x4f, 0x4a, 0x48, 0x5c, 0x14];
    if (bytes.length === 7 && magic.every((b, i) => bytes[i] === b)) return this.notify(Uint8Array.of(6));
    if (bytes.length === 1 && bytes[0] === 0x02) return this.notify(Uint8Array.of(0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff, 0x4e));
    if (bytes.length === 1 && bytes[0] === 0x06) return this.notify(Uint8Array.of(6));
    if (bytes.length === 1 && bytes[0] === 0x45) return undefined;
    const address = (bytes[1] << 8) | bytes[2];
    if (bytes[0] === 0x52 && bytes.length === 4) return this.read(address);
    if (bytes[0] === 0x57 && bytes.length === 37) {
      const payload = bytes.slice(4, 36);
      const sum = payload.reduce((a, b) => (a + b) & 255, 0);
      if (address >= 0x4000 && address < 0x5000) this.serviceWrite(address, payload, sum === bytes[36]);
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
    if (address >= 0x3c00 && address < 0x4000) {
      if (!this.bulk) return this.reply(address, data);
      const k = (address - 0x3c00) >> 5;
      let status = 0, start = 0, count = 0;
      if (k === 17 || k === 18) {
        this.linkRequests.push(k);
      } else if (k % 8 < 5) {
        const size = BULK_SIZES[k % 8];
        if (this.cursor === null) status = 5;
        else {
          start = this.cursor;
          let seq = 0;
          while (count < 2048) {
            const n = Math.min(size, 2048 - count);
            const packet = new Uint8Array(4 + n);
            packet.set([0xb7, 0x5a, seq++ & 255, n]);
            packet.set(this.chip.subarray(start + count, start + count + n), 4);
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
      words.setUint32(12, ((this.cursor ?? 0) | (0xb7 << 24)) >>> 0, true);
      return this.reply(address, data);
    }
    if (address >= 0x4000 && address < 0x8000) {
      this.bank = (address - 0x4000) >> 5;
      this.cursor = this.bank << 15;
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

const logoPattern = seed => Uint8Array.from({ length: LOGO_BYTES }, (_, i) => (i * 13 + seed + (i >> 7)) & 255);

test('checks the service: version 2, flash ready, 16 MiB JEDEC', async () => {
  const sim = new SimulatedRadio();
  const status = await sim.radio.checkService();
  assert.equal(status.version, 2);
  assert.equal(status.ready, true);
  assert.deepEqual(status.jedec, [0xc8, 0x40, 0x18]);
});

test('fast read returns the logo exactly, requesting and restoring the link', async () => {
  const sim = new SimulatedRadio();
  const logo = logoPattern(1);
  sim.chip.set(logo, LOGO_ADDRESS);
  const fractions = [];
  const result = await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES, fraction => fractions.push(fraction));
  assert.equal(result.method, 'fast');
  assert.deepEqual(result.bytes, logo);
  assert.deepEqual(sim.linkRequests, [18, 17]);
  assert.equal(fractions.at(-1), 1);
});

test('fast read reassembles packets split by a small link MTU and shrinks packets', async () => {
  const sim = new SimulatedRadio({ mtu: 100 });
  const logo = logoPattern(2);
  sim.chip.set(logo, LOGO_ADDRESS);
  const result = await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES);
  assert.deepEqual(result.bytes, logo);
  assert.ok(BULK_SIZES[sim.radio.bulkSizeIndex] + 4 <= 100);
});

test('firmware without bulk reads falls back to 32-byte reads', async () => {
  const sim = new SimulatedRadio({ bulk: false });
  const logo = logoPattern(3);
  sim.chip.set(logo, LOGO_ADDRESS);
  const result = await sim.radio.readRange(LOGO_ADDRESS, 0x2000);
  assert.equal(result.method, '32-byte');
  assert.deepEqual(result.bytes, logo.subarray(0, 0x2000));
});

test('writes only differing sectors, verifies, and leaves other data alone', async () => {
  const sim = new SimulatedRadio();
  const old = logoPattern(4);
  sim.chip.set(old, LOGO_ADDRESS);
  sim.chip.fill(0x11, LOGO_ADDRESS + LOGO_BYTES, LOGO_ADDRESS + LOGO_BYTES + SECTOR); // next resource
  const next = old.slice();
  next.fill(0x22, 0x1000, 0x1800);   // sector 1 changes
  next.fill(0xff, 0x9000, 0xa000);   // sector 9 becomes erased (erase only)
  const current = (await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES)).bytes;
  const phases = [];
  const result = await sim.radio.writeLogo(current, next, (fraction, text) => phases.push([fraction, text]));
  assert.equal(result.sectors, 2);
  assert.equal(sim.erases, 2);
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES), next);
  assert.ok(sim.chip.subarray(LOGO_ADDRESS + LOGO_BYTES, LOGO_ADDRESS + LOGO_BYTES + SECTOR).every(b => b === 0x11));
  assert.deepEqual(current, next);
  assert.ok(phases.every(([a], i) => i === 0 || a >= phases[i - 1][0]), 'progress never goes backwards');
  assert.ok(phases.at(-1)[0] > 0.999);
  assert.equal((await sim.radio.writeLogo(current, next)).sectors, 0);
});

test('refuses to erase when the radio no longer holds what was read', async () => {
  const sim = new SimulatedRadio();
  sim.chip.set(logoPattern(5), LOGO_ADDRESS);
  const current = (await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES)).bytes;
  sim.chip[LOGO_ADDRESS + 10] ^= 1; // changed behind our back
  const before = sim.chip.slice(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES);
  await assert.rejects(sim.radio.writeLogo(current, logoPattern(6)), /sector CRC mismatch/);
  assert.equal(sim.erases, 0);
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES), before);
});

test('a stopped write resumes from the sectors already done', async () => {
  const sim = new SimulatedRadio();
  sim.chip.set(logoPattern(7), LOGO_ADDRESS);
  const current = (await sim.radio.readRange(LOGO_ADDRESS, LOGO_BYTES)).bytes;
  const next = logoPattern(8);
  let sectorsSeen = 0;
  await assert.rejects(sim.radio.writeLogo(current, next, (_, text) => {
    if (text.endsWith('erasing') && ++sectorsSeen === 4) sim.radio.stopRequested = true;
  }), Stopped);
  sim.radio.stopRequested = false;
  // Stop takes effect between sectors: the sector in progress (the 4th)
  // finishes, so four are written and six remain.
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + 4 * SECTOR), next.subarray(0, 4 * SECTOR));
  assert.deepEqual(current.subarray(0, 4 * SECTOR), next.subarray(0, 4 * SECTOR));
  const result = await sim.radio.writeLogo(current, next);
  assert.equal(result.sectors, 6);
  assert.deepEqual(sim.chip.subarray(LOGO_ADDRESS, LOGO_ADDRESS + LOGO_BYTES), next);
});
