# Main RF AF selector

The pinned TD-H9 main v1.0.38 `app.bin` has SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Independent Pi32v2 decoding of app+`0x0592E4` gives five instructions:
shift input register `r0` left eight bits, load `0x6040` into `r1`, OR
`r0` into `r1`, set `r0` to `0x47`, and tail-call the RF-register writer.
The source verifier pins those 14 instruction bytes to the stock image.

This produces register `0x47` words `0x6040`, `0x6140`, and `0x6340` for
input modes `0`, `1`, and `3`. Stock receive setup at app+`0x05CB2C`
uses mode `1`; the tone path uses mode `3`. The source port in
`replacement-source/main/h9_main_rf_registers.*` computes the word and
offers a write through an injected register transport. No boot entry invokes
it yet.

The meaning of mode `0` as mute comes from a different radio's BK4819
implementation. The TD-H9 signal route to the separate APRS controller's
ADC and to Bluetooth audio remains unknown. In particular, selecting mode
`0` might cut the APRS decode input. This register write is therefore an
accurate port of the stock arithmetic, but is not an APRS mute implementation
or a validated hardware setting for this purpose.
