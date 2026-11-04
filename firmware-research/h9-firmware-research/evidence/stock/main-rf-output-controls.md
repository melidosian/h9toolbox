# Main RF output controls near TX setup

Source: pinned stock main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The independent Pi32v2 r3 decoder report is regenerated with
`analyze_app.py disasm --offset 0x58e58 --length 0x70` into ignored
`output/stock/main-rf-control-independent.asm`. The table at app+`0x554`
contains `0x001E5000` as its first 32-bit little-endian pointer.

Stock `FUN_01e58f58` (app+`0x058E58`) obtains that pointer through
`FUN_01e001ba(8)`, reads `0x001E0200`, then sets or clears bit 8 of
`0x001E5000` according to whether its argument is nonzero. The critical
clear instruction at app+`0x058E78` is `C0 EF 80 0F`, decoded as
`[r0+0] &= 0xFFFFFEFF`. Ghidra's pseudocode for this branch says
`& 0x100`, which would preserve only bit 8 and is incorrect. The same
independent decoder confirms `FUN_01e50820` at app+`0x050720` controls
bit 0 of the same register; its clear instruction at app+`0x050740`
is `C0 EF 01 00`, or `&= 0xFFFFFFFE`. That function also mirrors its
argument to RAM `0x0000C1BC`; the low-level replacement primitive does
not yet update this application state.

Stock `FUN_01e58f80` (app+`0x058E80`) reads `0x001E0608`, writes
`argument_2 * period / 255` to `0x001E060C`, and sets or clears bit 8
of `0x001E0600` according to `argument_1`. The recovered order and
register effects are in `main/h9_main_rf_output.*`, with fake-register
tests and a Pi32v2 diagnostic link. The Pi32 build uses inline memory-bit
instructions for the GPIO updates; generated assembly contains
`[r0+0] |= r1` and `[r0+0] &= r1`. The host build uses a read-modify-write
fallback for register tests.

The APRS TX setup `FUN_01e5ad68` calls `FUN_01e596e4` (which includes
the bit-0 write) before `FUN_01e58f58(4)` and
`FUN_01e58f80(1, duty)`, then returns to the packet builder. RX
configuration includes `FUN_01e58f58(0)` and a call to
`FUN_01e58f80` with its first argument zero. This ordering makes these outputs candidates for
keying or transmit audio, but **does not prove their electrical roles or
the first RF-emitting instruction**. The replacement primitives are
compiled but not called by a board entry. The existing unkeyed
`h9_main_tx_service` callback contract therefore still needs a
separately verified adapter; calling the stock TX setup inside its
`prepare_tx` callback would violate that contract if any of these
outputs keys RF.
