// US amateur repeater suggestions from the ARRL band plan. Local repeater
// coordination and individual repeater listings take precedence.
const BANDS = [
  { name: '2 m', low: 144, high: 148, offset: 0.6 },
  { name: '1.25 m', low: 222, high: 225, offset: 1.6 },
  { name: '70 cm', low: 420, high: 450, offset: 5 },
];

export function parseMHz(value) {
  const text = String(value).trim();
  if (!/^\d{2,3}(?:\.\d{1,5})?$/.test(text)) return null;
  const mhz = Number(text);
  return Number.isFinite(mhz) && mhz >= 18 && mhz <= 600 ? mhz : null;
}
export function formatMHz(value) { return value.toFixed(5); }
export function standardOffset(mhz) {
  return BANDS.find(b => mhz >= b.low && mhz <= b.high) ?? null;
}
export function suggestedDirection(mhz) {
  if (mhz >= 145.2 && mhz <= 145.5) return 1;
  if (mhz >= 146.61 && mhz <= 146.97) return 1;
  if (mhz >= 147.0 && mhz <= 147.39) return 2;
  if (mhz >= 223.85 && mhz <= 224.98) return 1;
  return 0;
}
export function planTransmit(rxInput, {direction = 0, autoDirection = false, customShift = null} = {}) {
  const rx = parseMHz(rxInput);
  if (rx === null) return null;
  const choice = autoDirection ? suggestedDirection(rx) : Number(direction);
  const band = standardOffset(rx);
  if (choice === 0 || choice === 3) {
    if (customShift !== null) {
      const tx = rx + customShift;
      if (tx < 18 || tx > 600) return {direction: choice, tx: null, message: 'Custom TX split would be out of range. Enter TX manually.'};
      return {direction: choice, tx: formatMHz(tx), message: `Custom split kept (${customShift >= 0 ? '+' : ''}${customShift.toFixed(5)} MHz).`};
    }
    if (rx >= 442 && rx <= 450 && autoDirection) return {direction: 0, tx: formatMHz(rx), message: '70 cm repeater direction varies locally. Choose + or − from the repeater listing.'};
    return {direction: choice, tx: formatMHz(rx), message: 'Simplex: TX follows RX.'};
  }
  if (choice !== 1 && choice !== 2) return null;
  if (!band) return {direction: choice, tx: null, message: 'No standard repeater offset for this band. Enter TX manually.'};
  const shift = customShift !== null ? customShift : (choice === 1 ? -band.offset : band.offset);
  const tx = rx + shift;
  if (tx < band.low || tx > band.high) return {direction: choice, tx: null, message: `${band.name} TX would leave the amateur band. Check the direction or enter TX manually.`};
  if (customShift !== null) return {direction: choice, tx: formatMHz(tx), message: `Custom ${band.name} split (${shift >= 0 ? '+' : ''}${shift.toFixed(5)} MHz) follows RX.`};
  const inferred = autoDirection && choice !== Number(direction);
  return {direction: choice, tx: formatMHz(tx), message: `${band.name} ${choice === 1 ? '−' : '+'}${band.offset} MHz${inferred ? ' suggested from the US output range' : ''}. Verify the repeater listing.`};
}
