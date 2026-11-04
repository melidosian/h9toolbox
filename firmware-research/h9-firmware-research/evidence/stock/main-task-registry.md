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

The third pointer in the first initializer group targets app+`0x0462F8`.
Its 14 bytes load RAM address `0x119EC`, store that address to
words `0x119EC` and `0x119F0`, and return zero. Ghidra and the independent
Pi32 decode agree. `main/h9_main_core_list.*` ports this self-linked list
head using injectable RAM storage and a target address binding. Its stock
slice SHA-256 is
`495683945a63e4d89abee6ab3f394d7e7267fc2bd7f9f76059e013b47aef6583`.
The list's consumers and app-core integration are still unknown.

The neighboring first-group initializer at app+`0x02D4D8` sets a guard byte
at RAM `0xC070`, initializes a list head at `0xC08C`, and allocates `0x38`
bytes for two `0x1C`-byte linked entries. The first allocation failure returns
`-12` and leaves the guard set, so a later call will not retry. If RAM word
`0xC080` is zero, it next requests a `0x124`-byte buffer; success stores the
pointer at `0xC080` and initializes 256 bytes starting 36 bytes into it.
Failure of this second allocation is ignored. The initializer then requests
the `sys_event` task through the same task-launch helper, using stock entry
address `0x01E2D642` (app+`0x02D542`); its task-launch return is ignored. `main/h9_main_sys_event.*`
ports these writes and branches using injected allocation, buffer-init, and
task hooks. The event task body, target allocator, and `app_core` caller are
still unported. The pinned stock initializer slice app+`0x02D4D8..0x02D542`
has SHA-256
`46ce3f88d3e6a114a8127c20334a94845d439be5143da7b2ee91e523f9041cde`.

The task entry at app+`0x02D542` allocates `0x50` bytes, calls a stock
failure helper if that allocation returns zero, then passes the allocation
to ROM `0x02002948` and saves its address in RAM word `0xC084`. It sets up
a 28-byte local workspace and register values `r4=0x11770`,
`r10=0x01E2DC44`, `r15=0x01E2DC5C`, and `r9=0` before handing control to
ROM `0x02001634`. The role of this ROM call and the two table pointers is
not yet established. `h9_main_sys_event_task_prelude` ports the visible
prelude through injectable hooks; it stops on allocation failure if the
stock failure helper returns, rather than handing a null pointer to ROM.
The app+`0x02D542..0x02D57C` task slice has SHA-256
`e1015be090c561d330c99d7bcfa526a164fc0d32278060810b108c3754292301`.
The initializer at app+`0x02C514` walks three 28-byte records at
`0x01E2DC5C..0x01E2DCB0` and calls each nonzero first-word callback.
Their first-word targets are app+`0x02C862`, `0x08C9D4`, and `0x02C54C`.
Those callback bodies remain unported.

Stock `my_app` at app+`0x088180` begins with delays, port/configuration
calls, and radio-device writes. Later in its analyzed body it constructs the
32-byte APRS serial-open record with pins `0x20/0x21`, 2,048-byte RX storage,
and 1,024-byte read chunks, matching the separately traced main APRS UART
path. The fixed Ghidra project reports unresolved instructions near
`0x01E886BA` and `0x01E888B8`, so its full pseudocode is not a reliable
whole-task implementation plan. The entrypoint still needs a bounded
instruction/control-flow reconstruction before it can replace the stock task.
