# Main transmit frequency conversion and precheck

Source: pinned TD-H9 main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The corrected `H9mainFixed` Ghidra project was inspected at app offsets
`0x0569E2`, `0x056612`, and `0x05AC68`. The independently decoded TX
setup is under ignored `output/stock/main-tx-band-independent.asm`.

At app+`0x0569E2`, stock converts a channel frequency to the TX frequency.
When RAM `0x00011AEB` is zero, shift mode 1 subtracts offset
`0x00011C98` and mode 2 adds it. Other modes keep the channel frequency.
When `0x00011AEB` is nonzero, no shift is applied. The values are in
10 Hz units; the TX setup at app+`0x05AC68` passes this result to RF
frequency programming and compares it against band thresholds.

The first TX setup gate rejects values below 13,600,000 (136 MHz).
The 200–300 MHz interval requires RAM flag `0x00012BDC`; 300–400 MHz
requires `0x00012BDD`; and 500 MHz or above requires `0x00012BDE`.
These flags were read from the stock function, but their menu sources
and hardware implications are not yet mapped.

RAM `0x00012B9C` selects an additional region gate. Value 1 admits
144–148 MHz and 420–450 MHz inclusive. Value 2 admits only the 30
10-Hz-unit frequency words at virtual address `0x01EB5078`, corresponding
to app offset `0x0B4F78`. The table contains channels in the 462/467 MHz
range. `verify_source_inputs.py` compares every ported word with the
pinned image. Value 0 bypasses the extra region gate; the replacement
rejects other selector values instead of silently accepting them.

`replacement-source/main/h9_main_tx_frequency.*` ports these gates and
the shift operation. It also rejects invalid shift modes, zero and
overflowing results, and Hz inputs not divisible by 10. Those are
replacement safeguards. `h9_main_tx_service` now requires a frequency
precheck callback before RF preparation. The board adapter must supply
the active region and band flags to that callback; the host test does so
with the ported function.

The stock TX setup next calls `FUN_01e56712` at app+`0x056612`, which
uses additional RAM limits. Its Ghidra decompilation truncates at a bad
instruction in the branch for argument 1, and its full behavior has not
been ported. The precheck does not prove an emitted frequency or resolve
the stock PTT assertion point. No radio transmit was performed.
