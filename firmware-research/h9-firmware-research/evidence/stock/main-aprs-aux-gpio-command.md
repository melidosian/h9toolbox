# Main-to-APRS auxiliary GPIO handshake

Stock main `app.bin` SHA-256:
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The image is mapped at inferred virtual base `0x01E00100`.

- At app+`0x072E4A`, the main firmware builds APRS serial command `0x11`
  with a one-byte payload. Direct callers at app+`0x072EAC` and
  app+`0x072EF8` pass values `1` and `2` respectively. Independent Pi32
  instruction decoding establishes the command and payload; the decompiler
  cannot recover this wrapper's full body.
- app+`0x072E8A` normalizes RAM `0xC1C4` to zero or one. When the APRS
  serial handle at RAM `0x11CFC` is nonzero, the one branch sends `0x11/1`
  and the zero branch sends `0x11/2`. It also updates RAM `0x12550` onward
  and starts or stops event `0x81`; those side effects are not ported here.
- app+`0x072F1A` stores the selected byte via the stock storage routine
  using group `4`, offset `0x3066`, length `1`, then invokes app+`0x072E8A`.
  The user-facing setting name has not been established.
- In the stock APRS-controller image, command `0x11/1` raises GPIOB5 and
  `0x11/2` lowers it. Command `0x11/4` sends subsequent bytes to raw
  USART2 only while that GPIO reads high. See [the APRS serial-path
  trace](aprs-serial-tx-paths.md).

`replacement-source/main/h9_main_aprs_aux_control.*` ports the conditional
serial command. A host integration test passes that frame to the replacement
APRS controller and verifies the GPIO-gated raw USART2 callback. The
external circuit on GPIOB5 is unverified. In particular, this trace does not
identify an RF-ready, PTT, speaker, or Bluetooth audio gate, so the APRS TX
runtime must not use it as its RF permit input.
