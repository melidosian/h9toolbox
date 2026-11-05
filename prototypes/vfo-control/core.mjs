export function hex(bytes) {
  return [...bytes].map(byte => byte.toString(16).padStart(2, '0')).join(' ');
}

export function changedBytes(first, second) {
  if (first.length !== second.length) throw new Error('Snapshots have different lengths.');
  const changes = [];
  for (let offset = 0; offset < first.length; offset++) {
    if (first[offset] !== second[offset]) changes.push({ offset, from: first[offset], to: second[offset] });
  }
  return changes;
}

export function compareVfoSnapshots(a1, b, a2) {
  if (a1.length !== b.length || b.length !== a2.length) throw new Error('Snapshots have different lengths.');
  const stable = [];
  const unstable = [];
  for (let offset = 0; offset < a1.length; offset++) {
    if (a1[offset] !== a2[offset]) unstable.push(offset);
    else if (a1[offset] !== b[offset]) stable.push({ offset, a: a1[offset], b: b[offset] });
  }
  return { stable, unstable };
}

export function analyzeVolumeSnapshots(low1, high, low2) {
  if (low1.length !== high.length || high.length !== low2.length) {
    throw new Error('Volume snapshots have different lengths.');
  }
  const drift = changedBytes(low1, low2);
  const changes = changedBytes(low1, high);
  const returnChanges = changedBytes(high, low2);
  const change = changes.length === 1 ? changes[0] : null;
  const returned = returnChanges.length === 1 ? returnChanges[0] : null;
  const volumeField = change && returned && change.offset === returned.offset
    && drift.every(item => item.offset === change.offset)
    && change.from >= 0 && change.from <= 30
    && change.to > change.from && change.to <= 30
    && returned.to >= 0 && returned.to < change.to
    ? { offset: change.offset, firstLow: change.from, high: change.to, returnedLow: returned.to }
    : null;
  const candidate = volumeField && volumeField.high === volumeField.returnedLow + 1
    ? { offset: volumeField.offset, low: volumeField.returnedLow, high: volumeField.high }
    : null;
  return { changes, drift, returnChanges, volumeField, candidate };
}

export function volumeWriteBlock(current, address, offset, value) {
  if (current.length !== 32) throw new Error('Expected a 32-byte radio block.');
  if (!Number.isInteger(address) || address < 0 || address >= 0x3140 || address % 32 !== 0) {
    throw new Error('Volume block address must be aligned within the clone image.');
  }
  if (!Number.isInteger(offset) || offset < 0 || offset >= 32) throw new Error('Invalid byte offset.');
  if (!Number.isInteger(value) || value < 0 || value > 30) throw new Error('Volume value must be 0–30.');
  const next = current.slice();
  next[offset] = value;
  const checksum = next.reduce((sum, byte) => (sum + byte) & 255, 0);
  return Uint8Array.of(0x57, address >> 8, address & 255, 0x20, ...next, checksum);
}

export function vfoWriteBlock(current, value) {
  if (current.length !== 32) throw new Error('Expected a 32-byte 0x3000 radio block.');
  if (![0, 1].includes(current[4]) || ![0, 1].includes(value)) throw new Error('VFO selector must be A=0 or B=1.');
  const next = current.slice();
  next[4] = value;
  const checksum = next.reduce((sum, byte) => (sum + byte) & 255, 0);
  return Uint8Array.of(0x57, 0x30, 0x00, 0x20, ...next, checksum);
}

export function bChannelWriteBlock(current, channel) {
  if (current.length !== 32) throw new Error('Expected a 32-byte 0x0CA0 radio block.');
  if (!Number.isInteger(channel) || channel < 1 || channel > 199) throw new Error('Channel must be 1–199.');
  const next = current.slice();
  next[5] = channel; // radio memory offset 0x0CA5
  const checksum = next.reduce((sum, byte) => (sum + byte) & 255, 0);
  return Uint8Array.of(0x57, 0x0c, 0xa0, 0x20, ...next, checksum);
}
