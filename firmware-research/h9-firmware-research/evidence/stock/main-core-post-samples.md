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

It calls `0x021127B4(0x12F40,0,0xA4)` to zero 164 bytes, writes the
self address `0x12F38` into RAM words `0x12F38` and `0x12F3C`, calls
`0x02002CAE(0x12F38)`, then `0x02002948(0x12F90)`. Next,
`0x01E2D022(0x01E4A970,0,0x01EB17DF)` controls the only branch: a
nonzero return calls `0x01E4A674`. Stock finishes with
`0x02002B66(0x12F90)` and a tail call to `0x01E5176E` using the byte
at RAM `0x12188` as its first argument.

The wrapper at app+`0x024E92..0x024E9C` simply passes `0x12940` to
ROM `0x02002CAE`. Its slice hashes to SHA-256
`d0251b671345f9bcedb13a5b701224ca4dfab1c451d92effd6869a9f36d7e2f8`.
The wrapper at app+`0x04A574..0x04A578` tail-calls ROM `0x0200010A`
without changing the task-launch return in `r0`. Its four bytes hash to
`8d2dc18219c4cd47323a1b57a5cb80b25581565311dfee263ee9cc6cf37b9277`.
The string at `0x01EB17DF` is `dev_mg`; `0x01E2D022` is the task-registry
launcher already modeled by `h9_main_task_launch`. The app+`0x02CF22..0x02CF66`
launcher slice hashes to SHA-256
`d020b759cb79c94e2a92f7cf1fe1208deb8d47064e4dae615f3fb3fa3610426e`.
On task-creation failure, that launcher reports the failure internally and
returns `-14`; the outer caller then invokes the error thunk again. The
replacement uses the source launcher with a caller-supplied `dev_mg`
entrypoint, which is not yet implemented.

The replacement source preserves the order and known RAM writes, performs
the zero fill directly, and bypasses the two wrappers. Other calls remain
explicit hooks identified by stock address. This source is not a runnable
`app_core` task or a flashable firmware image.
