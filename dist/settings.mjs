// TD-H9 image offsets include the eight-byte identity. Values not described here
// are deliberately left untouched. Layout: CHIRP TD-H9 test driver (2026-10-31).
const opt = (labels, start = 0) => labels.map((label, i) => [start + i, label]);
const yesNo = [[0, 'Off'], [1, 'On']];
const steps = opt(['2.5 kHz', '5 kHz', '6.25 kHz', '10 kHz', '12.5 kHz', '25 kHz', '50 kHz', '0.5 kHz', '8.33 kHz']);
const colors = opt(['Blue', 'Red', 'Green', 'Yellow', 'Purple', 'Orange', 'Light blue', 'Cyan', 'Gray', 'Dark blue', 'Light green', 'Brown', 'Pink', 'Bright red', 'Green blue', 'Light gray', 'LG blue', 'LB blue']);
const shortButtons = opt(['None', 'FM radio', 'GNSS switch', 'None (3)', 'Tone', 'Alarm', 'Weather', 'PTT2', 'OD PTT']);
const longButtons = opt(['None', 'FM radio', 'GNSS switch', 'Cancel squelch', 'Tone', 'Alarm', 'Weather']);
const add = (section, id, label, offset, options, extra = {}) => ({ section, id, label, offset, type: 'select', options, ...extra });
const bit = (section, id, label, offset, mask, shift = 0, options = yesNo) => add(section, id, label, offset, options, { mask, shift });
const num = (section, id, label, offset, min, max, extra = {}) => ({ section, id, label, offset, type: 'number', min, max, ...extra });
const str = (section, id, label, offset, length, extra = {}) => ({ section, id, label, offset, type: 'text', length, ...extra });

export const SETTING_GROUPS = [
  'Radio', 'VFO & scan', 'Display & buttons', 'Bluetooth', 'SMS & Morse',
  'DTMF', 'GNSS', 'APRS identity', 'APRS beacon', 'APRS relay & routing',
];

