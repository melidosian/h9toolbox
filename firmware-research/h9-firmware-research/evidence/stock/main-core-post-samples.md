# `app_core` setup immediately after ADC samples

Source: stock TD-H9 v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The app+`0x060E16..0x060E96` slice hashes to SHA-256
`647f7f73b4d96ab6a6e83a5d52fb55404994b9146993ff8ca077dfc66914a4fb`.
Addresses use the inferred app base `0x01E00100`. The corrected Ghidra
`H9mainFixed` instruction listing was checked against the independent Pi32v2
decoder. The Ghidra pseudocode alone is not authoritative for the adjacent
ADC poll, where it disagrees with the vendor disassembler.

After setting the ADC ready halfword, stock calls `0x01E2475A` with
`(23,0,0x01E8A908)`, ROM `0x02003904` with
`(0,0x01E8A826,2,0)`, and `0x01E60776`. It writes `0x01EB1361` to RAM
`0x11BC0`, then calls `0x01E24F92`.

It calls `0x021127B4(0x12F40,0,0xA4)` to clear 164 bytes, writes the
self address `0x12F38` into RAM words `0x12F38` and `0x12F3C`, calls
`0x02002CAE(0x12F38)`, then `0x02002948(0x12F90)`. Next,
`0x01E2D022(0x01E4A970,0,0x01EB17DF)` controls the only branch: a
nonzero return calls `0x01E4A674`. Stock finishes with
`0x02002B66(0x12F90)` and a tail call to `0x01E5176E` using the byte
at RAM `0x12188` as its first argument.

The replacement source preserves this order and the known RAM writes.
Each call is an explicit hook identified by stock address. The ROM and
application services behind the hooks remain unported; this source is not
a runnable `app_core` task or a flashable firmware image.
