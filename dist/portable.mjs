import { CHANNEL_COUNT, readChannel, writeChannel, clearChannel } from './channel.mjs';
import { SETTINGS, SETTING_GROUPS, readSetting, writeSetting } from './settings.mjs';

export const PORTABLE_FORMAT = 'td-h9-web-programmer';
export const PORTABLE_VERSION = 2;
const ARCHIVE_VERSION = 1;
const MEMORY_BLOCK_SIZE = 32;
const MEMORY_SIZE = 0x3140;
const IMAGE_SIZE = MEMORY_SIZE + 8;
const identity = [0x54, 0x44, 0x48, 0x39, 0xff, 0xff, 0xff];
const modes = { 0x4e: 'Normal', 0x48: 'HAM', 0x47: 'GMRS' };

function validImage(image) {
  if (!(image instanceof Uint8Array) || image.length !== IMAGE_SIZE ||
      !identity.every((byte, i) => image[i] === byte) || !modes[image[7]]) {
    throw new Error('Expected a complete TD-H9 Normal, HAM, or GMRS image.');
  }
}
function toHex(bytes) { return [...bytes].map(byte => byte.toString(16).padStart(2, '0')).join(''); }
function fromHex(value, expectedLength, label) {
  if (typeof value !== 'string' || value.length !== expectedLength * 2 || !/^[0-9a-f]+$/i.test(value)) throw new Error(`${label} must be ${expectedLength} bytes of hex.`);
  return Uint8Array.from({length: expectedLength}, (_, i) => Number.parseInt(value.slice(i*2, i*2+2), 16));
}
function visibleSetting(image, setting) {
  const raw = readSetting(image, setting);
  if (setting.type !== 'select' || raw === 'Uninitialized') return setting.type === 'number' || setting.type === 'u16le' ? Number(raw) : raw;
  const option = setting.options.find(([n]) => String(n) === raw);
  return option ? option[1] : `Unknown (${raw})`;
}
function inputSetting(image, setting, value) {
  if (setting.type !== 'select') return String(value);
  const option = setting.options.find(([n, label]) => label === value || String(n) === String(value));
  if (!option) throw new Error(`${setting.label}: choose a documented option.`);
  return String(option[0]);
}
function portableChannel(channel) {
  const {valid, ...fields} = channel;
  return { ...fields, tx: fields.tx ?? '' };
}

export function exportPortable(baseImage, workingImage = baseImage, {archive = false} = {}) {
  validImage(baseImage);
  validImage(workingImage);
  if (baseImage[7] !== workingImage[7]) throw new Error('The backup and edited image have different radio modes.');
  const channels = [];
  for (let n = 1; n <= CHANNEL_COUNT; n++) {
    const channel = readChannel(workingImage, n);
    if (channel.valid) channels.push(portableChannel(channel));
    else if (readChannel(baseImage, n).valid) channels.push({n, clear: true});
  }
  const settings = {};
  for (const group of SETTING_GROUPS) {
    settings[group] = {};
    for (const setting of SETTINGS.filter(s => s.section === group)) settings[group][setting.id] = visibleSetting(workingImage, setting);
  }
  const document = {
    format: PORTABLE_FORMAT,
    formatVersion: archive ? ARCHIVE_VERSION : PORTABLE_VERSION,
    model: 'TD-H9',
    mode: modes[baseImage[7]],
    kind: archive ? 'lossless-archive' : 'configuration',
    note: archive
      ? 'This archive includes every original image byte. Edit channels and settings; removed channel entries leave their original bytes unchanged.'
      : 'This is a readable configuration. Read the destination radio or open its .img backup before importing. Channel entries merge into that image; omitted channels remain unchanged. Use {"n": 1, "clear": true} to clear a channel.',
    channels,
    settings,
  };
  if (archive) {
    const memoryBlocks = [];
    for (let offset = 8; offset < IMAGE_SIZE; offset += MEMORY_BLOCK_SIZE) memoryBlocks.push(toHex(baseImage.subarray(offset, offset + MEMORY_BLOCK_SIZE)));
    document.identityHex = toHex(baseImage.subarray(0, 8));
    document.memoryBlocks = memoryBlocks;
  }
  return document;
}

export function importPortable(data, baseImage = null) {
  if (!data || typeof data !== 'object' || data.format !== PORTABLE_FORMAT || ![ARCHIVE_VERSION, PORTABLE_VERSION].includes(data.formatVersion) || data.model !== 'TD-H9') throw new Error('Unsupported TD-H9 JSON format or version.');
  let original;
  if (data.formatVersion === ARCHIVE_VERSION) {
    original = readArchiveImage(data);
  } else {
    if (!baseImage) throw new Error('Read the destination radio or open its .img backup before importing configuration JSON.');
    validImage(baseImage);
    original = baseImage.slice();
  }
  if (data.mode !== modes[original[7]]) throw new Error('JSON mode does not match the loaded radio image.');
  if (!Array.isArray(data.channels) || !data.settings || typeof data.settings !== 'object') throw new Error('JSON is missing channels or settings.');
  let edited = original.slice();
  const seen = new Set();
  for (const channel of data.channels) {
    if (!channel || !Number.isInteger(channel.n) || channel.n < 1 || channel.n > CHANNEL_COUNT || seen.has(channel.n)) throw new Error('JSON has an invalid or duplicate channel number.');
    seen.add(channel.n);
    if (channel.clear === true) { edited = clearChannel(edited, channel.n); continue; }
    const current = readChannel(edited, channel.n);
    if (JSON.stringify(portableChannel(current)) === JSON.stringify(channel)) continue;
    edited = writeChannel(edited, channel.n, channel);
  }
  for (const setting of SETTINGS) {
    const group = data.settings[setting.section];
    if (!group || !Object.hasOwn(group, setting.id)) continue;
    const wanted = group[setting.id];
    if (wanted === 'Uninitialized' || (typeof wanted === 'string' && (/^Unknown \(/.test(wanted) || wanted.startsWith('Raw: ')))) continue;
    if (wanted === visibleSetting(edited, setting)) continue;
    edited = writeSetting(edited, setting, inputSetting(edited, setting, wanted));
  }
  return {original, edited};
}

function readArchiveImage(data) {
  if (!Array.isArray(data.memoryBlocks) || data.memoryBlocks.length !== MEMORY_SIZE / MEMORY_BLOCK_SIZE) throw new Error('JSON must contain every 32-byte memory block.');
  const original = new Uint8Array(IMAGE_SIZE);
  original.set(fromHex(data.identityHex, 8, 'identityHex'));
  for (let i = 0; i < data.memoryBlocks.length; i++) original.set(fromHex(data.memoryBlocks[i], MEMORY_BLOCK_SIZE, `memoryBlocks[${i}]`), 8 + i*MEMORY_BLOCK_SIZE);
  validImage(original);
  return original;
}