export const SETTINGS = [
  add('Radio','squelch','Squelch',0x0cb1,opt(Array.from({length:10},(_,i)=>String(i)))),
  bit('Radio','voicePrompt','Voice prompt',0x0ca9,0x01),
  bit('Radio','keyLock','Auto keypad lock',0x0ca9,0x10,4),
  bit('Radio','beep','Key beep',0x0ca9,0x04,2),
  bit('Radio','txLight','Display on TX',0x0ca8,0x80,7),
  bit('Radio','rxLight','Display on RX',0x0ca8,0x40,6),
  bit('Radio','dualWatch','Dual watch / sync',0x0caa,0x40,6),
  add('Radio','voxGain','VOX gain',0x0caf,opt(['Off','1','2','3','4','5']),{mask:0x07,shift:0}),
  add('Radio','voxDelay','VOX delay',0x0cb6,opt(['1 s','2 s','3 s'])),
  add('Radio','backlight','Backlight timer',0x0cb5,opt(['Continuous','5 s','10 s','15 s','30 s'])),
  add('Radio','brightness','Display brightness',0x0ca5,[[4,'Level 1'],[3,'Level 2'],[2,'Level 3'],[1,'Level 4'],[0,'Level 5']]),
  add('Radio','breathLed','Breathing LED',0x0cb7,opt(['Off','5 s','10 s','15 s','30 s']),{mask:0x70,shift:4}),
  add('Radio','timeout','TX timeout',0x0cb2,opt(['Off','30 s','60 s','90 s','120 s','150 s','180 s','210 s'])),
  add('Radio','roger','Roger beep',0x0cb3,opt(['Off','Tone 1','Tone 2']),{mask:0xc0,shift:6}),
  add('Radio','alarm','Alarm mode',0x0cb7,opt(['Local','TX alarm']),{mask:0x01,shift:0}),
  add('Radio','repeaterTone','Repeater tone',0x0caa,opt(['1000 Hz','1450 Hz','1750 Hz','2100 Hz']),{mask:0x30,shift:4}),
  add('Radio','micGain','Microphone gain',0x1f28,opt(Array.from({length:10},(_,i)=>String(i)))),
  add('Radio','powerSave','Battery saver',0x0cb4,opt(['Off','1:1','1:2','1:3','1:4'])),
  bit('Radio','tailClean','Squelch tail elimination',0x0caf,0x40,6),
  bit('Radio','talkAround','Talk around',0x3014,0x01),
  add('Radio','callCode','Call code',0x3015,opt(Array.from({length:8},(_,i)=>String(i+1)))),
  add('Radio','pttDelay','PTT delay',0x3013,opt(Array.from({length:30},(_,i)=>`${(i+1)*100} ms`)),{mask:0x1f,shift:0}),

  add('VFO & scan','scanMode','Scan resume',0x0ca9,opt(['Time','Carrier','Stop']),{mask:0xc0,shift:6}),
  add('VFO & scan','scanHang','Scan hang time',0x1f37,opt(Array.from({length:20},(_,i)=>`${(i+1)/2} s`))),
  add('VFO & scan','aMode','VFO A mode',0x0caa,opt(['VFO','VFO + channel','Channel']),{mask:0x03,shift:0}),
  add('VFO & scan','bMode','VFO B mode',0x0cab,opt(['VFO','VFO + channel','Channel']),{mask:0x03,shift:0}),
  add('VFO & scan','aStep','VFO A step',0x0cb0,steps,{mask:0xf0,shift:4}),
  add('VFO & scan','bStep','VFO B step',0x0cb0,steps,{mask:0x0f,shift:0}),
  num('VFO & scan','aChannel','VFO A default channel',0x0cac,1,199),
  num('VFO & scan','bChannel','VFO B default channel',0x0cad,1,199),
  add('VFO & scan','fmMode','Broadcast FM mode',0x0caa,opt(['VFO','Channel']),{mask:0x80,shift:7}),
  num('VFO & scan','fmChannel','Broadcast FM channel',0x0cae,1,25),
  bit('VFO & scan','fmInterrupt','Broadcast FM interrupt',0x0caa,0x08,3),
  num('VFO & scan','rangerHigh','Scan upper limit (MHz)',0x1f33,0,999,{type:'u16le'}),
  num('VFO & scan','rangerLow','Scan lower limit (MHz)',0x1f35,0,999,{type:'u16le'}),

  add('Display & buttons','language','Menu language',0x1f30,opt(['English','Chinese','Turkish','Russian','German','Spanish','Italian','French','Thai'])),
  add('Display & buttons','display','Display layout',0x1f31,opt(['Single','Dual','Classic'])),
  add('Display & buttons','color','Menu color',0x1f32,colors),
  add('Display & buttons','aLabel','VFO A label',0x0caa,opt(['Frequency + number','Name + number']),{mask:0x04,shift:2}),
  add('Display & buttons','bLabel','VFO B label',0x0cab,opt(['Frequency + number','Name + number']),{mask:0x10,shift:4}),
  add('Display & buttons','powerOnType','Power-on display',0x0cab,opt(['Voltage','Message','Picture']),{mask:0xc0,shift:6}),
  str('Display & buttons','powerLine1','Power-on line 1',0x1c08,16),
  str('Display & buttons','powerLine2','Power-on line 2',0x1c18,16),
  str('Display & buttons','powerLine3','Power-on line 3',0x1c28,16),
  add('Display & buttons','topShort','Top short press',0x0c98,shortButtons),
  add('Display & buttons','pf1Short','PF1 short press',0x0c99,shortButtons),
  add('Display & buttons','pf2Short','PF2 short press',0x0c9a,shortButtons),
  add('Display & buttons','topLong','Top long press',0x0c9b,longButtons),
  add('Display & buttons','pf1Long','PF1 long press',0x0c9c,longButtons),
  add('Display & buttons','pf2Long','PF2 long press',0x0c9d,longButtons),

  add('Bluetooth','btOn','Bluetooth extended mode',0x1f29,yesNo),
  add('Bluetooth','btMode','Bluetooth role',0x1f2a,opt(['Receiver','Emitter'])),
  add('Bluetooth','btMic','Bluetooth mic gain',0x1f2b,opt(['Level 1','Level 2','Level 3','Level 4','Level 5'])),
  add('Bluetooth','btSpeaker','Bluetooth speaker gain',0x1f2c,opt(['Level 1','Level 2','Level 3','Level 4','Level 5'])),
  add('Bluetooth','odPtt','OD PTT',0x1f2d,opt(['OD','OD + analog'])),
  add('Bluetooth','odMode','OD mode',0x1f2e,opt(['Local','Forward','Full'])),

  add('SMS & Morse','smsOn','SMS',0x301d,yesNo),
  add('SMS & Morse','smsMode','SMS transport',0x301e,opt(['RF','APRS'])),
  add('SMS & Morse','smsFilter','SMS RX filter',0x301f,yesNo),
  bit('SMS & Morse','morseOn','Morse',0x302a,0x01),
  add('SMS & Morse','morseTone','Morse tone',0x302a,opt(['500 Hz','600 Hz','700 Hz','800 Hz','1000 Hz']),{mask:0xf0,shift:4}),
  add('SMS & Morse','morseSpeed','Morse speed',0x302a,opt(['5 WPM','8 WPM','12 WPM','18 WPM','24 WPM']),{mask:0x0e,shift:1}),
  add('SMS & Morse','morseKey','Morse key',0x302b,opt(['Straight','Iambic']),{mask:0xc0,shift:6}),
  add('SMS & Morse','morsePower','Morse power',0x302b,opt(['Low','Mid','High']),{mask:0x30,shift:4}),
  add('SMS & Morse','morseThreshold','Morse RX threshold',0x302b,opt(['Weak','Medium-weak','Medium','Medium-strong','Strong']),{mask:0x0f,shift:0}),

  bit('DTMF','decode','DTMF decode',0x0ca0,0x01),
  add('DTMF','autoReset','DTMF auto reset',0x0ca1,opt(['Off','5 s','10 s','15 s']),{mask:0x03,shift:0}),
  add('DTMF','decodeResponse','DTMF response',0x0ca2,opt(['None','Ring','Reply','Ring + reply']),{mask:0x03,shift:0}),
  add('DTMF','dtmfSpeed','DTMF speed',0x0ca3,opt(Array.from({length:8},(_,i)=>`${80+i*10} ms`)),{mask:0x07,shift:0}),
  bit('DTMF','sidetone','DTMF sidetone',0x0ca8,0x02,1),

  add('GNSS','gnssOn','GNSS',0x306e,yesNo),
  add('GNSS','positionFormat','Coordinate format',0x306f,opt(['Degrees','Degrees + minutes','Degrees + minutes + seconds'])),
  add('GNSS','timeZone','Time zone',0x3070,opt(Array.from({length:25},(_,i)=>`UTC${i-12>=0?'+':''}${i-12}`))),
  add('GNSS','speedUnit','Speed unit',0x3071,opt(['km/h','knots','m/s'])),
  add('GNSS','distanceUnit','Distance unit',0x3072,opt(['km','nautical miles','miles'])),
  add('GNSS','altitudeUnit','Altitude unit',0x3073,opt(['meters','feet'])),

  add('APRS identity','aprsOn','APRS',0x3084,yesNo),
  add('APRS identity','siteType','Position source',0x3083,opt(['Fixed coordinates','GNSS'])),
  str('APRS identity','call','Callsign',0x3085,6,{pattern:'[A-Za-z0-9]{1,6}'}),
  add('APRS identity','ssid','SSID',0x308b,opt(Array.from({length:16},(_,i)=>String(i)))),
  add('APRS identity','symbolTable','Symbol table',0x308c,[[0x2f,'Primary /'],[0x5c,'Alternate \\']]),
  num('APRS identity','symbol','Symbol index',0x30de,0,255),
  str('APRS identity','customInfo','Beacon comment',0x309d,64),

  add('APRS beacon','micE','Mic-E start',0x30df,yesNo),
  add('APRS beacon','micType','Mic-E type',0x30e0,opt(['Off duty','En route','In service','Returning','Committed','Special','Priority','Emergency'])),
  add('APRS beacon','reportVoltage','Report voltage',0x30f3,yesNo),
  add('APRS beacon','reportSats','Report satellites',0x30f4,yesNo),
  add('APRS beacon','reportMileage','Report mileage',0x30f5,yesNo),
  add('APRS beacon','pttLink','PTT beacon',0x30f6,yesNo),
  add('APRS beacon','timedBeacon','Timed beacon',0x30f7,yesNo),
  num('APRS beacon','beaconInterval','Beacon interval (seconds)',0x30f8,1,65534,{type:'u16le'}),
  str('APRS beacon','route1','Path 1',0x30e1,8),
  add('APRS beacon','route1Count','Path 1 hops',0x30e9,opt(Array.from({length:10},(_,i)=>String(i)))),
  str('APRS beacon','route2','Path 2',0x30ea,8),
  add('APRS beacon','route2Count','Path 2 hops',0x30f2,opt(Array.from({length:10},(_,i)=>String(i)))),

  add('APRS relay & routing','forwardChannel','Digipeater channel',0x30fa,opt(['A','B','A + B'])),
  add('APRS relay & routing','digi1On','Digipeater 1',0x30fb,yesNo),
  str('APRS relay & routing','digi1Name','Digipeater 1 alias',0x30fc,8),
  add('APRS relay & routing','digi2On','Digipeater 2',0x3104,yesNo),
  str('APRS relay & routing','digi2Name','Digipeater 2 alias',0x3105,8),
  add('APRS relay & routing','forwardWait','Forward delay',0x310d,opt(Array.from({length:10},(_,i)=>`${i} s`))),
  str('APRS relay & routing','remotePassword','Relay remote password',0x310e,8,{sensitive:true}),
  add('APRS relay & routing','rxChannel','APRS RX channel',0x3116,opt(['Off','A','B'])),
  add('APRS relay & routing','txChannel','APRS TX channel',0x3117,opt(['A locked','B locked','A auto','B auto','A + B'])),
  add('APRS relay & routing','pttPriority','PTT priority',0x3118,opt(['Call','APRS'])),
  add('APRS relay & routing','rxPopup','RX popup',0x311b,yesNo),
];

