// TD-H9 resource-flash service session over the Bluetooth programming link.
// Ported from prototypes/resource-reader/app.mjs, whose read and write paths
// were proven on hardware (2026-11-08): bulk reads at about 6.5 KiB/s with the
// fixed 15 ms link, and guarded sector erase/program with a status check after
// every 32-byte block. The transport is injected, so Node tests can drive a
// simulated radio; connectBluetooth() wires it to Web Bluetooth.
import { readBlockReply } from '../protocol.mjs';
import {
  BANK_SIZE, BLOCK, BULK_HEADER, BULK_SIZES, LINK_FIXED_ADDRESS, LINK_STOCK_ADDRESS, LOGO_ADDRESS, RAM_SELECT_ADDRESS, RAM_SIZE,
  LOGO_BYTES, OP, SECTOR, assembleBulk, bulkSlotAddress, controlFrame, crc16, dataFrame, hex,
  logoSectorPlan, parseBulkReply, parseStatus, readPlan, statusAddress,
} from './resource.mjs?v=20261108a';

const MAGIC = Uint8Array.of(0x50, 0x56, 0x4f, 0x4a, 0x48, 0x5c, 0x14);
const IDENTITY = Uint8Array.of(0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff);
const SERVICE = '0000ff00-0000-1000-8000-00805f9b34fb';
const WRITE = '0000ff02-0000-1000-8000-00805f9b34fb';
const NOTIFY = '0000ff01-0000-1000-8000-00805f9b34fb';
const MAX_RETRIES = 5;
const ERASED_CRC = 0xe03e;
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

export class BulkUnsupported extends Error {}
export class Stopped extends Error {}

export class ResourceRadio {
  // write(bytes): sends one GATT write. log(message, level): console-style.
  constructor({ write, log = () => {}, linkDelayMs = 1500 }) {
    this.write = write;
    this.log = log;
    this.linkDelayMs = linkDelayMs;
    this.queue = [];
    this.wake = null;
    this.checksumMode = 'auto';
    // 176-byte packets fit the 182-byte link payload seen on macOS; adjusted
    // from the first notifications if the link is smaller.
    this.bulkSizeIndex = BULK_SIZES.indexOf(176);
    this.bulkActive = false;
    this.bulkPartial = null;
    this.bulkPackets = [];
    this.notificationSizes = [];
    this.stopRequested = false;
  }

  // Feed one notification. Bulk packets (B7 5A seq len) may arrive split
  // across notifications larger than the link MTU and are reassembled by
  // their length byte; everything else is reply bytes.
  receive(bytes) {
    let rest = bytes;
    if (this.bulkActive && this.notificationSizes.length < 64) this.notificationSizes.push(rest.length);
    while (rest.length) {
      if (this.bulkPartial) {
        const take = Math.min(rest.length, this.bulkPartial.bytes.length - this.bulkPartial.filled);
        this.bulkPartial.bytes.set(rest.subarray(0, take), this.bulkPartial.filled);
        this.bulkPartial.filled += take;
        rest = rest.subarray(take);
        if (this.bulkPartial.filled === this.bulkPartial.bytes.length) {
          this.bulkPackets.push(this.bulkPartial.bytes);
          this.bulkPartial = null;
        }
      } else if (this.bulkActive && rest.length >= BULK_HEADER && rest[0] === 0xb7 && rest[1] === 0x5a) {
        this.bulkPartial = { bytes: new Uint8Array(rest[3] + BULK_HEADER), filled: 0 };
      } else {
        this.queue.push(...rest);
        rest = rest.subarray(rest.length);
      }
    }
    if (this.wake) { const resolve = this.wake; this.wake = null; resolve(); }
  }

  async readExact(length, timeoutMs = 2500) {
    const output = new Uint8Array(length);
    const deadline = Date.now() + timeoutMs;
    let offset = 0;
    while (offset < length) {
      while (this.queue.length && offset < length) output[offset++] = this.queue.shift();
      if (offset === length) break;
      const remaining = deadline - Date.now();
      if (remaining <= 0) throw new Error(`Timed out waiting for radio (${offset}/${length} bytes).`);
      await new Promise((resolve, reject) => {
        const timer = setTimeout(() => { this.wake = null; reject(new Error(`Timed out waiting for radio (${offset}/${length} bytes).`)); }, remaining);
        this.wake = () => { clearTimeout(timer); resolve(); };
      });
    }
    return output;
  }

  async send(bytes) {
    this.log(`write ${bytes.length} ${hex(bytes.subarray(0, 16))}`, 'debug');
    await this.write(bytes);
  }

  async expectAck(label) {
    const reply = await this.readExact(1);
    if (reply[0] !== 0x06) throw new Error(`${label}: expected ACK 06, got ${hex(reply)}.`);
  }

