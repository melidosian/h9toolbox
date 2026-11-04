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

The path also calls `FUN_01E596E4`, which invokes `FUN_01E50804(1)` and `FUN_01E50820(1)`. `FUN_01E50820` toggles bit 0 of a register returned by `FUN_01E001BA(0)`; it is also called from other audio/control paths. The bit's physical role is therefore unproven. This trace does not establish which instruction first asserts PTT or whether RF setup is safe to run unkeyed.

The independent Pi32 decoder later confirmed that `FUN_01E58F58` sets or
clears bit 8 of MMIO `0x001E5000`; Ghidra mistranslated its clear branch.
`FUN_01E58F80` also sets or clears bit 8 of `0x001E0600` after writing
a duty register. Both are now ported as isolated low-level primitives. See
[the register trace](main-rf-output-controls.md). Neither bit is yet
identified as physical PTT.

The replacement `h9_main_tx_service` deliberately requires an *unkeyed* preparation callback, side/frequency readback, and cleanup callback. Those are proposed replacement safeguards. A board adapter must be based on further Pi32 register/ROM mapping and RF bench evidence before the service can transmit.
