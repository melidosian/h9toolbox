# Main APRS AX.25 receive path

Source: decoded stock v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Virtual address base `0x01E00100` is inferred from JLFS entry metadata.
The checked Ghidra decompilation was also compared with the independent
Pi32v2 r3 instruction decoder around app+`0x05D9A6..0x05DACA`.

- app+`0x05D9A6` decodes each seven-byte AX.25 address. It shifts the first
  six bytes right once, trims trailing spaces or zero bytes, and derives the
  output SSID/flags byte as `((input[6] & 0x1E) >> 1) | (input[6] & 0x80)`.
- app+`0x05D9F6` requires at least 16 input bytes, decodes destination and
  source, follows the extension bit through no more than six digipeaters,
  reads control and PID, and copies at most 256 information bytes.
- app+`0x0870BC` handles serial command `0x00` by calling that decoder. It
  forwards a packet only if decoding succeeds, the information length is
  nonzero, and the PID is `0xF0` or has high nibble `0x3`.

The replacement decoder requires a terminating address extension bit and
checks bounds before each address. It has not been compared against real
received radio traffic. The stock dispatcher also handles other commands;
their payload-specific behavior is not yet ported.
