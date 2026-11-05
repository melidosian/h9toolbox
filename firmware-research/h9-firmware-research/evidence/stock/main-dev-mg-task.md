# Main `dev_mg` task entry

Source: stock TD-H9 v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The 58-byte app+`0x04A870..0x04A8AA` entry hashes to SHA-256
`366d6c1012faa3cdbd84fcb8570833b93edf8479bf60e56d6f3433c5ca2a5c1e`.
The virtual base is the inferred `0x01E00100`. Ghidra's Pi32v2 decode
was checked at the instruction level; this is static evidence.

The task zeros eight stack words once, sets RAM byte `0x11798` to 1
through app+`0x024D44`, and calls app+`0x02C2BC`. That function walks
two 12-byte built-in device initialization records at virtual
`0x01E2473C..0x01E24754`; it invokes a non-null initializer and calls
app+`0x04A574` on nonzero return. The task then passes three literal
names to app+`0x04A7B2`: `sdfile` at app+`0x0B17D4`, `fat_nor` at
app+`0x0B1813`, and `res_nor` at app+`0x0B181B`. It ignores the
registration return values. Finally it calls ROM `0x02002722(0x12F90)`
and loops on ROM `0x02002CD8` with the same 32-byte message buffer.
The latter call's return value is not used.

The registration helper at app+`0x04A7B2..0x04A870` hashes to SHA-256
`00a9a912ae03cde279eb5df6db63f3c17b278cd54188d96d7a1b28dacb284b4c`.
It walks 20-byte driver records at virtual `0x01EA28E0`; the four records
and a null terminator hash to SHA-256
`98284a1d9d211728ec88948455d2b7947ef56c605e42df0c83855a21cd069f37`.
The first three record names are the three mounted by the task. It returns
`-1` for a null name, `4` for an unknown name, `1` for an already
registered name, and `3` if the 24-byte node allocation fails. The
second lock covers the driver-open call and list insertion. The driver
opens with mode `3` and option `0`; the returned handle goes at node+12.
The node stores the driver-record pointer at +8 and a nonzero-handle flag
at +16. app+`0x04A79C` increments a sequence word through base
`0x11AD0` plus `0x544 << 2`, or RAM `0x12FE0`; its prior value goes
at node+20. The node is inserted even if the open handle is zero, then
the helper returns `5`; successful open returns `0`.

The replacement `h9_main_volume_registry` ports this list and its result
codes with injected locking, allocation, and driver opening. Its sequence
counter is local until the target RAM binding exists. The actual driver
open routines, allocator, and fourth driver record remain unported.
`h9_main_dev_mg_task` preserves entry ordering and the message loop with
callbacks for those services. Neither module is connected to a runnable
board entry; no target execution was attempted.
