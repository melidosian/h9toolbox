import { CHANNEL_COUNT, readChannel, writeChannel, clearChannel } from './channel.mjs';

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
export function exportChannels(image) {
  checkImage(image);
  const channels = [];
  for (let n=1; n<=CHANNEL_COUNT; n++) {
    const channel = readChannel(image,n);
    if (channel.valid) channels.push(channelData(channel));
  }
  return {format:CHANNELS_FORMAT, formatVersion:CHANNELS_VERSION, model:'TD-H9', mode:modeNames[image[7]], replaceAll:false, channels};
}
export function inspectChannelImport(image, document) {
  checkImage(image);
  if (!document || document.format !== CHANNELS_FORMAT || document.formatVersion !== CHANNELS_VERSION || document.model !== 'TD-H9' || document.mode !== modeNames[image[7]] || !Array.isArray(document.channels)) {
    throw new Error('Expected a TD-H9 channels JSON file for the same radio mode.');
  }
  const seen = new Set(), conflicts = [], additions = [], removals = [], unchanged = [];
  for (const entry of document.channels) {
    if (!entry || !Number.isInteger(entry.n) || entry.n < 1 || entry.n > CHANNEL_COUNT || seen.has(entry.n)) throw new Error('Channels JSON has an invalid or duplicate channel number.');
    seen.add(entry.n);
    const current = readChannel(image,entry.n);
    if (entry.clear === true) {
      if (current.valid) removals.push(entry.n);
      else unchanged.push(entry.n);
    } else if (!current.valid) additions.push(entry.n);
    else if (JSON.stringify(channelData(current)) === JSON.stringify(entry)) unchanged.push(entry.n);
    else conflicts.push(entry.n);
  }
  if (document.replaceAll === true) {
    for (let n=1; n<=CHANNEL_COUNT; n++) if (!seen.has(n) && readChannel(image,n).valid) removals.push(n);
  }
  return {conflicts, additions, removals, unchanged};
}
export function importChannels(image, document) {
  const plan = inspectChannelImport(image, document);
  let next = image.slice();
  for (const n of [...plan.removals, ...plan.conflicts]) next = clearChannel(next,n);
  for (const entry of document.channels) {
    if (entry.clear === true || plan.unchanged.includes(entry.n)) continue;
    next = writeChannel(next,entry.n,entry);
  }
  return {image:next, ...plan};
}
