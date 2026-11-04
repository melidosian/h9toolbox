# Main four-channel clock measurement stage

Source: pinned stock main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The independent Pi32v2 decode of app+`0x0C54B2..0x0C5592` is in ignored
`output/main-clock-early-independent.asm`; Ghidra's earlier export is in
ignored `output/main-board-early-callees.txt`.

The board entry calls this helper after writing its RAM guards and three
flags. At helper app+`0x0C54B4`, it reads MMIO `0x1F0200`, shifts right by
eight, adds seven, and rotates the low nibble right by one. When the result
is less than eight and its bit is set in mask `0xE1`, four words at
`0xDA14`, `0xDA28`, `0xDA3C`, and `0xDA50` receive `50,000,000`. The
independent decode shows `uextra` and `insert` operations; this low-nibble
expression accounts for their combined result.

The helper then loops over four channel indices. Each iteration calls ROM
`0x020000A0(0xB0, 0x40)` and
`0x020000A0(0xB1, (index << 2) | 0x82)`. It writes `0x4000`, zero, and
`0x960` to MMIO words at `0x1E0400`, `0x1E0404`, and `0x1E0408`, then
writes one to `0x1E0400`. Stock polls bit 15 of that first word until set,
restores `0x4000`, calls ROM `0x02000058(0xB2)`, and calls ROM
`0x02000A28(0xB1, 0xFD)`. The first loop ends at app+`0x0C5544`.
The rest of the helper configures clocks and calibrates a 240 MHz path;
the calibration path remains unported.

The next call at app+`0x0C5544` targets app+`0x0C5492`. Its independent
decode reads register `0x1E1920`, replaces it with `0x331D524D`, preserves
bit 26 from the old word, and writes `0x1102` to `0x1E1924`. It tail-calls
app+`0x0C543E`, which clears the two fields under `0xFFF807FF` in
`0x1E1920` and inserts `0x5000` when RAM byte `0xDA58` is nonzero and
`0x50000` when byte `0xDA5C` is nonzero. The board marker stage sets both
bytes to one. The independent decode is in ignored
`output/main-clock-helpers-independent.asm`. No hardware clock-rate
measurement has confirmed this interpretation.

At app+`0x0C555E`, the stock caller writes one to RAM byte `0xD9E0`
and calls app+`0x0C509C` with `240,000,000`. Ghidra and the independent
decode agree that this callee writes band three to `0xD9D4` when the
`0xD9D0` flag is zero. Otherwise it scans four 32-bit thresholds at
`0xDA0C`, `0xDA20`, `0xDA34`, and `0xDA48`, advancing while a threshold
is strictly below the requested frequency. It stores the first matching
index, capped at three, in `0xD9D4`. This is a calibration-record selector;
the subsequent app+`0x0C50D8` arithmetic has unresolved Pi32 operations
in Ghidra and remains unported. Ghidra's selected-function export is in
ignored `output/main-clock-calibration-ghidra.txt`.

`replacement-source/main/h9_main_clock_probe.*` ports this first stage
with injectable ROM and measurement callbacks. Its target binding uses the
observed addresses. Unlike stock's unbounded poll, the replacement has a
caller-selected bound; on timeout it restores `0x4000` and stops before
the ROM read/mask calls. Host tests verify four-channel order, calibration
word boundaries, and timeout behavior. `h9_main_clock_selector_run` ports
the next register sequence with a caller-supplied register and flag binding;
host tests cover both flag bytes and the preserved bit.
`h9_main_clock_band_select` ports the following 240 MHz band choice and
caller request flag; host tests cover disabled mode, each boundary, and the
index cap. The diagnostic board entry invokes these three stages and stops
before calibration arithmetic. The Pi32 link check verifies key addresses,
constants, and call placement. No radio has run it, and the linked image
remains unflashable.
