# Main APRS command 0x10 and 0x21 handlers

Source: stock v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Ghidra project: `H9mainFixed`, with the corrected Pi32 byte-load offset.
Virtual address base `0x01E00100` is inferred from JLFS entry metadata.

- The UART dispatcher at app+`0x0870BC` sends nonempty command `0x10`
  payloads to app+`0x086EF4`. That routine begins collecting at `$`, resets
  on a later `$`, and waits for a newline. A sentence longer than five bytes
  and no longer than 255 bytes is copied to a temporary buffer and passed
  to app+`0x086656` for GPS parsing. A nonterminated sentence resets after
  the 1,022-byte active-length threshold. The replacement source implements
  collection and exposes a sentence callback; GPS field parsing is still open.
- Command `0x21` at app+`0x0870BC` uses the first payload byte as the version
  query selector. Selector `1` provides the module version, `2` the protocol
  version, and `0` both as consecutive zero-terminated strings. The helper
  at app+`0x086FB6` writes the versions to 15-byte main-radio fields. The
  replacement source retains bounded, zero-terminated strings in RAM;
  persistent settings writes and UI refresh remain unimplemented.
- Command `0x11` calls app+`0x086F7A` with the first payload byte and the
  remaining length. Independent Pi32v2 r3 disassembly shows that it acts
  only when that first byte is `3`: it frames command `0x11` with payload
  `{3, 0}` through app+`0x05B32E`, then writes the frame through app+`0x05B3BE`.
  The replacement returns the framed bytes for a UART callback to send.
- Command `0x51` checks a CRC-16/CCITT-style value using app+`0x087030`
  before executing control actions. The replacement source deliberately
  leaves those actions unimplemented until their input and state are mapped.

The decompiler produced a valid body for app+`0x086EF4`, app+`0x086FB6`,
and app+`0x087030`; it failed at app+`0x086F7A`, which was independently
decoded from the stock bytes. The notes above describe offline firmware
behavior, not radio runtime validation.
