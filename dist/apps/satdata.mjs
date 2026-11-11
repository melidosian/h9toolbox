// SAT PASS's satellite data module (".SAT DATA"), made in the browser from fresh orbital elements.
// A port of firmware-research/module-loader/satpack.py (the build-time source of truth: a test compares
// the two byte for byte) of what the radio's integer orbit code needs: per satellite the SGP4 "un-Kozai"
// mean motion and semi-major axis, the secular J2 rates of RAAN, argument of perigee and mean anomaly and
// the ndot/2 decay term, all moved to one shared epoch, plus a Greenwich sidereal time reference.
// Layout: modules/satpass/satdata.h.

export const MODULE_NAME = '.SAT DATA';
export const MAX_SATELLITES = 16;
export const TLE_URL = 'https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle';
const MAGIC = 0x44544153;          // "SATD"
const HEAD = 32, ENTRY = 64;
const XKE = 60.0 / Math.sqrt(6378.135 ** 3 / 398600.8);   // SGP4 (WGS72) constants
const J2 = 1.082616e-3;
const EARTH_KM = 6378.135;
const TURN = 2 ** 32;

// Python's round(): halves go to the even neighbour.
const roundEven = x => {
  const f = Math.floor(x), d = x - f;
  return d < 0.5 ? f : d > 0.5 ? f + 1 : (f % 2 === 0 ? f : f + 1);
};
const mod = (x, m) => ((x % m) + m) % m;

export function parseTles(text) {
  const lines = text.split(/\r?\n/).map(l => l.trimEnd()).filter(l => l.trim());
  const out = new Map();
  for (let i = 0; i + 2 < lines.length; i++) {
    if (lines[i + 1].startsWith('1 ') && lines[i + 2].startsWith('2 ')) out.set(lines[i].trim(), [lines[i + 1], lines[i + 2]]);
  }
  return out;
}

export function epochUnix(line1) {
  const yy = Number(line1.slice(18, 20));
  const year = yy < 57 ? 2000 + yy : 1900 + yy;
  const day = Number(line1.slice(20, 32));
  return Date.UTC(year, 0, 1) / 1000 + (day - 1) * 86400.0;
}

function gmstTurns(unix) {
  const t = (unix / 86400.0 + 2440587.5 - 2451545.0) / 36525.0;
  const seconds = 67310.54841 + (876600 * 3600 + 8640184.812866) * t + 0.093104 * t * t - 6.2e-6 * t ** 3;
  return mod(seconds, 86400.0) / 86400.0;
}

export function element(line1, line2, ref) {
  const rad = deg => deg * Math.PI / 180;
  const ndot2 = Number(line1.slice(33, 43));                // rev/day^2 (already /2)
  const inc = rad(Number(line2.slice(8, 16)));
  const raan = rad(Number(line2.slice(17, 25)));
  const ecc = Number('0.' + line2.slice(26, 33).trim());
  const argp = rad(Number(line2.slice(34, 42)));
  const m0 = rad(Number(line2.slice(43, 51)));
  const nRev = Number(line2.slice(52, 63));
  const no = nRev * 2 * Math.PI / 1440.0;                   // rad/min
  const cosi = Math.cos(inc);
  const beta0 = Math.sqrt(1 - ecc * ecc);
  const k2 = 0.5 * J2;
  const a1 = (XKE / no) ** (2 / 3);
  const d1 = 1.5 * k2 * (3 * cosi * cosi - 1) / (a1 * a1 * beta0 ** 3);
  const a0 = a1 * (1 - d1 / 3 - d1 * d1 - 134 / 81 * d1 ** 3);
  const d0 = 1.5 * k2 * (3 * cosi * cosi - 1) / (a0 * a0 * beta0 ** 3);
  const n = no / (1 + d0);                                  // rad/min
  const a = a0 / (1 - d0);                                  // Earth radii
  const p = a * (1 - ecc * ecc);
  const raanDot = -1.5 * J2 * n * cosi / (p * p);           // rad/min
  const argpDot = 0.75 * J2 * n * (5 * cosi * cosi - 1) / (p * p);
  const mDot = n * (1 + 0.75 * J2 * beta0 * (3 * cosi * cosi - 1) / (p * p));
  const perMin = TURN / (2 * Math.PI) / 60.0;               // rad/min -> binary angle/s
  const epoch = epochUnix(line1);
  const shift = ref - epoch;                                // seconds to the shared epoch
  const mDdot = ndot2 * TURN / 86400.0 ** 2;                // binary angle/s^2
  const mRate = mDot * perMin;                              // at the TLE's epoch
  const mRateRef = mRate + 2 * mDdot * shift;               // at the shared epoch
  return {
    m0: mod(roundEven(m0 * TURN / (2 * Math.PI) + mRate * shift + mDdot * shift * shift), TURN),
    argp0: mod(roundEven((argp + argpDot * shift / 60) * TURN / (2 * Math.PI)), TURN),
    raan0: mod(roundEven((raan + raanDot * shift / 60) * TURN / (2 * Math.PI)), TURN),
    cosI: roundEven(cosi * 2 ** 30), sinI: roundEven(Math.sin(inc) * 2 ** 30), ecc: roundEven(ecc * 2 ** 30),
    a: roundEven(a * EARTH_KM * 256),
    argpDot: roundEven(argpDot * perMin * 65536), raanDot: roundEven(raanDot * perMin * 65536),
    mDot: roundEven(mRateRef * 256), mDdot: roundEven(mDdot * 2 ** 40),
    epoch,
  };
}

