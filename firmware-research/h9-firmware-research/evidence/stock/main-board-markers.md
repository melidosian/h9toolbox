# Main board guards and early state flags

Source: pinned stock main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The independent Pi32v2 decode of app+`0x05FBF6..0x05FC26` is in ignored
`output/main-board-markers-independent.asm`; Ghidra's board listing and
early-callee trace are in ignored `output/main-board-entry.txt` and
`output/main-board-early-callees.txt`.

The board entry reaches app+`0x05FBF6` whether RAM byte `0xD5F0` was
previously set or the one-time allocation and memory-window branch just
ran. Four doubleword stores fill 16 bytes at `0xDA80..0xDA8F` and 16 bytes
at `0xE290..0xE29F` with `0x5A`. The first range immediately follows the
captured stock RAM-code extent `0xDA78`; the second starts at the observed
initial stack pointer `0xE290`. Their runtime use is not yet traced.

Two local helper calls follow. The first, app+`0x0C5472`, sets bytes at
`0xDA58` and `0xDA5C` to one. The second, app+`0x0C5486`, sets byte at
`0xD9D0` to one. The next helper, app+`0x0C54B2`, begins a larger clock
and register setup; it is outside this port.

`replacement-source/main/h9_main_board_markers.*` ports the bounded writes
using injectable pointers. The diagnostic board entry calls it after the
memory-window branch, including when that branch is skipped. Host tests
check both guard boundaries and all three flags. The Pi32 link check verifies
the binding address arithmetic, emitted byte stores, and call order. This
is not a bootable image or a hardware test.
