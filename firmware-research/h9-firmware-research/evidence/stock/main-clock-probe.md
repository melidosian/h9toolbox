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
index, capped at three, in `0xD9D4`. This is a calibration-record selector.

The subsequent app+`0x0C50D8` routine selects the 20-byte record at
`0xDA08 + 20 * band`. It returns `0xFFFFFFFF` when either record word
zero or one is below the requested frequency. Otherwise it clamps record
word three to `[frequency, record word zero]`. The 32-bit predicate opcode
at app+`0x0C5106` is `11 E9 00 10`; its second word `0x1000` encodes one
then instruction and one else instruction. Those are the two unsigned
divides at app+`0x0C510A` and `0x0C510E`. The preceding bounds ensure the
first divide is selected for a valid record; the result is multiplied by
the requested frequency. The caller stores it at `0xD9E4`. Ghidra stops
before these arithmetic operations because its Pi32 p-code cannot resolve
app+`0x0C50FE`; the independent decoder and the Pi32 predicate format
resolve the branch. Ghidra's selected-function export is in ignored
`output/main-clock-calibration-ghidra.txt`. Predicate encoding was checked
against `docs/ISA.md` in [pi32v2-isa](https://github.com/jvitkauskas/pi32v2-isa)
at commit `9a06ab98808e1c368dea1741f4eeb5aa1b388615`.

When that result is not `0xFFFFFFFF`, the caller enters
app+`0x0C5118`. It searches four source words at virtual address
`0x01EB27DC`: `192000000`, `137000000`, `320000000`, and `480000000`.
The two four-byte divider tables at `0x01EB146B` and `0x01EB146F`
contain `{1,2,4,8}` and `{1,3,5,7}`. The nested loops compare
`source / primary / secondary` with the requested result, in that order.
On a match, RAM `0xD9FC` receives the source index, `0xDA00` receives
`primary_index * 4 + secondary_index`, and `0xD9E4` retains the result.
On exhaustion, `0xD9E4` receives `0xFFFFFFFF` while the two indices
remain untouched. For a 240 MHz input, the first match is source index
three (480 MHz), primary index one (divide by two), and secondary index
zero (divide by one), yielding encoded divider index four. The table
bytes were read from the pinned stock app hash above; the replacement
owns equivalent constant tables rather than referring to stock image
addresses. Zero-divisor handling in the replacement is a guard for
injected tables; the captured stock tables contain no zero.

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
index cap. `h9_main_clock_calculate` ports the bounded record calculation
and `0xFFFFFFFF` result; host tests cover valid multiples, clamping, and
invalid records. `h9_main_clock_match` ports the source/divider search,
including match and exhaustion outputs. The diagnostic board entry invokes
these stages, the following timing calculation, and the first clock-apply
transition, then stops before the next stock helper. The Pi32 link
check verifies key addresses, constants, table bytes, and call placement.
No radio has run it, and the linked image remains unflashable.

The next two stock helpers are separately ported as
`h9_main_clock_program_source` and `h9_main_clock_latch_source`. At
app+`0x0C52D4`, the first helper returns without touching hardware when
RAM word `0xD9E4` is `0xFFFFFFFF`. Otherwise it reads and writes
`0x1E0014` twice: RAM byte `0xD9FC` replaces bits 0–1, and RAM byte
`0xDA00` replaces bits 2–5. At app+`0x0C5308`, the second helper reads
`0x1E0014` six times, then sets bits 28, 27, 26, 25, and 24 of
`0x1E0044` in five separate writes. The replacement exposes injectable
register pointers and a target-address binder, with host tests for the
bitfields, failure sentinel, and latch bits. The Pi32 link check inspects
their emitted instructions. These helpers are linked but are **not** called
by the diagnostic board entry: the next helper at app+`0x0C5288` and the
remaining clock transitions are still under study. No hardware behavior is inferred from the
host tests or link check.

Stock app+`0x0C5330` then updates `0x1E0008` in three stages. It inserts
the low three bits of RAM byte `0xD9E8` into register bits 12–14 and writes
the word. It passes RAM byte `0xD9EC` as the first argument and RAM byte
`0xD9F0` as the second argument to ROM `0x02000110`. It then inserts the
low eight bits of RAM halfword `0xD9F4` into register bits 0–7 and writes
again. Finally, it inserts the low three bits of RAM byte `0xD9F8` into
register bits 8–10 and writes a third time. The direct Pi32 decode shows
that call target because `0x01EC5460 + 0x0013ACB0 = 0x02000110`.
`h9_main_clock_aux_program` ports these writes and the call order with an
injectable ROM callback. Its target binder uses those observed addresses;
the ROM routine's internal effect is still unknown. This helper remains
unconnected to diagnostic board startup pending the surrounding sequence.
