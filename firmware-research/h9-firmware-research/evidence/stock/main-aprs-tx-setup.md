# Main APRS TX setup boundary

Source: captured main v1.0.38 `app.bin`, SHA-256 `11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`, analyzed in the corrected Pi32v2 `H9mainFixed` Ghidra project.

The selector at app+`0x05C2D4` chooses the side, calls RF side setup `FUN_01E58E3E`, then calls `FUN_01E5AD68` and packet builder `FUN_01E5BF2C`. `FUN_01E5AD68` validates the TX frequency, enters the TX configuration path, writes RF register values through `FUN_01E5070A`, and calls `FUN_01E59094(1, ...)` and `FUN_01E5924C(1, 2)`. The latter writes RF register `0x74`. `FUN_01E5070A` delegates to a ROM routine at `0x02001634`; its peripheral transport is not reconstructed here.

The independent Pi32v2 decode of RF side setup at app+`0x058D4C..0x058D76`
shows a two-input side decision. It reads Dual Watch from RAM `0x12BA7`.
When Dual Watch is on, it reads route byte `0x11AEC` and calls B setup at
app+`0x0568D2` for a nonzero byte, A setup at app+`0x0567C2` for zero.
When Dual Watch is off, it instead tests bit 7 of channel-state byte
`0x11B50`, calling B setup if set and A setup if clear. The source-input
verifier pins the 42 instruction bytes containing this branch to the stock
image. `h9_main_rf_setup_side` ports this decision, and its host tests cover
both Dual Watch states. This is a setup-function choice, not an RF-frequency
measurement or proof that the display's red line corresponds to that side.

The selected setup routine copies its channel frequency pair into the working
RF fields. The A routine at app+`0x0567C2` copies RAM `0x11CA0` to `0x11C8C`
and `0x11CA4` to `0x11C9C`; B at app+`0x0568D2` copies `0x11CA8` and
`0x11CAC` to those same destinations. Both channel-mode branches make these
copies. The channel-record loader supplies the A/B pairs through its frequency
conversion routine. The independent instruction decode and source-input
verifier pin the copy instructions at app+`0x0567E6..0x0567F6` and
`0x0568F6..0x056906`. `h9_main_rf_select_working_frequencies` ports this
frequency-pair selection; it does not port the other settings those routines
also copy.

For a null frequency override pointer, TX setup `FUN_01E5AD68` tests RAM byte
`0x12BBA`: zero loads the second working frequency at `0x11C9C`, nonzero
loads the first at `0x11C8C`, together with shift-mode byte `0x12259`.
For a non-null override pointer, zero selects its word at `+0`, nonzero
selects `+4`, and the shift mode comes from byte `+8`. The selected base
word and mode enter repeater-shift conversion and the TX limit check.
`h9_main_rf_tx_input` ports both source choices. The flag's mode meaning
is not yet established. Neither trace
measures the programmed RF frequency or establishes the CHB-Lock fault's
hardware cause.

The path also calls `FUN_01E596E4`, which invokes `FUN_01E50804(1)` and `FUN_01E50820(1)`. `FUN_01E50820` toggles bit 0 of a register returned by `FUN_01E001BA(0)`; it is also called from other audio/control paths. The bit's physical role is therefore unproven. This trace does not establish which instruction first asserts PTT or whether RF setup is safe to run unkeyed.

A closer read of the corrected Pi32v2 project and an independent decode of
app+`0x050678..0x05071F` show what the first call does. `FUN_01E50804(1)`
calls `FUN_01E50778(0, 1)`, then mirrors 1 to RAM `0xC1BB`.
`FUN_01E50778` reads RF register `0x33`, clears bits from mask `0x4040`,
sets bit `0x40`, writes register `0x33`, then clears bits `0x0F00` in RF
register `0x35`. The same function supports selectors 0..6 with shifted
masks and clears one nibble in register `0x34` or `0x35`. The replacement
`h9_main_rf_control_words` ports only this calculation and does not write
hardware. The stock TX path makes this change, then enables MMIO GPIO bit 0,
then programs frequency registers `0x38/0x39` through `FUN_01E59094`, and
later enables MMIO GPIO bit 8 and PWM bit 8. None of the RF-register or GPIO
outputs is yet identified electrically. In particular, the stock setup must
not be used wholesale as the replacement's unkeyed `prepare_tx` callback.

The independent Pi32 decoder later confirmed that `FUN_01E58F58` sets or
clears bit 8 of MMIO `0x001E5000`; Ghidra mistranslated its clear branch.
`FUN_01E58F80` also sets or clears bit 8 of `0x001E0600` after writing
a duty register. Both are now ported as isolated low-level primitives. See
[the register trace](main-rf-output-controls.md). Neither bit is yet
identified as physical PTT.

The replacement `h9_main_tx_service` deliberately requires an *unkeyed* preparation callback, side/frequency readback, live RF route snapshot, and cleanup callback. The snapshot checks Dual Watch route and working frequency copies against the plan. Those are proposed replacement safeguards. A board adapter must be based on further Pi32 register/ROM mapping and RF bench evidence before the service can transmit. See [the route-readback trace](main-aprs-route-readback.md).
