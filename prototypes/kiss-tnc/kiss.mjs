// KISS carries AX.25 frames without the over-the-air FCS.
export function encodeKiss(payload, port = 0, command = 0) {
  if (!Number.isInteger(port) || port < 0 || port > 15 || !Number.isInteger(command) || command < 0 || command > 15) throw new Error('KISS port and command must be 0–15.');
  const out = [0xc0];
  for (const byte of [(port << 4) | command, ...payload]) {
    if (byte === 0xc0) out.push(0xdb, 0xdc);
    else if (byte === 0xdb) out.push(0xdb, 0xdd);
    else out.push(byte);
  }
  out.push(0xc0);
  return Uint8Array.from(out);
}

export class KissDecoder {
  constructor(maxLength = 4096) { this.maxLength = maxLength; this.reset(); }
  reset() { this.active = false; this.escaped = false; this.invalid = false; this.bytes = []; }
  feed(chunk) {
    const frames = [];
    for (let byte of chunk) {
      if (byte === 0xc0) {
        if (this.active && !this.invalid && !this.escaped && this.bytes.length) {
          const [type, ...payload] = this.bytes;
          frames.push({ port: type >> 4, command: type & 15, payload: Uint8Array.from(payload) });
        }
        this.active = true; this.escaped = false; this.invalid = false; this.bytes = [];
        continue;
      }
      if (!this.active || this.invalid) continue;
      if (this.escaped) {
        this.escaped = false;
        if (byte === 0xdc) byte = 0xc0;
        else if (byte === 0xdd) byte = 0xdb;
        else { this.invalid = true; this.bytes = []; continue; }
      } else if (byte === 0xdb) { this.escaped = true; continue; }
      this.bytes.push(byte);
      if (this.bytes.length > this.maxLength) { this.invalid = true; this.bytes = []; }
    }
    return frames;
  }
}

function address(text, last, repeated = false, destination = false) {
  const match = /^([A-Z0-9]{1,6})(?:-(\d{1,2}))?$/.exec(text);
  if (!match || Number(match[2] || 0) > 15) throw new Error(`Invalid AX.25 address: ${text}`);
  return [...match[1].padEnd(6)].map(c => c.charCodeAt(0) << 1).concat(0x60 | (Number(match[2] || 0) << 1) | (last ? 1 : 0) | (repeated || destination ? 0x80 : 0));
}

export function encodeAprs(line) {
  const match = /^([^>]+)>([^:]+):([\s\S]*)$/.exec(line);
  if (!match) throw new Error('Use SOURCE>DESTINATION[,PATH]:information.');
  const source = match[1], [destination, ...path] = match[2].split(',');
  if (path.length > 8) throw new Error('At most eight digipeaters are supported.');
  const info = match[3];
  if (!info || info.length > 256 || /[^\x20-\x7e]/.test(info)) throw new Error('Use 1–256 printable ASCII characters for APRS information.');
  const addresses = [address(destination, false, false, true), address(source, !path.length)];
  path.forEach((p, i) => addresses.push(address(p.replace(/\*$/, ''), i === path.length - 1, p.endsWith('*'))));
  return Uint8Array.from([...addresses.flat(), 0x03, 0xf0, ...new TextEncoder().encode(info)]);
}

export function decodeAprs(bytes) {
  let at = 0, ended = false;
  const addresses = [];
  while (at + 7 <= bytes.length && addresses.length < 10) {
    const raw = bytes.slice(at, at + 6);
    if (raw.some(b => b & 1)) throw new Error('Invalid shifted AX.25 address.');
    const call = String.fromCharCode(...raw.map(b => b >> 1)).trimEnd();
    if (!/^[A-Z0-9]{1,6}$/.test(call)) throw new Error('Invalid AX.25 callsign.');
    const flags = bytes[at + 6], ssid = (flags >> 1) & 15;
    addresses.push(call + (ssid ? `-${ssid}` : '') + (addresses.length >= 2 && flags & 0x80 ? '*' : ''));
    at += 7;
    if (flags & 1) { ended = true; break; }
  }
  if (!ended || addresses.length < 2 || bytes[at] !== 0x03 || bytes[at + 1] !== 0xf0) throw new Error('Not an APRS AX.25 UI frame (03/F0).');
  const info = new TextDecoder().decode(bytes.slice(at + 2));
  return `${addresses[1]}>${addresses[0]}${addresses.length > 2 ? ',' + addresses.slice(2).join(',') : ''}:${info}`;
}
