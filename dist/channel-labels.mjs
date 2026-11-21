import { CHANNEL_COUNT, readChannel } from './channel.mjs';

export const LONG_NAME_BYTES = 32;

export function checkLongName(value, label = 'Long name') {
  if (typeof value !== 'string' || !/^[\x20-\x7e]{0,32}$/.test(value)) {
    throw new Error(`${label} must be at most 32 printable ASCII characters.`);
  }
  return value;
}

export function longNameField(channel, longNames) {
  return longNames.has(channel) ? {longName: checkLongName(longNames.get(channel))} : {};
}

export function longNameChanges(baseline, edited, before, after) {
  const changes = [];
  for (let n = 1; n <= CHANNEL_COUNT; n++) {
    const hadChannel = readChannel(before, n).valid;
    const hasChannel = readChannel(after, n).valid;
    const known = baseline?.has(n) ?? false;
    const previous = baseline?.get(n) ?? '';
    const next = !hasChannel ? '' : edited.get(n) ?? previous;
    const implicit = !known && hadChannel !== hasChannel && next === '';
    if (next !== previous || implicit || (!known && hasChannel && edited.has(n))) {
      changes.push({channel: n, previous, next, implicit});
    }
  }
  return changes;
}

const HELLO = 0x3f00;
const SELECT = 0x3f20;
const DATA = 0x3f40;
const MAGIC = [0x48, 0x39, 0x4c, 0x4e]; // H9LN
const VERSION = 1;

function statusBlock(bytes) {
  if (bytes.length !== LONG_NAME_BYTES || bytes[4] !== VERSION ||
      !MAGIC.every((byte, index) => bytes[index] === byte)) return null;
  return {result: bytes[5], channel: bytes[6]};
}

export async function supportsLongNames(radio) {
  return (await readCloneCapabilities(radio)).longNames;
}

export async function readCloneCapabilities(radio) {
  const bytes = await radio.readBlock(HELLO);
  const longNames = !!statusBlock(bytes);
  return {longNames, sparseSerialWrites: longNames && !!(bytes[7] & 1)};
}

async function expectStatus(radio, channel) {
  const status = statusBlock(await radio.readBlock(HELLO));
  if (!status || status.channel !== channel || status.result !== 0) {
    throw new Error(`Channel ${channel}: radio rejected the long-name operation${status?.result ? ` (status ${status.result})` : ''}.`);
  }
}

async function selectChannel(radio, channel) {
  if (!Number.isInteger(channel) || channel < 1 || channel > CHANNEL_COUNT) {
    throw new Error(`Channel must be 1–${CHANNEL_COUNT}.`);
  }
  const selector = new Uint8Array(LONG_NAME_BYTES);
  selector.set([...MAGIC, VERSION, channel]);
  await radio.writeBlock(SELECT, selector);
  await expectStatus(radio, channel);
}

function decodeLongName(bytes) {
  if (bytes.length !== LONG_NAME_BYTES) throw new Error('Incomplete channel long-name reply.');
  const zero = bytes.indexOf(0);
  const end = zero < 0 ? bytes.length : zero;
  if (zero >= 0 && bytes.subarray(zero).some(value => value !== 0)) {
    throw new Error('Invalid channel long-name padding.');
  }
  return checkLongName(String.fromCharCode(...bytes.subarray(0, end)));
}

export async function readLongName(radio, channel) {
  await selectChannel(radio, channel);
  const name = decodeLongName(await radio.readBlock(DATA));
  await expectStatus(radio, channel);
  return name;
}

export async function writeLongName(radio, channel, value) {
  const name = checkLongName(value);
  const data = new Uint8Array(LONG_NAME_BYTES);
  data.set(Array.from(name, character => character.charCodeAt(0)));
  await selectChannel(radio, channel);
  await radio.writeBlock(DATA, data);
  await expectStatus(radio, channel);
  const verified = decodeLongName(await radio.readBlock(DATA));
  await expectStatus(radio, channel);
  if (verified !== name) throw new Error(`Channel ${channel}: long-name readback differs.`);
}
