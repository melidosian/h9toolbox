import { CHANNEL_COUNT, readChannel, writeChannel, clearChannel } from './channel.mjs';
import { checkLongName, longNameField } from './channel-labels.mjs';

export const CHANNELS_FORMAT = 'td-h9-channels';
export const CHANNELS_VERSION = 1;
const IMAGE_SIZE = 0x3148;
const modeNames = {0x4e:'Normal',0x48:'HAM',0x47:'GMRS'};

function checkImage(image) {
  if (!(image instanceof Uint8Array) || image.length !== IMAGE_SIZE ||
      ![0x54,0x44,0x48,0x39,0xff,0xff,0xff].every((b,i)=>image[i]===b) || !modeNames[image[7]]) {
    throw new Error('Load a TD-H9 image before working with channels.');
  }
}
function channelData(channel) {
  const {valid, ...data} = channel;
  return {...data, tx: data.tx ?? ''};
}
export function exportChannels(image, longNames = new Map()) {
  checkImage(image);
  const channels = [];
  for (let n=1; n<=CHANNEL_COUNT; n++) {
    const channel = readChannel(image,n);
    if (channel.valid) channels.push({...channelData(channel), ...longNameField(n, longNames)});
  }
  return {format:CHANNELS_FORMAT, formatVersion:CHANNELS_VERSION, model:'TD-H9', mode:modeNames[image[7]], replaceAll:false, channels};
}
function sameChannelData(current, entry) {
  const {longName, ...nativeEntry} = entry;
  return JSON.stringify(channelData(current)) === JSON.stringify(nativeEntry);
}

export function inspectChannelImport(image, document, longNames = new Map()) {
  checkImage(image);
  if (!document || document.format !== CHANNELS_FORMAT || document.formatVersion !== CHANNELS_VERSION || document.model !== 'TD-H9' || document.mode !== modeNames[image[7]] || !Array.isArray(document.channels)) {
    throw new Error('Expected TD-H9 channel data for the same radio mode.');
  }
  const seen = new Set(), conflicts = [], additions = [], removals = [], unchanged = [];
  for (const entry of document.channels) {
    if (!entry || !Number.isInteger(entry.n) || entry.n < 1 || entry.n > CHANNEL_COUNT || seen.has(entry.n)) throw new Error('Channels file has an invalid or duplicate channel number.');
    seen.add(entry.n);
    if (Object.hasOwn(entry, 'longName')) checkLongName(entry.longName, `Channel ${entry.n} long name`);
    const current = readChannel(image,entry.n);
    if (entry.clear === true) {
      if (current.valid) removals.push(entry.n);
      else unchanged.push(entry.n);
    } else if (!current.valid) additions.push(entry.n);
    else if (sameChannelData(current, entry) && (!Object.hasOwn(entry, 'longName') ||
      checkLongName(entry.longName) === (longNames.get(entry.n) ?? ''))) unchanged.push(entry.n);
    else conflicts.push(entry.n);
  }
  if (document.replaceAll === true) {
    for (let n=1; n<=CHANNEL_COUNT; n++) if (!seen.has(n) && readChannel(image,n).valid) removals.push(n);
  }
  return {conflicts, additions, removals, unchanged};
}
export function importChannels(image, document, longNames = new Map()) {
  const plan = inspectChannelImport(image, document, longNames);
  let next = image.slice();
  const nextNames = new Map(longNames);
  for (const n of plan.removals) {
    next = clearChannel(next, n);
    nextNames.set(n, '');
  }
  for (const entry of document.channels) {
    if (entry.clear === true) {
      nextNames.set(entry.n, '');
      continue;
    }
    if (Object.hasOwn(entry, 'longName')) nextNames.set(entry.n, checkLongName(entry.longName));
    const current = readChannel(image, entry.n);
    if (current.valid && sameChannelData(current, entry)) continue;
    if (current.valid) next = clearChannel(next, entry.n);
    next = writeChannel(next, entry.n, entry);
  }
  return {image:next, longNames:nextNames, ...plan};
}
