# Main early allocation bitmap

Source: pinned stock main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The stock code is at app+`0x05FA7C..0x05FB32` (virtual
`0x01E5FB7C..0x01E5FC32`). The independent Pi32v2 decode is regenerated into
ignored `output/main-heap-start-independent.asm` and
`output/main-heap-blocks-independent.asm` with `analyze_app.py disasm`.

The board entry first calls ROM `0x020000A0(0x80, 0)` and `0x02003174()`.
If byte `0xD5F0` is zero, it sets it to one, calls ROM `0x021127B4` to clear
`0xDC` bytes at RAM `0x1D7D0`, then sets allocation bits for two block ranges.
The stock first block is `(0x1E4D8 + 127) >> 7 = 0x3CA`; its exclusive end
is `0x2BF00 >> 7 = 0x57E`. The second starts at
`0x2D27F >> 7 = 0x5A4` **without** rounding up; its exclusive end is
`0x2FF80 >> 7 = 0x5FF`. Each block sets bit `block & 31` in 32-bit word
`0x1D7D0 + 4*(block >> 5)` and bit `(block >> 5) & 31` in word
`0x1D890 + 4*(block >> 10)`. The independent decoder shows the `bitset`
operations and corrects Ghidra's misleading `lsl` rendering of the second
range start: the opcode is a right shift by seven.

`replacement-source/main/h9_main_heap_bitmap.*` ports that bounded stage.
It uses explicit 32-bit zero stores instead of the stock ROM clear call and
retains the early `0xD5F0` guard. Host tests check every primary bit through
block `0x5FF`, the second-level words, the gap, and the already-initialized
path. `link_main_check.py` verifies that the Pi32 build reaches this stage
after the two ROM calls, uses two Pi32 bitset instructions, and retains the
four stock block boundaries. The later allocator metadata at app+`0x05FB32`
onward, clock setup, interrupts, drivers, and application loop are still
unported. No bootable image or hardware test follows from this stage.
