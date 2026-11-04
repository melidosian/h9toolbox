# Main APRS serial control command `0x51`

Source: stock v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Ghidra project: `H9mainFixed`; virtual base `0x01E00100` inferred from JLFS
entry metadata. Local decompilation reports:
`output/stock/main-module-uart-dispatch.txt` and
`output/stock/main-command-handlers.txt`.

The incoming UART frame parser at app+`0x087340` supplies a command and decoded
payload to app+`0x0870BC`. For command `0x51`, that dispatcher requires at
least four payload bytes. It compares the last two bytes, high byte first,
against the CRC of all preceding payload bytes. The CRC routine at
app+`0x087030` starts at `0xFFFF`, XORs each byte into the upper eight bits,
then shifts eight times using polynomial `0x1021` on a set high bit. The result
is limited to 16 bits, with no final XOR.

Only after the CRC matches does the dispatcher inspect payload bytes zero and
one as a group and opcode. The observed routes are:

- `06 01` → app+`0x078E94(1)`
- `06 02` and `06 04` → stop a timer when active and clear RAM `0x11B12`
- `06 03` → app+`0x078E94(-1)`
- `06 05` → app+`0x087086(1)`
- `06 06` → app+`0x087086(2)`
- `0A 01` → app+`0x05040A(0)`
- `0A 03` → app+`0x062C58()`

The `0x078E94` routine adjusts an internal value within `0..30`; this appears
related to volume. The other side effects and user-visible names need more
tracing. The replacement source currently validates the CRC and returns the
raw recognized pair. It does not execute any of these effects or change radio
settings. Host tests check the standard CRC vector `123456789 → 0x29B1`,
all eight pairs, escaped serial payload, a corrupted CRC, and an unknown pair.
This is offline source analysis, not runtime validation on the radio.
