import { KissDecoder, encodeKiss, encodeAprs, decodeAprs } from './kiss.mjs';

export class KissTnc extends EventTarget {
  constructor() { super(); this.decoder = new KissDecoder(); this.transport = null; this.queue = Promise.resolve(); this.session = 0; this.bleOptions = {}; this.lastRx = this.lastTx = 0; }
  emit(type, detail) { this.dispatchEvent(new CustomEvent(type, { detail })); }
  ensureClosed() { if (this.transport) throw new Error('Disconnect the current transport first.'); }
  receive(bytes) {
    this.lastRx = Date.now();
    for (const frame of this.decoder.feed(bytes)) {
      let aprs = null, decodeError = null;
      if (frame.command === 0) {
        try { aprs = decodeAprs(frame.payload); } catch (error) { decodeError = error.message; }
      }
      this.emit('packet', { ...frame, aprs, decodeError, demo: this.transport === 'demo' });
    }
  }
  connectDemo() {
    this.ensureClosed(); this.decoder.reset(); this.transport = 'demo'; this.emit('state', 'Demo connected — no RF');
    this.demoReceive();
  }
  demoReceive() {
    if (this.transport !== 'demo') throw new Error('Connect demo mode first.');
    const packet = encodeKiss(encodeAprs('N0CALL-1>APRS,WIDE1-1:!4903.50N/07201.75W-Demo station; simulated packet'));
    // Deliberately fragment it, as BLE/serial reads do in practice.
    for (let i = 0; i < packet.length; i += 7) this.receive(packet.slice(i, i + 7));
  }
  async connectBle({ service, write, notify, singleWrite = false, maxFrameSize = 182, txGapMs = 0, rxGuardMs = 0 }) {
    this.ensureClosed();
    if (!globalThis.navigator?.bluetooth) throw new Error('Web Bluetooth needs desktop Chrome or Edge on localhost or HTTPS.');
    this.decoder.reset();
    try {
      this.device = await navigator.bluetooth.requestDevice({ acceptAllDevices: true, optionalServices: [service] });
      this.onBleDisconnect = () => { void this.disconnect(); };
      this.device.addEventListener('gattserverdisconnected', this.onBleDisconnect);
      const server = await this.device.gatt.connect();
      const svc = await server.getPrimaryService(service);
      this.writer = await svc.getCharacteristic(write);
      this.notifier = await svc.getCharacteristic(notify);
      if (!this.writer.properties.write && !this.writer.properties.writeWithoutResponse) throw new Error('The selected TX characteristic is not writable.');
      this.onNotification = event => {
        const view = event.target.value;
        this.receive(new Uint8Array(view.buffer, view.byteOffset, view.byteLength));
      };
      this.notifier.addEventListener('characteristicvaluechanged', this.onNotification);
      await this.notifier.startNotifications();
      this.bleOptions = { singleWrite, maxFrameSize, txGapMs, rxGuardMs };
      this.transport = 'ble'; this.emit('state', `BLE connected: ${this.device.name || 'unnamed device'}`);
    } catch (error) { await this.disconnect(); throw error; }
  }
  async connectSerial(baudRate = 9600) {
    this.ensureClosed();
    if (!globalThis.navigator?.serial) throw new Error('Web Serial is unavailable. Use desktop Chrome or Edge on localhost or HTTPS.');
    if (!Number.isInteger(baudRate) || baudRate < 300 || baudRate > 1000000) throw new Error('Choose a baud rate between 300 and 1000000.');
    this.decoder.reset();
    try {
      this.serialPort = await navigator.serial.requestPort();
      await this.serialPort.open({ baudRate });
      this.reader = this.serialPort.readable.getReader();
      this.transport = 'serial'; this.emit('state', 'Serial connected — waiting for KISS packets');
      const reader = this.reader;
      this.readTask = (async () => {
        try {
          while (this.reader === reader) {
            const { value, done } = await reader.read();
            if (done) break;
            this.receive(value);
          }
        } catch (error) { this.emit('error', error.message); }
        finally {
          reader.releaseLock();
          if (this.reader === reader) void this.disconnect();
        }
      })();
    } catch (error) { await this.disconnect(); throw error; }
  }
  // Callers explicitly authorize RF by passing allowTransmit. Connecting never
  // sends initialization, KISS settings, programming commands, or packets.
  sendAprs(line, { port = 0, allowTransmit = false } = {}) {
    return this.sendFrame(encodeAprs(line), { port, allowTransmit, label: line });
  }
  sendFrame(payload, { port = 0, allowTransmit = false, label = 'AX.25 frame' } = {}) {
    if (!(payload instanceof Uint8Array) || !payload.length) return Promise.reject(new Error('Enter a nonempty AX.25 frame.'));
    const bytes = encodeKiss(payload, port);
    const transport = this.transport, device = this.device, serialPort = this.serialPort, writer = this.writer;
    const session = this.session;
    if (!transport) return Promise.reject(new Error('Connect a transport first.'));
    if (transport !== 'demo' && !allowTransmit) return Promise.reject(new Error('Enable transmission before sending to hardware.'));
    const operation = this.queue.then(async () => {
      const checkConnection = () => {
        if (this.session !== session || this.transport !== transport || this.device !== device || this.serialPort !== serialPort) throw new Error('Connection changed before transmission completed.');
      };
      checkConnection();
      if (transport === 'ble') {
        const { singleWrite, maxFrameSize, txGapMs, rxGuardMs } = this.bleOptions;
        if (singleWrite) {
          if (!writer.properties.writeWithoutResponse) throw new Error('H9 KISS needs BLE write without response.');
          if (bytes.length > maxFrameSize) throw new Error(`KISS frame exceeds the H9 BLE limit of ${maxFrameSize} bytes.`);
          while (true) {
            checkConnection();
            const wait = Math.max(this.lastTx + txGapMs, this.lastRx + rxGuardMs) - Date.now();
            if (wait <= 0) break;
            await new Promise(resolve => setTimeout(resolve, wait));
          }
          checkConnection();
          await writer.writeValueWithoutResponse(bytes);
          this.lastTx = Date.now();
        } else {
          for (let i = 0; i < bytes.length; i += 20) {
            checkConnection();
            const chunk = bytes.slice(i, i + 20);
            if (writer.properties.write) await writer.writeValueWithResponse(chunk);
            else await writer.writeValueWithoutResponse(chunk);
          }
        }
      } else if (transport === 'serial') {
        const output = serialPort.writable.getWriter();
        try { await output.write(bytes); } finally { output.releaseLock(); }
      }
      checkConnection();
      this.emit('sent', { aprs: label, port, payload, demo: transport === 'demo' });
    });
    this.queue = operation.catch(() => {});
    return operation;
  }
  async disconnect() {
    this.session++;
    this.transport = null;
    this.bleOptions = {}; this.lastRx = this.lastTx = 0;
    const device = this.device, notifier = this.notifier, reader = this.reader, task = this.readTask, serialPort = this.serialPort;
    this.device = this.writer = this.notifier = this.reader = this.readTask = this.serialPort = null;
    if (device) device.removeEventListener('gattserverdisconnected', this.onBleDisconnect);
    if (notifier) {
      notifier.removeEventListener('characteristicvaluechanged', this.onNotification);
      try { await notifier.stopNotifications(); } catch { /* Device may already be gone. */ }
    }
    if (device?.gatt.connected) device.gatt.disconnect();
    if (reader) { try { await reader.cancel(); } catch { /* Disconnected. */ } }
    if (task) await task;
    await this.queue;
    if (serialPort) { try { await serialPort.close(); } catch (error) { this.emit('error', error.message); } }
    this.decoder.reset(); this.emit('state', 'Disconnected');
  }
}
