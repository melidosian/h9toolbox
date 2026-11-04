# Main task registry after board setup

Source: stock v1.0.38 main `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Addresses use the inferred `0x01E00100` application base. This is a static
Pi32v2 r3 decode plus direct inspection of the image bytes.

The table at app+`0x0A2A64` has 27 12-byte records followed by a zero record.
Each nonzero record contains an application pointer to a NUL-terminated task
name, a byte at offset 4, and halfwords at offsets 6 and 8. The whole 336-byte
region hashes to SHA-256
`eb8bc023072f00c68df643283131a9bc5c9adf5699e291ac6b591965a1c37143`.
The first two names are `app_core` and `my_app`; later entries include
`sys_event`, `btstack`, `audio_dec`, `ui`, `key_tone`, and `mic_stream`.
`main/h9_main_task_registry.*` reconstructs all 27 names and these three
fields in order. The field meanings remain unverified.

At app+`0x02CF22`, the local helper walks this table in 12-byte steps and
compares a requested name with each record's name. Its branch reaches the
record-handling call only when the comparison returns zero; it returns zero
when that call succeeds. A missing name or nonzero result takes an error path
that returns `-14`. The corrected Pi32v2 Ghidra project and independent
instruction decode agree that the record-handling call is ROM `0x02003102`.
It receives the caller's entry and argument in `r0/r1`, the record byte at
offset 4 in `r2`, the halfword at offset 6 in `r3`, and the halfword at offset
8 and requested name as two stack arguments. The field meanings and ROM
side effects have not been measured on hardware. `h9_main_task_launch`
ports this lookup/call/error boundary with injectable hooks and a target
ROM wrapper.

Immediately after the memory-window helper, the stock board entry calls
ROM `0x0200391A` and `0x02002CAE`, then calls app+`0x02CF22` for `app_core`
with entry address `0x01E66748` and for `my_app` with entry address
`0x01E88280`. Both calls pass zero in `r1`. It then calls ROM
`0x02003180` and `0x02001650` before an `idle` loop. The exact startup
behavior of those ROM entries and the task bodies is not ported. The
diagnostic main entry still stops before the first of these ROM calls.

The argument to `0x02002CAE` is RAM `0x12850`: the board function sets
`r8=0x11AD0` earlier at app+`0x05FD9E`, then adds `0xD80` at app+`0x05FFB0`.
The Ghidra pseudocode reduces that argument to `0x50`; the instruction
listing preserves the full address. Whether intermediate ROM calls preserve
`r8` as assumed by the stock sequence needs runtime validation.

## First task entrypoints

The two entrypoint addresses were seeded temporarily in the read-only fixed
Ghidra project and checked against an independent instruction decode. Stock
`app_core` at app+`0x066648` calls four initializer ranges in this order:
three pointers at `0x01E2DC28..0x01E2DC34`, one at
`0x01E2DC40..0x01E2DC44`, local app+`0x060AF8`, three at
`0x01E2DC1C..0x01E2DC28`, and three at
`0x01E2DC34..0x01E2DC40`. The apparent fourth range at
`0x01E2DC44..0x01E2DC44` is empty. The ten pointer values, in table order,
are app+`0x05E43E`, `0x048A46`, `0x0462F8`, `0x05F790`, `0x02D4D8`,
`0x0252B8`, `0x0906E4`, `0x09053E`, `0x090332`, and `0x02C514`.
The 40-byte table hashes to SHA-256
`c13b0a8803bd74a2ca27375475e4fe76e96531a46231a5e2c70d0f01c4fac2bf`.
After further setup, `app_core` sets RAM `0x11B35` to state 10 and loops over
state handlers, including app+`0x0665B2` for state 10. The initializer
functions, state handlers, and event scheduler are not yet ported.

Stock `my_app` at app+`0x088180` begins with delays, port/configuration
calls, and radio-device writes. Later in its analyzed body it constructs the
32-byte APRS serial-open record with pins `0x20/0x21`, 2,048-byte RX storage,
and 1,024-byte read chunks, matching the separately traced main APRS UART
path. The fixed Ghidra project reports unresolved instructions near
`0x01E886BA` and `0x01E888B8`, so its full pseudocode is not a reliable
whole-task implementation plan. The entrypoint still needs a bounded
instruction/control-flow reconstruction before it can replace the stock task.