export function readSetting(image, setting) {
  const {offset, type, mask, shift = 0, length} = setting;
  if (!(image instanceof Uint8Array) || image.length < 0x3148) throw new Error('TD-H9 image is incomplete.');
  if (setting.id === 'morseOn' && image[offset] === 0xff) return '0';
  if (setting.id.startsWith('morse') && image[offset] === 0xff) return 'Uninitialized';
  if (type === 'text') {
    const raw = image.slice(offset, offset + length);
    const end = raw.findIndex(b => b === 0 || b === 0xff);
    const data = end < 0 ? raw : raw.slice(0, end);
    return [...data].every(b => b >= 32 && b <= 126) ? String.fromCharCode(...data) : `Raw: ${[...raw].map(b=>b.toString(16).padStart(2,'0')).join('')}`;
  }
  const n = type === 'u16le' ? image[offset] | image[offset+1] << 8 : mask !== undefined ? (image[offset] & mask) >> shift : image[offset];
  return String(n);
}

export function writeSetting(image, setting, value) {
  const {offset, type, mask, shift = 0, length} = setting;
  const next = image.slice();
  if (value === readSetting(image, setting)) return next;
  if (setting.id === 'morseOn' && image[offset] === 0xff && value === '1') {
    next[offset] = 0x25; // observed when Morse was enabled on a tested TD-H9
    return next;
  }
  if (setting.id.startsWith('morse') && image[offset] === 0xff) throw new Error('This Morse setting is uninitialized on this radio image.');
  if (type === 'text') {
    if (!/^[\x20-\x7e]*$/.test(value) || value.length > length || (setting.pattern && value && !(new RegExp(`^(?:${setting.pattern})$`).test(value)))) throw new Error(`${setting.label}: use up to ${length} printable ASCII characters.`);
    next.fill(0, offset, offset + length);
    for (let i=0; i<value.length; i++) next[offset+i] = value.charCodeAt(i);
    return next;
  }
  if (!/^\d+$/.test(value)) throw new Error(`${setting.label}: enter a whole number.`);
  const n = Number(value);
  if (type === 'select' && !setting.options.some(([v]) => v === n)) throw new Error(`${setting.label}: choose a listed value.`);
  if (type === 'number' || type === 'u16le') if (n < setting.min || n > setting.max) throw new Error(`${setting.label}: expected ${setting.min}–${setting.max}.`);
  if (type === 'u16le') { next[offset] = n & 255; next[offset+1] = n >> 8; }
  else if (mask !== undefined) next[offset] = (next[offset] & ~mask) | ((n << shift) & mask);
  else next[offset] = n;
  return next;
}

export function readFixedPosition(image) {
  const raw = image.slice(0x3074, 0x3082);
  if (raw.every(b => b === 0xff)) return 'Not configured in this image';
  const view = new DataView(image.buffer, image.byteOffset, image.byteLength);
  const longitude = view.getUint32(0x3074, false) / 1_000_000;
  const latitude = view.getUint32(0x3079, false) / 1_000_000;
  const westEast = String.fromCharCode(image[0x3078]);
  const northSouth = String.fromCharCode(image[0x307d]);
  if (!'EW'.includes(westEast) || !'NS'.includes(northSouth) || longitude > 180 || latitude > 90) return 'Stored bytes are not a valid fixed position';
  const altitude = view.getInt32(0x307e, false) / 1_000_000;
  return `${latitude.toFixed(6)}° ${northSouth}, ${longitude.toFixed(6)}° ${westEast}, ${altitude.toFixed(1)} m`;
}
