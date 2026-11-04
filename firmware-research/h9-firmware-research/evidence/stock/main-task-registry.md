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
that returns `-14`. The call target and exact argument contract still need
independent confirmation before connecting it to the replacement scheduler.

Immediately after the memory-window helper, the stock board entry calls
ROM `0x0200391A` and `0x02002CAE`, then calls app+`0x02CF22` for `app_core`
with entry address `0x01E66748` and for `my_app` with entry address
`0x01E88280`. Both calls pass zero in `r1`. It then calls ROM
`0x02003180` and `0x02001650` before an `idle` loop. The exact startup
behavior of those ROM entries and the task bodies is not ported. The
diagnostic main entry still stops before the first of these ROM calls.
