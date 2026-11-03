export async function readBlockReply(address, size, mode, readExact, readOptionalByte) {
  const expected = Uint8Array.of(0x57, (address >> 8) & 255, address & 255, size);
  let header = await readExact(4);

  // A late checksum from the previous block can precede this block's header.
  if (mode !== 'yes' && !header.every((byte, index) => byte === expected[index])) {
    const next = await readExact(1);
    const shifted = Uint8Array.of(header[1], header[2], header[3], next[0]);
    if (shifted.every((byte, index) => byte === expected[index])) {
      header = shifted;
      mode = 'yes';
    }
  }
  if (!header.every((byte, index) => byte === expected[index])) {
    throw new Error(`Invalid read header at 0x${address.toString(16)}: ${[...header].map(b => b.toString(16).padStart(2, '0')).join(' ')}.`);
  }

  const data = await readExact(size);
  if (mode === 'yes') await readExact(1); // firmware-provided checksum
  else if (mode === 'auto') mode = (await readOptionalByte()) === null ? 'no' : 'yes';
  return { data, mode };
}