  async enterProgramming() {
    this.queue = [];
    this.checksumMode = 'auto';
    await this.send(MAGIC);
    await this.expectAck('Handshake');
    await this.send(Uint8Array.of(0x02));
    const ident = await this.readExact(8);
    if (!IDENTITY.every((byte, index) => ident[index] === byte) || ![0x4e, 0x48, 0x47].includes(ident[7])) {
      throw new Error(`Unexpected TD-H9 identity: ${hex(ident)}.`);
    }
    await this.send(Uint8Array.of(0x06));
    await this.expectAck('Enter programming');
  }

  async leaveProgramming() {
    try { await this.send(Uint8Array.of(0x45)); } catch { /* radio already gone */ }
    await sleep(300);
    this.queue = [];
  }

  async readBlock(address) {
    await this.send(Uint8Array.of(0x52, address >> 8, address & 255, BLOCK));
    const reply = await readBlockReply(address, BLOCK, this.checksumMode, length => this.readExact(length), async () => {
      try { return (await this.readExact(1, 150))[0]; }
      catch (error) { if (error.message.startsWith('Timed out')) return null; throw error; }
    });
    this.checksumMode = reply.mode;
    return reply.data;
  }

  async selectBank(bank) {
    return parseStatus(await this.readBlock(statusAddress(bank)), bank);
  }

  // One status read: service version, flash ready, JEDEC ID, write state.
  async checkService() {
    await this.enterProgramming();
    try {
      return await this.selectBank(0);
    } finally {
      await this.leaveProgramming();
    }
  }

  checkStop() {
    if (this.stopRequested) throw new Stopped('Stopped by user.');
  }

  // 32-byte reads (works on every service version; about 1 KiB/s).
  async readSlow(start, length, onProgress) {
    const output = new Uint8Array(length);
    const steps = [...readPlan(start, length)];
    const blocks = length / BLOCK;
    const began = performance.now();
    let done = 0, index = 0, bank = -1, retries = 0;
    await this.enterProgramming();
    while (index < steps.length) {
      this.checkStop();
      const step = steps[index];
      try {
        if (step.kind === 'bank') {
          const state = await this.selectBank(step.bank);
          if (!state.ready) throw new Error('Flash stopped reporting ready.');
          bank = step.bank;
        } else {
          if (Math.floor(step.chip / BANK_SIZE) !== bank) throw new Error('Bank selection lost.');
          output.set(await this.readBlock(step.address), step.chip - start);
          done++;
          if (done % 16 === 0 || done === blocks) onProgress(done / blocks, done * BLOCK / ((performance.now() - began) / 1000));
        }
        index++;
        retries = 0;
      } catch (error) {
        if (error instanceof Stopped || ++retries > MAX_RETRIES) throw error;
        this.log(`Retry ${retries}: ${error.message}`, 'warn');
        await this.leaveProgramming();
        await this.enterProgramming();
        while (index > 0 && steps[index].kind !== 'bank') index--;
        done = steps.slice(0, index).filter(entry => entry.kind === 'data').length;
        bank = -1;
      }
    }
    await this.leaveProgramming();
    return { bytes: output, seconds: (performance.now() - began) / 1000, method: '32-byte' };
  }

  // Bulk read: the radio streams up to 2 KiB per request from the cursor set
  // by the bank-select status read; every byte is matched to its report.
  // Select the bank for `pos`; in RAM mode also switch its cursor to RAM.
  async selectCursor(pos, ram) {
    await this.selectBank(pos / BANK_SIZE);
    if (!ram) return;
    const reply = parseBulkReply(await this.readBlock(RAM_SELECT_ADDRESS));
    if (!reply || reply.status !== 0 || reply.start !== pos) {
      throw new BulkUnsupported(`This firmware has no RAM read (slot 19: ${reply ? reply.statusText : 'no bulk reply'}).`);
    }
  }

