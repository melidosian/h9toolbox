# APRS transmit settings over the main-radio serial link

Source: pinned APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Ghidra's `FUN_08005B8C` parser and cross-references to its RAM setting words
were checked in the read-only `H9Aprs` project. The read-only startup decoder
reports defaults of 500, 100, 200, and 63 at RAM `0x20000110..0x2000011C`.

The first serial byte after `0xC0` is a command. The parser preserves command
bytes `0x10`, `0x11`, `0x13`, `0x20`, and `0x21`; it reduces any other command
byte to its low nibble. It accepts the following short settings immediately
when each data byte arrives, before a closing `0xC0`. A later byte in the same
frame overwrites the previous value. These data bytes are raw; unlike packet
commands, `0xDB` is not treated as an escape prefix.

| Command | RAM word | Stock operation | Startup default |
| --- | --- | --- | ---: |
| `0x01` | `0x20000110` | byte × 10; TX lead milliseconds | 500 |
| `0x04` | `0x20000114` | byte × 10; TX tail milliseconds | 100 |
| `0x03` | `0x20000118` | byte × 10; runtime use not identified | 200 |
| `0x02` | `0x2000011C` | byte directly; TX persistence threshold | 63 |
| `0x0F` | `0x2000010E` | nonzero enables queued-TX status reply | 0 |

The literal pointers at `0x08005DD4..0x08005DE4` identify those parser writes.
`0x08002664` reads lead and tail for 1200-baud flag counts; `0x08005760`
compares an 8-bit random draw against the persistence byte. A direct-reference
scan of the analyzed image finds the write at `0x08005D8E` and its literal at
`0x08005DDC` for `0x20000118`, but no direct read of that word. The neighboring
lead, tail, and persistence words each have direct read sites. An indirect
read through a base pointer remains possible, so the third setting's runtime
effect is still unverified. The replacement
controller stores all four settings and passes lead/tail updates to the
diagnostic board's TX service. Its RF transmit authorization remains disabled.
