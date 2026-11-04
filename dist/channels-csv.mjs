import { CHANNEL_COUNT, CTCSS_TONES, DCS_CODES, encodeFrequency } from './channel.mjs';
import { CHANNELS_FORMAT, CHANNELS_VERSION } from './channels-file.mjs';

export const CSV_HEADERS = [
  'radio_mode', 'channel', 'action', 'name', 'rx_mhz', 'tx_mhz', 'modulation',
  'tx_tone_type', 'tx_tone', 'tx_dcs_polarity', 'rx_tone_type', 'rx_tone',
  'rx_dcs_polarity', 'power', 'scan', 'enabled', 'offset_direction', 'ptt_id',
  'scramble', 'busy_lock', 'hopping',
];
const radioModes = {0x4e:'Normal', 0x48:'HAM', 0x47:'GMRS'};
const powers = ['low', 'mid', 'high', 'unknown'];
const directions = ['none', 'minus', 'plus', 'unknown'];
const pttIds = ['off', 'beginning', 'end', 'both'];

function csvCell(value) { return `"${String(value ?? '').replaceAll('"', '""')}"`; }
function safeName(name) {
  const value = String(name ?? '');
  return /^\s*[=+\-@]/.test(value) || value.startsWith("'") ? `'${value}` : value;
}
function originalName(value) {
  return value.startsWith("'") && (value[1] === "'" || /^\s*[=+\-@]/.test(value.slice(1))) ? value.slice(1) : value;
}
function rowFor(channel, radioMode) {
  const tone = (side, part) => channel[`${side}Tone`]?.[part] ?? '';
  if (channel.clear === true) return [radioMode, channel.n, 'clear', ...Array(CSV_HEADERS.length - 3).fill('')];
  return [
    radioMode, channel.n, 'set', safeName(channel.name), channel.rx, channel.tx ?? '', channel.mode,
    tone('tx', 'type'), tone('tx', 'value'), tone('tx', 'polarity'),
    tone('rx', 'type'), tone('rx', 'value'), tone('rx', 'polarity'),
    powers[channel.power], String(!!channel.scan), String(!!channel.used),
    directions[channel.offsetDirection], pttIds[channel.pttId], channel.scramble,
    String(!!channel.busyLock), String(!!channel.hopping),
  ];
}
export function serializeChannelsCsv(document) {
  if (!document || document.format !== CHANNELS_FORMAT || document.formatVersion !== CHANNELS_VERSION ||
      document.model !== 'TD-H9' || !['Normal', 'HAM', 'GMRS'].includes(document.mode) || !Array.isArray(document.channels)) {
    throw new Error('Expected TD-H9 channel data.');
  }
  const rows = [CSV_HEADERS, ...document.channels.map(channel => rowFor(channel, document.mode))];
  return rows.map(row => row.map(csvCell).join(',')).join('\r\n') + '\r\n';
}