  async readFast(start, length, onProgress, { ram = false } = {}) {
    if (start % BANK_SIZE) throw new BulkUnsupported('Fast read needs a 32 KiB aligned start.');
    const output = new Uint8Array(length);
    const end = start + length;
    const began = performance.now();
    let pos = start, retries = 0, linkRequested = false, sizesChecked = false;
    this.notificationSizes = [];
    await this.enterProgramming();
    await this.selectCursor(pos, ram);
    const link = parseBulkReply(await this.readBlock(LINK_FIXED_ADDRESS));
    if (link) {
      linkRequested = link.status === 0;
      this.log(`Requested a fixed 15 ms, latency-0 link: ${link.statusText}.`, link.status === 0 ? 'info' : 'warn');
      await sleep(this.linkDelayMs);
    }
    try {
      while (pos < end) {
        this.checkStop();
        try {
          this.bulkPackets = [];
          this.bulkPartial = null;
          this.bulkActive = true;
          const reply = parseBulkReply(await this.readBlock(bulkSlotAddress(this.bulkSizeIndex, false)));
          if (!reply) throw new BulkUnsupported('This firmware has no bulk read.');
          if (!sizesChecked) {
            sizesChecked = true;
            // The first notification of a split packet is one full link
            // payload: use the largest packet size that fits it.
            const linkPayload = this.notificationSizes[0];
            if (linkPayload && linkPayload < BULK_SIZES[this.bulkSizeIndex] + BULK_HEADER) {
              const fitting = BULK_SIZES.findLastIndex(size => size + BULK_HEADER <= linkPayload);
              if (fitting >= 0) {
                this.log(`Link payload is ${linkPayload} bytes; using ${BULK_SIZES[fitting]}-byte packets.`, 'warn');
                this.bulkSizeIndex = fitting;
              }
            }
          }
          if (reply.count === 0 && reply.status === 6 && this.bulkSizeIndex > 0) { this.bulkSizeIndex--; continue; }
          if (reply.count === 0 && reply.status === 3) { await sleep(30); continue; }
          if (reply.count === 0) throw new Error(`Bulk read: ${reply.statusText}`);
          if (reply.start !== pos) throw new Error(`Radio cursor at 0x${reply.start.toString(16)}, expected 0x${pos.toString(16)}.`);
          let data;
          try {
            data = assembleBulk(this.bulkPackets, reply, BULK_SIZES[this.bulkSizeIndex]);
          } catch (error) {
            if (/expected \d+\.$/.test(error.message) && this.bulkSizeIndex > 0) this.bulkSizeIndex--;
            throw error;
          }
          output.set(data.subarray(0, Math.min(data.length, end - pos)), pos - start);
          pos += data.length;
          if (!reply.cursorValid || reply.cursorRam !== ram || reply.cursor !== (pos & 0xffffff)) throw new Error('Radio cursor disagrees with the bytes received.');
          retries = 0;
          const doneBytes = Math.min(pos, end) - start;
          onProgress(doneBytes / length, doneBytes / ((performance.now() - began) / 1000));
        } catch (error) {
          if (this.bulkPartial && this.bulkSizeIndex > 0) this.bulkSizeIndex--;
          this.bulkPartial = null;
          if (error instanceof BulkUnsupported || error instanceof Stopped || ++retries > MAX_RETRIES) throw error;
          this.log(`Fast retry ${retries} at 0x${pos.toString(16)}: ${error.message}`, 'warn');
          await this.leaveProgramming();
          await this.enterProgramming();
          pos = Math.floor(pos / BANK_SIZE) * BANK_SIZE;
          await this.selectCursor(pos, ram);
        }
      }
    } finally {
      this.bulkActive = false;
    }
    if (linkRequested) {
      const restored = parseBulkReply(await this.readBlock(LINK_STOCK_ADDRESS));
      this.log(`Restored stock link parameters: ${restored?.statusText ?? 'no reply'}.`, 'info');
    }
    await this.leaveProgramming();
    return { bytes: output, seconds: (performance.now() - began) / 1000, method: 'fast' };
  }

  // Research builds only: read RAM (32 KiB-aligned start, inside 0x00000-0x2FFFF).
  async readRam(start, length, onProgress = () => {}) {
    if (start % BANK_SIZE || length <= 0 || length % BLOCK || start + length > RAM_SIZE) {
      throw new Error('RAM reads need a 32 KiB-aligned start inside 0x00000-0x2FFFF.');
    }
    return this.readFast(start, length, onProgress, { ram: true });
  }

  // onProgress(fraction, bytesPerSecond)
  async readRange(start, length, onProgress = () => {}) {
    try {
      return await this.readFast(start, length, onProgress);
    } catch (error) {
      if (!(error instanceof BulkUnsupported)) throw error;
      this.log(`${error.message} Using 32-byte reads.`, 'warn');
      await this.leaveProgramming();
      return this.readSlow(start, length, onProgress);
    }
  }

  async sendWrite(frame) {
    await this.send(frame);
    await this.expectAck('Write');
  }

  // Status read through the sector's own bank (S2 status bytes 12-19).
  async writeStatus(address) {
    const state = await this.selectBank(Math.floor(address / BANK_SIZE));
    if (state.version !== 2) throw new Error('This radio runs firmware without the boot-logo write service.');
    if (!state.ready) throw new Error('Flash is not ready.');
    return state.write;
  }

  async expectWrite(address, label, check) {
    const state = await this.writeStatus(address);
    if (state.result !== 0 || (check && !check(state))) {
      throw new Error(`${label}: radio reports ${state.resultText} (pointer 0x${state.pointer.toString(16)}, CRC 0x${state.crc.toString(16)}).`);
    }
    return state;
  }

