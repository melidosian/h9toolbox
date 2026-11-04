# APRS controller serial transmit paths

Source: captured APRS v1.0.15 image, SHA-256 `1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`. Addresses below come from the ARM decompilation in `evidence/aprs-command-handlers.txt`, `evidence/aprs-tx-send-gate.txt`, and `evidence/aprs-tx-wave-scheduler.txt`. This is static evidence, not a transmitted RF capture.

Serial command `0x00` reaches the send gate at `0x08005760`, then packet encoder `0x0800534C` and the AFSK waveform queue. The replacement `h9_aprs_tx_runtime` models this route, but only accepts a packet while an explicit RF-ready input is true. The diagnostic board entry has no verified RF-ready signal and keeps this input false.

Serial command `0x11`, subcommand `4`, takes a separate route through `0x08005A94` and `0x08002A4C`. The latter reads GPIOB bit 5 through `0x08002964`; when the read equals 1, it calls `0x08002C94`, which reaches `0x0800476C`. The decompiled `0x0800476C` manipulates a different queue/peripheral object, so it must not be wired to the AFSK service merely because both commands carry packet bytes. The diagnostic board entry leaves the `queue_packet_tx` callback unset until that queue and its destination are reconstructed. The electrical meaning of GPIOB bit 5 is also unverified.
