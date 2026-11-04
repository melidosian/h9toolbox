# Main clock timing calculation

Source: pinned v1.0.38 main `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Addresses below use the inferred `0x01E00100` app base. The independent
Pi32v2 r3 decode is in ignored `output/main-clock-helpers-independent.asm`.
The read-only Ghidra export of `0x01EC52CA` and its helper `0x01EC5294` is
in a temporary local file; its pseudocode misorders parallel instruction
pairs, so the instruction listing and Pi32 pair semantics were used.

After the source/divider search at app+`0x0C5118`, stock calls
app+`0x0C51CA` with the selected clock in `r0` and `240000000` in `r1`.
It selects one 20-byte calibration record from `0xDA08 + 20 * band`, where
the band byte is at `0xD9D4`. The helper at app+`0x0C5194` finds the first
index `i` for which `value / (i + 1) <= limit`, returning `max + 1` if
none is found for the 0..7 search.

1. Choose `i` from 0..7 against record word 0, store it in byte `0xD9E8`,
   and divide the source clock by `i + 1`. Store that intermediate clock at
   `0xDA04`.
2. Store `((intermediate / 1000000) * 30) / 1000` in byte `0xD9F0`.
   Choose an index from 0..255 against record word 3 and store it in byte
   `0xD9EC`.
3. Choose an index from 0..255 against record word 1. It is raised to at
   least the low eight bits of `intermediate / target - 1` when that ratio's
   low byte is nonzero. Store the result in halfword `0xD9F4`, divide the
   intermediate clock by that result plus one, and store it in `0xD9D8`.
4. Choose an index from 0..7 against record word 2, store it in byte
   `0xD9F8`, divide the stage clock by that index plus one, and store the
   result in word `0xD9DC`.

The Pi32 `#` forms at app+`0x0C51F6`, `0x0C5210`, `0x0C5224`,
`0x0C5232`, `0x0C525C`, and `0x0C5368` issue in parallel with the following
memory instruction. Both instructions read their input registers before
either write occurs. Treating them sequentially produces false addresses
and values in decompiler output.

`replacement-source/main/h9_main_clock_timing.*` ports these calculations
with injected RAM pointers. It computes all fields before writing any output
and rejects zero inputs and a 0..255 divider search that never terminates.
The stock routine has no such bounds; its loop can wrap its eight-bit index.
The diagnostic board entry calls the port after a successful clock match,
then invokes the first clock-apply transition described in
[the apply trace](main-clock-apply.md). No radio has run the replacement.