  // Rewrite the logo sectors that differ between `current` (read from this
  // radio) and `next`. Each erase proves the sector still holds `current`
  // (CRC); each 32-byte block is checked by the radio; the whole logo is read
  // back at the end. `current` is updated as sectors complete, so a stopped
  // write can be resumed by planning again. onProgress(fraction, phase).
  async writeLogo(current, next, onProgress = () => {}) {
    const plan = logoSectorPlan(current, next);
    if (!plan.length) return { sectors: 0, bytes: current };
    // Work units weighted by measured time: about 1 s per erase, 70 ms per
    // verified block, about 9 s for the 40 KiB read-back.
    const ERASE_UNITS = 14, VERIFY_UNITS = 128;
    const writeUnits = plan.reduce((sum, entry) => sum + ERASE_UNITS + (entry.erasedTarget ? 0 : SECTOR / BLOCK), 0);
    const total = writeUnits + VERIFY_UNITS;
    let done = 0;
    const advance = (units, text) => { done += units; onProgress(done / total, text); };
    let armed = false;
    await this.enterProgramming();
    try {
      for (const [index, entry] of plan.entries()) {
        this.checkStop();
        const label = `0x${entry.address.toString(16)}`;
        const where = `Sector ${index + 1} of ${plan.length}`;
        advance(0, `${where}: erasing`);
        await this.sendWrite(controlFrame(OP.erase, entry.address, entry.oldCrc));
        await this.expectWrite(entry.address, `Erase ${label}`, state => state.crc === ERASED_CRC);
        current.fill(0xff, entry.offset, entry.offset + SECTOR);
        advance(ERASE_UNITS, `${where}: erased`);
        if (entry.erasedTarget) continue;
        const data = next.subarray(entry.offset, entry.offset + SECTOR);
        await this.sendWrite(controlFrame(OP.program, entry.address));
        armed = true;
        await this.expectWrite(entry.address, `Begin ${label}`, state => state.programming && state.pointer === entry.address);
        for (let offset = 0; offset < SECTOR; offset += BLOCK) {
          await this.sendWrite(dataFrame(data.subarray(offset, offset + BLOCK)));
          await this.expectWrite(entry.address, `Program ${label}+0x${offset.toString(16)}`,
            state => state.pointer === entry.address + offset + BLOCK && state.programming === (offset + BLOCK < SECTOR));
          advance(1, `${where}: ${offset + BLOCK} of ${SECTOR} bytes`);
        }
        armed = false;
        current.set(data, entry.offset);
      }
    } catch (error) {
      if (armed) { try { await this.sendWrite(controlFrame(OP.disarm)); } catch { /* reported by caller */ } }
      await this.leaveProgramming();
      throw error;
    }
    await this.leaveProgramming();
    const check = await this.readRange(LOGO_ADDRESS, LOGO_BYTES, fraction =>
      onProgress((writeUnits + fraction * VERIFY_UNITS) / total, 'Verifying the whole logo'));
    const differing = check.bytes.reduce((count, byte, index) => count + (byte !== next[index]), 0);
    if (differing) throw new Error(`Read-back differs from the new logo in ${differing} bytes.`);
    current.set(check.bytes);
    return { sectors: plan.length, bytes: check.bytes };
  }
}

// Web Bluetooth: choose a TD-H9, subscribe to notifications, return a session.
export async function connectBluetooth({ log = () => {}, onDisconnect = () => {} } = {}) {
  if (!navigator.bluetooth) throw new Error('Web Bluetooth is unavailable. Use desktop Chrome or Edge over HTTPS.');
  const device = await navigator.bluetooth.requestDevice({ filters: [{ namePrefix: 'TD-H9' }], optionalServices: [SERVICE] });
  device.addEventListener('gattserverdisconnected', onDisconnect);
  try {
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE);
    const writer = await service.getCharacteristic(WRITE);
    const reader = await service.getCharacteristic(NOTIFY);
    const radio = new ResourceRadio({
      log,
      write: bytes => {
        if (!device.gatt.connected) throw new Error('Radio disconnected.');
        return writer.properties.write ? writer.writeValueWithResponse(bytes) : writer.writeValueWithoutResponse(bytes);
      },
    });
    reader.addEventListener('characteristicvaluechanged', event => {
      const view = event.target.value;
      radio.receive(new Uint8Array(view.buffer, view.byteOffset, view.byteLength));
    });
    await reader.startNotifications();
    return { radio, name: device.name || 'TD-H9', disconnect: () => device.gatt.connected && device.gatt.disconnect() };
  } catch (error) {
    if (device.gatt.connected) device.gatt.disconnect();
    throw error;
  }
}

export { crc16 };
