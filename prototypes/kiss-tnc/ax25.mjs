// AX.25 modulo-8 connected frames, without the RF FCS (the TNC owns it).
export const AX25 = Object.freeze({ I: 0, RR: 0x01, RNR: 0x05, REJ: 0x09,
  SABM: 0x2f, DISC: 0x43, UA: 0x63, DM: 0x0f, FRMR: 0x87, PID: 0xf0, PF: 0x10 });

export function normalizeCall(value) {
  const call = String(value).trim().toUpperCase();
  if (!/^[A-Z0-9]{1,6}(?:-(?:[0-9]|1[0-5]))?$/.test(call)) throw new Error(`Invalid AX.25 callsign: ${value}`);
  return call.replace(/-0$/, '');
}

function encodeAddress(value, last, marked) {
  const [base, ssid = '0'] = normalizeCall(value).split('-');
  return [...base.padEnd(6)].map(char => char.charCodeAt(0) << 1)
    .concat(0x60 | (Number(ssid) << 1) | Number(last) | (marked ? 0x80 : 0));
}

function frameKind(control) {
  if (!(control & 1)) return AX25.I;
  return control & ((control & 3) === 1 ? 0x0f : 0xef);
}

export function encodeAx25({ source, destination, via = [], control, command = true, pid = AX25.PID, data = new Uint8Array() }) {
  if (via.length > 8) throw new Error('At most eight digipeaters are supported.');
  if (!Number.isInteger(control) || control < 0 || control > 255) throw new Error('Invalid AX.25 control byte.');
  const addresses = [encodeAddress(destination, false, command === true), encodeAddress(source, !via.length, command === false)];
  via.forEach((call, index) => addresses.push(encodeAddress(call, index === via.length - 1, false)));
  const kind = frameKind(control);
  return Uint8Array.from([...addresses.flat(), control, ...(kind === AX25.I || kind === 0x03 ? [pid] : []), ...data]);
}

export function decodeAx25(raw) {
  const addresses = [];
  let offset = 0, ended = false;
  while (addresses.length < 10 && offset + 7 <= raw.length) {
    const chars = raw.slice(offset, offset + 6);
    if (chars.some(byte => byte & 1)) throw new Error('Invalid shifted AX.25 address.');
    const base = String.fromCharCode(...chars.map(byte => byte >> 1)).trimEnd();
    const flags = raw[offset + 6], ssid = (flags >> 1) & 15;
    const call = normalizeCall(base + (ssid ? `-${ssid}` : ''));
    addresses.push({ call, marked: Boolean(flags & 0x80) });
    offset += 7;
    if (flags & 1) { ended = true; break; }
  }
  if (!ended || addresses.length < 2 || offset >= raw.length) throw new Error('Incomplete AX.25 address chain.');
  const control = raw[offset++], kind = frameKind(control);
  let pid = AX25.PID;
  if (kind === AX25.I || kind === 0x03) {
    if (offset >= raw.length) throw new Error('Missing AX.25 PID.');
    pid = raw[offset++];
  }
  return { destination: addresses[0].call, source: addresses[1].call,
    via: addresses.slice(2).map(address => address.call), control, kind,
    command: addresses[0].marked !== addresses[1].marked ? addresses[0].marked : null,
    poll: Boolean(control & AX25.PF), ns: (control >> 1) & 7, nr: control >> 5,
    pid, data: raw.slice(offset) };
}