// The data module body for `satellites` (entries of satellites.json) from parsed TLEs (parseTles).
export function pack(tles, satellites) {
  if (satellites.length < 1 || satellites.length > MAX_SATELLITES) throw new Error(`Choose 1 to ${MAX_SATELLITES} satellites.`);
  const missing = satellites.filter(s => !tles.has(s.name)).map(s => s.label);
  if (missing.length) throw new Error(`Not in the orbital data: ${missing.join(', ')}.`);
  for (const s of satellites) if (s.label.length > 7) throw new Error(`${s.label}: labels are at most 7 characters.`);
  const ref = roundEven(Math.min(...satellites.map(s => epochUnix(tles.get(s.name)[0]))));
  const elements = satellites.map(s => element(...tles.get(s.name), ref));
  elements.forEach((e, i) => {
    if (!(Math.abs(e.mDot) < 2 ** 31 && Math.abs(e.mDdot) < 2 ** 31 && e.ecc < 0.02 * 2 ** 30))
      throw new Error(`${satellites[i].label}: outside the fixed-point ranges (eccentricity must be below 0.02).`);
  });
  const newest = Math.max(...elements.map(e => e.epoch));
  const date = new Date(Math.floor(newest) * 1000).toISOString().slice(0, 10);
  const gmst = mod(roundEven(gmstTurns(ref) * TURN), TURN);
  const body = new Uint8Array(HEAD + ENTRY * elements.length);
  const view = new DataView(body.buffer);
  view.setUint32(0, MAGIC, true); view.setUint16(4, 1, true); view.setUint16(6, elements.length, true);
  view.setUint32(8, ref, true); view.setUint32(12, gmst, true); view.setUint32(16, roundEven(newest), true);
  body.set(new TextEncoder().encode(date), 20);
  elements.forEach((e, i) => {
    const at = HEAD + ENTRY * i, s = satellites[i];
    body.set(new TextEncoder().encode(s.label), at);
    view.setUint32(at + 8, e.m0, true); view.setUint32(at + 12, e.argp0, true); view.setUint32(at + 16, e.raan0, true);
    [e.cosI, e.sinI, e.ecc, e.a, e.argpDot, e.raanDot, e.mDot, e.mDdot].forEach((v, k) => view.setInt32(at + 20 + 4 * k, v, true));
    view.setUint32(at + 52, roundEven(s.down * 1e5), true); view.setUint32(at + 56, roundEven(s.up * 1e5), true);
    view.setUint16(at + 60, roundEven(s.tone * 10), true);
  });
  return { body, info: { count: elements.length, date, newest, ref, gmst } };
}

// What a data module body holds, for display: { count, date, newest, labels } (null if it is not one).
export function describe(body) {
  if (!body || body.length < HEAD) return null;
  const view = new DataView(body.buffer, body.byteOffset, body.byteLength);
  if (view.getUint32(0, true) !== MAGIC) return null;
  const count = view.getUint16(6, true);
  if (count > MAX_SATELLITES || body.length < HEAD + ENTRY * count) return null;
  const text = at => new TextDecoder().decode(body.subarray(at, at + 12)).split('\0')[0];
  const labels = Array.from({ length: count }, (_, i) => new TextDecoder().decode(body.subarray(HEAD + ENTRY * i, HEAD + ENTRY * i + 8)).split('\0')[0]);
  return { count, date: text(20), newest: view.getUint32(16, true), labels };
}

// Fresh elements from CelesTrak, parsed. Throws with a readable message on failure.
export async function fetchTles(fetcher = fetch, url = TLE_URL) {
  let response;
  try { response = await fetcher(url, { cache: 'no-store' }); } catch (error) { throw new Error(`Could not reach CelesTrak (${error.message}).`); }
  if (!response.ok) throw new Error(`CelesTrak answered ${response.status}.`);
  const tles = parseTles(await response.text());
  if (tles.size < 5) throw new Error('CelesTrak sent no usable orbital data.');
  return tles;
}
