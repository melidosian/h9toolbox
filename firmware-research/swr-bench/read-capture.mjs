import { BLOCK } from '../../dist/boot-logo/resource.mjs';

export const CAPTURE_ADDRESS = 0x3c00;
export const ADC_IDS = [0, 3, 4, 10, 13];
const MAGIC = [0x48, 0x39, 0x53, 0x57]; // H9SW
const HEADER_BYTES = BLOCK;
const RECORD_BYTES = 16;
const PRETRIGGER = 16;
const CAPTURE_LIMIT = 84;
const MILLISECONDS_PER_TICK = 10;
const MISSING_SAMPLE = 0xffff;

export function parseCaptureHeader(bytes) {
  if (bytes.byteLength !== HEADER_BYTES) throw new Error('Incomplete capture header.');
  if (!MAGIC.every((byte, index) => bytes[index] === byte)) {
    throw new Error('The radio did not return an SWR bench header. Check the installed firmware.');
  }
  if (bytes[4] !== 1 || bytes[9] !== RECORD_BYTES) {
    throw new Error('Unsupported SWR bench capture format.');
  }

  const [status, count, preHead, preCount] = bytes.subarray(5, 9);
  // The firmware serves header bytes individually. The cursor and count can
  // change between bytes until sampling has stopped.
  const validFrozen = count > 0 && count <= CAPTURE_LIMIT && preHead < PRETRIGGER &&
    preCount <= PRETRIGGER && (preCount === PRETRIGGER || preHead === preCount);
  if (status > 3 || (status >= 2 && !validFrozen)) {
    throw new Error('Invalid SWR bench capture header.');
  }
  return { status, count, preHead, preCount };
}

export function decodeCapture(bytes, header) {
  const { count, preHead, preCount } = header;
  const neededBytes = (PRETRIGGER + count) * RECORD_BYTES;
  if (bytes.byteLength < neededBytes) throw new Error('Incomplete capture records.');

  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const triggerTick = view.getUint32(PRETRIGGER * RECORD_BYTES, true);
  const firstPre = (preHead + PRETRIGGER - preCount) % PRETRIGGER;
  const rows = [];
  for (let index = 0; index < preCount + count; index++) {
    const record = index < preCount ? (firstPre + index) % PRETRIGGER : PRETRIGGER + index - preCount;
    const offset = record * RECORD_BYTES;
    const tick = view.getUint32(offset, true);
    const relativeMs = ((tick - triggerTick) | 0) * MILLISECONDS_PER_TICK;
    const txFlag = view.getUint8(offset + 14);
    if ((rows.length && relativeMs <= rows.at(-1).relativeMs) ||
        (index === preCount && txFlag !== 1)) {
      throw new Error('Capture timestamps or transmit flags are inconsistent.');
    }
    const adc = ADC_IDS.map((_, channel) => {
      const sample = view.getUint16(offset + 4 + 2 * channel, true);
      return sample === MISSING_SAMPLE ? null : sample;
    });
    rows.push({ tick, relativeMs, adc, txFlag, cursor: view.getUint8(offset + 15) });
  }
  return rows;
}

export async function readCapture(radio, onProgress = () => {}) {
  try {
    await radio.enterProgramming();
    const headerBytes = await radio.readBlock(CAPTURE_ADDRESS);
    const header = parseCaptureHeader(headerBytes);
    if (header.status < 2) return { ...header, rows: [] };

    const required = (PRETRIGGER + header.count) * RECORD_BYTES;
    const length = Math.ceil(required / BLOCK) * BLOCK;
    const bytes = new Uint8Array(length);
    for (let offset = 0; offset < length; offset += BLOCK) {
      const block = await radio.readBlock(CAPTURE_ADDRESS + HEADER_BYTES + offset);
      if (block.byteLength !== BLOCK) throw new Error('Incomplete capture block.');
      bytes.set(block, offset);
      onProgress((offset + BLOCK) / length);
    }
    const latest = await radio.readBlock(CAPTURE_ADDRESS);
    if (latest.byteLength !== headerBytes.byteLength ||
        !headerBytes.every((byte, index) => byte === latest[index])) {
      throw new Error('Capture changed during readout. Read the radio again.');
    }
    return { ...header, rows: decodeCapture(bytes, header) };
  } finally {
    await radio.leaveProgramming();
  }
}

export function captureCsv(rows) {
  const columns = ['relative_ms', 'tick_100hz', 'tx_flag', 'adc_cursor', ...ADC_IDS.map(id => `adc_${id}`)];
  return [columns.join(','), ...rows.map(({ relativeMs, tick, txFlag, cursor, adc }) =>
    [relativeMs, tick, txFlag, cursor, ...adc].join(','))].join('\r\n') + '\r\n';
}