function parseRows(text) {
  const source = text.replace(/^\uFEFF/, '');
  const rows = [], row = [];
  let field = '', quoted = false, closed = false;
  for (let i = 0; i < source.length; i++) {
    const char = source[i];
    if (quoted) {
      if (char === '"' && source[i + 1] === '"') { field += '"'; i++; }
      else if (char === '"') { quoted = false; closed = true; }
      else field += char;
      continue;
    }
    if (char === '"' && !field && !closed) { quoted = true; continue; }
    if (char === ',') { row.push(field); field = ''; closed = false; continue; }
    if (char === '\r' || char === '\n') {
      row.push(field); field = ''; closed = false;
      if (row.length > 1 || row[0] !== '') rows.push(row.slice());
      row.length = 0;
      if (char === '\r' && source[i + 1] === '\n') i++;
      continue;
    }
    if (char === '"' || closed) throw new Error(`Malformed CSV near row ${rows.length + 1}.`);
    field += char;
  }
  if (quoted) throw new Error('CSV has an unclosed quoted field.');
  if (field || row.length || closed) { row.push(field); if (row.length > 1 || row[0] !== '') rows.push(row); }
  return rows;
}
function enumValue(value, options, label, line) {
  const index = options.indexOf(value.trim().toLowerCase());
  if (index < 0) throw new Error(`Row ${line}: ${label} must be ${options.join(', ')}.`);
  return index;
}
function booleanValue(value, label, line) {
  const normalized = value.trim().toLowerCase();
  if (['true', 'yes', 'on', '1'].includes(normalized)) return true;
  if (['false', 'no', 'off', '0'].includes(normalized)) return false;
  throw new Error(`Row ${line}: ${label} must be true or false.`);
}
function toneValue(typeCell, valueCell, polarityCell, side, line) {
  const type = typeCell.trim().toLowerCase();
  const value = valueCell.trim();
  const polarity = (polarityCell.trim() || 'N').toUpperCase();
  if (type === 'off') return {type, value:'', polarity:'N'};
  if (type === 'ctcss') {
    const number = Number(value);
    if (!Number.isFinite(number) || !CTCSS_TONES.some(t => t.toFixed(1) === number.toFixed(1))) throw new Error(`Row ${line}: unsupported ${side} CTCSS tone.`);
    return {type, value:number.toFixed(1), polarity:'N'};
  }
  if (type === 'dcs') {
    const code = value.padStart(3, '0');
    if (!DCS_CODES.includes(code) || !['N', 'R'].includes(polarity)) throw new Error(`Row ${line}: unsupported ${side} DCS code or polarity.`);
    return {type, value:code, polarity};
  }
  if (type === 'raw' && /^[0-9a-f]{4}$/i.test(value)) return {type, value:value.toLowerCase(), polarity:'N'};
  throw new Error(`Row ${line}: ${side} tone type must be off, ctcss, dcs, or an existing raw value.`);
}
function frequency(value, label, line, optional = false) {
  const trimmed = value.trim();
  if (optional && !trimmed) return '';
  try { encodeFrequency(trimmed, label); }
  catch (error) { throw new Error(`Row ${line}: ${error.message}`); }
  return Number(trimmed).toFixed(5);
}
export function parseChannelsCsv(text, image) {
  if (!(image instanceof Uint8Array) || image.length !== 0x3148 ||
      ![0x54,0x44,0x48,0x39,0xff,0xff,0xff].every((byte, index) => image[index] === byte) || !radioModes[image[7]]) {
    throw new Error('Load a TD-H9 image before importing channels.');
  }
  const rows = parseRows(text);
  if (!rows.length) throw new Error('Channels CSV is empty.');
  const headers = rows.shift().map(value => value.trim().toLowerCase());
  if (new Set(headers).size !== headers.length || CSV_HEADERS.some(header => !headers.includes(header))) throw new Error(`Channels CSV needs these columns: ${CSV_HEADERS.join(', ')}.`);
  const field = (row, key) => row[headers.indexOf(key)];
  const channels = [], seen = new Set(), mode = radioModes[image[7]];
  for (const [index, row] of rows.entries()) {
    const line = index + 2;
    if (row.length !== headers.length) throw new Error(`Row ${line}: expected ${headers.length} columns, found ${row.length}.`);
    if (field(row, 'radio_mode').trim().toLowerCase() !== mode.toLowerCase()) throw new Error(`Row ${line}: radio_mode must match the loaded ${mode} image.`);
    const numberCell = field(row, 'channel').trim();
    const n = Number(numberCell);
    if (!/^\d+$/.test(numberCell) || !Number.isInteger(n) || n < 1 || n > CHANNEL_COUNT || seen.has(n)) throw new Error(`Row ${line}: invalid or duplicate channel number (1–${CHANNEL_COUNT}).`);
    seen.add(n);
    const action = field(row, 'action').trim().toLowerCase();
    if (action === 'clear') { channels.push({n, clear:true}); continue; }
    if (action !== 'set') throw new Error(`Row ${line}: action must be set or clear.`);
    const name = originalName(field(row, 'name'));
    if (!/^[\x20-\x7e]{0,8}$/.test(name)) throw new Error(`Row ${line}: name must be at most 8 plain ASCII characters.`);
    const modulation = field(row, 'modulation').trim().toUpperCase();
    if (!['FM', 'NFM', 'AM', 'NAM'].includes(modulation)) throw new Error(`Row ${line}: modulation must be FM, NFM, AM, or NAM.`);
    const scrambleCell = field(row, 'scramble').trim(), scramble = Number(scrambleCell);
    if (!/^\d+$/.test(scrambleCell) || !Number.isInteger(scramble) || scramble < 0 || scramble > 255) throw new Error(`Row ${line}: scramble must be 0–255.`);
    channels.push({
      n, name, rx:frequency(field(row, 'rx_mhz'), 'RX', line), tx:frequency(field(row, 'tx_mhz'), 'TX', line, true),
      used:booleanValue(field(row, 'enabled'), 'enabled', line), scan:booleanValue(field(row, 'scan'), 'scan', line),
      rxTone:toneValue(field(row, 'rx_tone_type'), field(row, 'rx_tone'), field(row, 'rx_dcs_polarity'), 'RX', line),
      txTone:toneValue(field(row, 'tx_tone_type'), field(row, 'tx_tone'), field(row, 'tx_dcs_polarity'), 'TX', line),
      scramble, pttId:enumValue(field(row, 'ptt_id'), pttIds, 'ptt_id', line),
      hopping:booleanValue(field(row, 'hopping'), 'hopping', line), busyLock:booleanValue(field(row, 'busy_lock'), 'busy_lock', line),
      power:enumValue(field(row, 'power'), powers, 'power', line), mode:modulation,
      offsetDirection:enumValue(field(row, 'offset_direction'), directions, 'offset_direction', line),
    });
  }
  return {format:CHANNELS_FORMAT, formatVersion:CHANNELS_VERSION, model:'TD-H9', mode, replaceAll:false, channels};
}
