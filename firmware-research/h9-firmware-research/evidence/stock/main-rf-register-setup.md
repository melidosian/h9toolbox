# Main RF frequency and mode register writes

Source: pinned TD-H9 main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The corrected Pi32v2 Ghidra project `H9mainFixed` was queried at app
offsets `0x058F94`, `0x05914C`, and `0x05AC68`. Independent Pi32v2 r3
decoding is reproducible with `analyze_app.py disasm` at those offsets;
the local reports are in ignored `output/stock/main-rf-frequency-independent.asm`
and `output/stock/main-rf-mode74-independent.asm`.

At app+`0x058F94`, stock reads the band selector from RAM `0x00012B9D`.
For selector 0 it reads a signed calibration halfword at `0x00012138`;
for selector 1 it reads `0x00012136`; other values skip this correction.
It divides that halfword by 10, adds the result to the frequency argument,
then writes the low and high 16-bit words to RF register commands `0x38`
and `0x39` in that order. The independent decoder confirms the two calls
at app+`0x058FCE` and `0x058FD6`. The frequency is in 10 Hz units in this
path: TX setup compares it with 13,600,000 and other band thresholds.

At app+`0x05914C`, stock clears bits 4..7 of the mode argument and maps
values 1, 2, 3, 4, 13, 14, and 15 to words `F61C`, `DF22`, `D42D`,
`CC35`, `FEF0`, `FCFA`, and `FA02`; other values use `F50B`. The destination
register is `0x75` for path 0 and `0x74` for path 1. The TX setup at
app+`0x05AF98` calls path 1 with mode 2, producing a `0x74 = 0xDF22`
write. The independent decoder confirms the mask, destination numbers,
and call site; Ghidra supplies the switch-case values.

`replacement-source/main/h9_main_rf_registers.*` ports these values and
the two frequency-write calls through an injected register writer. It
rejects a zero or out-of-range corrected frequency, which is a replacement
guard. This source does not implement the later setup within stock
`FUN_01e59094` or `FUN_01e5ad68`: Ghidra reports a bad instruction in
the former, and the latter also changes RF outputs before packet send.
The ROM RF register transport at `0x02001634`, the precise PTT assertion
point, and emitted frequency remain unverified. This is therefore a
partial source port and cannot serve as the unkeyed TX preparation adapter
required by `h9_main_tx_service` yet.
