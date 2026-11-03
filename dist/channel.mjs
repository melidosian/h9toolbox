// TD-H9 channel-memory codec. All offsets below are radio-memory offsets;
// an .img file has an eight-byte model/mode prefix before radio memory.
export const CHANNEL_COUNT = 199;
export const CTCSS_TONES = [
  67.0, 69.3, 71.9, 74.4, 77.0, 79.7, 82.5, 85.4, 88.5, 91.5,
  94.8, 97.4, 100.0, 103.5, 107.2, 110.9, 114.8, 118.8, 123.0,
  127.3, 131.8, 136.5, 141.3, 146.2, 151.4, 156.7, 159.8,
  162.2, 165.5, 167.9, 171.3, 173.8, 177.3, 179.9, 183.5,
  186.2, 189.9, 192.8, 196.6, 199.5, 203.5, 206.5, 210.7,
  218.1, 225.7, 229.1, 233.6, 241.8, 250.3, 254.1,
];
export const DCS_CODES = [
  23,25,26,31,32,36,43,47,51,53,54,65,71,72,73,74,114,115,116,
  122,125,131,132,134,143,145,152,155,156,162,165,172,174,205,
  212,223,225,226,243,244,245,246,251,252,255,261,263,265,266,
  271,274,306,311,315,325,331,332,343,346,351,356,364,365,371,
  411,412,413,423,431,432,445,446,452,454,455,462,464,465,466,
  503,506,516,523,526,532,546,565,606,612,624,627,631,632,654,
  662,664,703,712,723,731,732,734,743,754,
].map(n => String(n).padStart(3, '0'));

const PREFIX = 8;
const RECORD_SIZE = 16;
const NAME_OFFSET = 0x0d40;
const USED_OFFSET = 0x1900;
const SCAN_OFFSET = 0x1920;

function checkNumber(n) {
  if (!Number.isInteger(n) || n < 1 || n > CHANNEL_COUNT) throw new RangeError('Channel must be 1–199.');
}
function recordOffset(n) { checkNumber(n); return PREFIX + n * RECORD_SIZE; }
function nameOffset(n) { checkNumber(n); return PREFIX + NAME_OFFSET + (n - 1) * 8; }
function flag(image, offset, n) {
  const index = n - 1;
  return !!(image[PREFIX + offset + (index >> 3)] & (1 << (index & 7)));
}
function setFlag(image, offset, n, value) {
  const index = n - 1, at = PREFIX + offset + (index >> 3), mask = 1 << (index & 7);
  image[at] = value ? image[at] | mask : image[at] & ~mask;
}
function decodeBcd(bytes) {
  let value = 0, place = 1;
  for (const byte of bytes) {
    const ones = byte & 15, tens = byte >> 4;
    if (ones > 9 || tens > 9) return null;
    value += (tens * 10 + ones) * place;
    place *= 100;
  }
  return value;
}
function encodeBcd(value, length) {
  const out = new Uint8Array(length);
  for (let i = 0; i < length; i++) {
    const pair = value % 100;
    out[i] = (Math.floor(pair / 10) << 4) | (pair % 10);
    value = Math.floor(value / 100);
  }
  if (value) throw new Error('Value is too large for the radio field.');
  return out;
}
export function decodeFrequency(bytes) {
  if (bytes[0] === 0xff || bytes.every(byte => byte === 0)) return null;
  const tenHz = decodeBcd(bytes);
  return tenHz === null ? null : (tenHz / 100000).toFixed(5);
}
export function encodeFrequency(input, label = 'Frequency') {
  const value = Number(String(input).trim());
  if (!Number.isFinite(value) || value < 18 || value > 600) throw new Error(`${label} must be between 18 and 600 MHz.`);
  const tenHz = Math.round(value * 100000);
  if (Math.abs(value * 100000 - tenHz) > 0.001) throw new Error(`${label} must be a multiple of 10 Hz.`);
  return encodeBcd(tenHz, 4);
}
function decodeTone(bytes) {
  const [lo, hi] = bytes;
  if ((lo === 0xff && hi === 0xff) || (lo === 0 && hi === 0)) return { type: 'off', value: '', polarity: 'N' };
  const flags = hi & 0xc0;
  if (flags === 0x80 || flags === 0xc0) {
    const digits = [hi & 15, lo >> 4, lo & 15];
    if (digits.some(d => d > 7)) return { type: 'raw', value: [...bytes].map(b => b.toString(16).padStart(2, '0')).join(''), polarity: 'N' };
    return { type: 'dcs', value: digits.join(''), polarity: flags === 0xc0 ? 'R' : 'N' };
  }
  const tenthHz = decodeBcd(bytes);
  if (tenthHz === null || tenthHz < 500 || tenthHz > 3000) return { type: 'raw', value: [...bytes].map(b => b.toString(16).padStart(2, '0')).join(''), polarity: 'N' };
  return { type: 'ctcss', value: (tenthHz / 10).toFixed(1), polarity: 'N' };
}
function encodeTone(tone) {
  if (tone.type === 'off') return Uint8Array.of(0xff, 0xff);
  if (tone.type === 'ctcss') {
    const value = Number(tone.value);
    if (!CTCSS_TONES.some(n => n.toFixed(1) === value.toFixed(1))) throw new Error(`Unsupported CTCSS tone: ${tone.value}.`);
    return encodeBcd(Math.round(value * 10), 2);
  }
  if (tone.type === 'dcs') {
    const code = String(tone.value).padStart(3, '0');
    if (!DCS_CODES.includes(code)) throw new Error(`Unsupported DCS code: ${code}.`);
    const bytes = encodeBcd(Number(code), 2);
    bytes[1] |= tone.polarity === 'R' ? 0xc0 : 0x80;
    return bytes;
  }
  throw new Error('Select a supported tone mode.');
}
function toneKey(tone) { return `${tone.type}|${tone.value}|${tone.type === 'dcs' ? tone.polarity : ''}`; }

