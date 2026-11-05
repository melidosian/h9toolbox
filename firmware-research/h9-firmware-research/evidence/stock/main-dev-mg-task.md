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

The registration helper at app+`0x04A7B2` looks up a name in a 20-byte
driver record table, checks the existing list under a lock, allocates 24
bytes, opens the driver, and inserts a node. Its full driver table and
allocator/open semantics are not ported. The replacement
`h9_main_dev_mg_task` preserves entry ordering and the message loop with
callbacks for those services. It is not yet connected to the task registry
or a runnable board entry; no target execution was attempted.
