export const IMAGE_SIZE = 17304;
const BLOCK_SIZE = 128;
const BAUD = 38400;
const STARTUP_QUIET_MS = 20;
const TIMEOUT_MS = 3000;
const WRITE_ACK = Uint8Array.of(0x54, 0x49, 0x44, 0, 1, 0x13, 0x14);
const EXIT_ACK = Uint8Array.of(0x54, 0x49, 0x44, 0, 1, 0x10, 0x11);
const EXIT_FRAME = Uint8Array.of(0x54, 0x49, 0x44, 0, 1, 0xff, 0);

export const RADIO_FILTER = Object.freeze({ usbVendorId: 0x4c4a, usbProductId: 0x4155 });
export const STOCK_URL = 'https://raw.githubusercontent.com/teedubyeah/TDH9/1ccb67c2684204b0097533486c545607caaf65b8/APRS/APRS-1.0.15/TID-H9-APRS-update_421f0916-6de9-4e79-9e35-bdb248c70e2b/APRS-1.0.15.bin';
export const IMAGES = Object.freeze({
  stock: Object.freeze({
    label: 'Stock APRS 1.0.15',
    filename: 'APRS-1.0.15.bin',
    sha256: '1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33',
  }),
  packet: Object.freeze({
    label: 'APRS 1.0.15 for the KISS TNC',
    filename: 'APRS-1.0.15-rx15-rxff.bin',
    sha256: '3a70e2f48283cfd0d06fb29a08f462fc0b8302898e09d41946d5f74205daf741',
  }),
});

function hex(bytes, separator = '') {
  return Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join(separator);
}

export function isAprsRadio(port) {
  const { usbVendorId, usbProductId } = port.getInfo();
  return usbVendorId === RADIO_FILTER.usbVendorId && usbProductId === RADIO_FILTER.usbProductId;
}

export async function verifyImage(bytes, kind) {
  if (!Object.hasOwn(IMAGES, kind) || bytes.length !== IMAGE_SIZE ||
      hex(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes))) !== IMAGES[kind].sha256) {
    throw new Error('The APRS image does not match the verified release file.');
  }
}

export function writeFrame(offset, payload) {
  const frame = new Uint8Array(11 + payload.length);
  frame.set([0x54, 0x49, 0x44, 0, payload.length + 5, 3]);
  new DataView(frame.buffer).setUint32(6, offset);
  frame.set(payload, 10);
  frame[frame.length - 1] = frame.subarray(3).reduce((sum, byte) => sum + byte, 0) & 0xff;
  return frame;
}

async function withDeadline(promise, ms, timeout) {
  let timer;
  try {
    return await Promise.race([promise, new Promise(resolve => {
      timer = setTimeout(() => resolve(timeout), Math.max(0, ms));
    })]);
  } finally {
    clearTimeout(timer);
  }
}

class SerialInput {
  constructor(reader) {
    this.reader = reader;
    this.pending = null;
  }

  async read(ms) {
    this.pending ??= this.reader.read();
    const result = await withDeadline(this.pending, ms, null);
    if (result === null) return null;
    this.pending = null;
    if (result.done) throw new Error('The radio closed its USB serial connection.');
    return result.value;
  }

  async drain(timeoutMs) {
    const deadline = performance.now() + timeoutMs;
    let discarded = 0;
    while (deadline - performance.now() >= STARTUP_QUIET_MS) {
      const incoming = await this.read(STARTUP_QUIET_MS);
      if (incoming === null) return discarded;
      discarded += incoming.length;
    }
    throw new Error('The USB serial input did not go quiet before the first write.');
  }

  async readAck(expected, timeoutMs) {
    const deadline = performance.now() + timeoutMs;
    const reply = new Uint8Array(expected.length);
    let length = 0;
    while (length < reply.length) {
      const remaining = deadline - performance.now();
      const incoming = remaining > 0 ? await this.read(remaining) : null;
      if (incoming === null) {
        const detail = length ? ` after ${length} of ${reply.length} bytes (${hex(reply.subarray(0, length), ' ')})` : '';
        throw new Error(`Timed out waiting for an APRS acknowledgment${detail}.`);
      }
      if (incoming.length > reply.length - length) {
        throw new Error('Unexpected extra bytes followed the APRS acknowledgment.');
      }
      reply.set(incoming, length);
      length += incoming.length;
    }
    if (!reply.every((byte, index) => byte === expected[index])) {
      throw new Error(`Unexpected APRS acknowledgment (${hex(reply, ' ')}).`);
    }
    return hex(reply, ' ');
  }
}

export async function flashAprs(port, image, kind, { onEvent = () => {}, timeoutMs = TIMEOUT_MS } = {}) {
  if (!isAprsRadio(port)) throw new Error('Select the TD-H9 USB serial port.');
  const bytes = Uint8Array.from(image);
  await verifyImage(bytes, kind);
  onEvent({ event: 'attempt_started', baud: BAUD, image_bytes: bytes.length, image_sha256: IMAGES[kind].sha256 });
  let opened = false;
  let reader;
  let writer;
  let acknowledged = 0;
  try {
    // The entire transfer fits in this Chromium serial buffer. Some H9 USB
    // handlers lose requests that straddle the browser's smaller default buffer.
    await port.open({ baudRate: BAUD, dataBits: 8, stopBits: 1, parity: 'none',
      flowControl: 'none', bufferSize: 64 * 1024 });
    opened = true;
    // Web Serial exposes the modem control lines only after opening the port.
    await port.setSignals({ dataTerminalReady: true, requestToSend: true });
    reader = port.readable.getReader();
    writer = port.writable.getWriter();
    const input = new SerialInput(reader);
    const discarded = await input.drain(timeoutMs);
    onEvent({ event: 'startup_drained', discarded_bytes: discarded, quiet_ms: STARTUP_QUIET_MS });

    async function send(frame, expected) {
      if (!await withDeadline(writer.write(frame).then(() => true), timeoutMs, false)) {
        throw new Error('Timed out writing to the USB serial port.');
      }
      return input.readAck(expected, timeoutMs);
    }

    for (let offset = 0; offset < bytes.length; offset += BLOCK_SIZE) {
      const payload = bytes.subarray(offset, offset + BLOCK_SIZE);
      const reply = await send(writeFrame(offset, payload), WRITE_ACK);
      acknowledged = offset + payload.length;
      onEvent({ event: 'block_acknowledged', offset, length: payload.length,
        acknowledged_end: acknowledged, reply_hex: reply });
    }
    const reply = await send(EXIT_FRAME, EXIT_ACK);
    onEvent({ event: 'exit_acknowledged', acknowledged_end: acknowledged, reply_hex: reply });
  } catch (error) {
    onEvent({ event: 'attempt_failed', acknowledged_end: acknowledged, error: error.message });
    throw error;
  } finally {
    if (reader) {
      await reader.cancel().catch(() => {});
      reader.releaseLock();
    }
    writer?.releaseLock();
    if (opened) {
      await port.close().catch(error => onEvent({ event: 'close_error', error: error.message }));
    }
  }
}