export function readChannel(image, n) {
  const at = recordOffset(n), nameAt = nameOffset(n);
  const record = image.subarray(at, at + RECORD_SIZE);
  const nameBytes = image.subarray(nameAt, nameAt + 8);
  let end = nameBytes.findIndex(b => b === 0 || b === 0xff);
  if (end < 0) end = 8;
  const name = String.fromCharCode(...nameBytes.subarray(0, end)).trimEnd();
  const rx = decodeFrequency(record.subarray(0, 4));
  const tx = decodeFrequency(record.subarray(4, 8));
  const narrow = !!(record[14] & 0x08), am = !!(record[15] & 0x01);
  return {
    n, name, rx, tx,
    valid: rx !== null,
    used: flag(image, USED_OFFSET, n),
    scan: flag(image, SCAN_OFFSET, n),
    rxTone: decodeTone(record.subarray(8, 10)),
    txTone: decodeTone(record.subarray(10, 12)),
    scramble: record[12],
    pttId: (record[13] >> 6) & 3,
    hopping: !!(record[13] & 0x20),
    busyLock: !!(record[13] & 0x04),
    power: (record[14] >> 4) & 3,
    mode: `${narrow ? 'N' : ''}${am ? 'AM' : 'FM'}`,
    offsetDirection: record[14] & 3,
  };
}
export function writeChannel(image, n, form) {
  const current = readChannel(image, n);
  const rx = encodeFrequency(form.rx, 'RX');
  const tx = String(form.tx).trim() ? encodeFrequency(form.tx, 'TX') : Uint8Array.of(0xff, 0xff, 0xff, 0xff);
  const name = String(form.name ?? '').trim();
  if (!/^[\x20-\x7e]{0,8}$/.test(name)) throw new Error('Name must be 8 or fewer plain ASCII characters.');
  if (!['FM', 'NFM', 'AM', 'NAM'].includes(form.mode)) throw new Error('Select a valid modulation/bandwidth.');
  const power = Number(form.power), pttId = Number(form.pttId), direction = Number(form.offsetDirection), scramble = Number(form.scramble);
  if (![0, 1, 2, 3].includes(power) || ![0, 1, 2, 3].includes(pttId) || ![0, 1, 2, 3].includes(direction) || !Number.isInteger(scramble) || scramble < 0 || scramble > 255) throw new Error('Invalid channel option.');
  const rxTone = !current.valid || toneKey(form.rxTone) !== toneKey(current.rxTone) ? encodeTone(form.rxTone) : null;
  const txTone = !current.valid || toneKey(form.txTone) !== toneKey(current.txTone) ? encodeTone(form.txTone) : null;
  const at = recordOffset(n), nameAt = nameOffset(n);
  const next = image.slice();
  if (!current.valid) next.fill(0, at, at + RECORD_SIZE);
  if (!current.valid || form.rx !== current.rx) next.set(rx, at);
  if (!current.valid || form.tx !== (current.tx ?? '')) next.set(tx, at + 4);
  if (!current.valid || name !== current.name) {
    next.fill(0, nameAt, nameAt + 8);
    for (let i = 0; i < name.length; i++) next[nameAt + i] = name.charCodeAt(i);
  }
  if (rxTone) next.set(rxTone, at + 8);
  if (txTone) next.set(txTone, at + 10);
  if (!current.valid || scramble !== current.scramble) next[at + 12] = scramble;
  next[at + 13] = (next[at + 13] & ~0xe4) | (pttId << 6) | (form.hopping ? 0x20 : 0) | (form.busyLock ? 0x04 : 0);
  next[at + 14] = (next[at + 14] & ~0x3b) | (power << 4) | (form.mode.startsWith('N') ? 0x08 : 0) | direction;
  next[at + 15] = (next[at + 15] & ~0x01) | (form.mode.includes('AM') ? 0x01 : 0);
  setFlag(next, USED_OFFSET, n, !!form.used);
  setFlag(next, SCAN_OFFSET, n, !!form.scan);
  return next;
}
export function clearChannel(image, n) {
  const next = image.slice(), at = recordOffset(n), nameAt = nameOffset(n);
  next.fill(0xff, at, at + RECORD_SIZE);
  next.fill(0xff, nameAt, nameAt + 8);
  setFlag(next, USED_OFFSET, n, false);
  setFlag(next, SCAN_OFFSET, n, false);
  return next;
}
