# APRS controller boot text

Source: pinned APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Read-only Ghidra instruction export of `0x08005DE8`, `0x08005E34`,
`0x08004FFC`, `0x08005056`, `0x080054A0`, and `0x08006010` was checked
against the literal strings in the image.

After the board initializer, stock prints `888`, a CRLF-prefixed APRS
module heading, software version `V1.0.15`, hardware version `V1.0.0`, and
a closing line. The format helper at `0x08005056` emits bytes through
`0x080054A0`; that function calls `0x08006010`, which writes one byte via
`0x0800476C` using the USART1 descriptor at RAM `0x20000904`. Thus the
banner shares the 115,200-baud serial port used by the APRS protocol.

`replacement-source/aprs/h9_aprs_boot_banner.*` follows the stock layout
and synchronous USART1 route, but emits `H9R0.01` as the replacement software
version. The protocol version remains `V1.0.0`. The serial command `0x20`
version reply uses that same seven-character replacement version. The host
test checks the banner bytes and failure path; the APRS link check pins all
seven stock string literals in the vendor image and rejects a diagnostic ELF
that lacks `H9R0.01` or still contains the vendor software version. This is
an offline source and compilation result; it has not run on the radio.
