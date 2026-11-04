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
- `07 01` → app+`0x05040A(0)`
- `07 03` → app+`0x062C58()`

The group `07` correction comes from the independent Pi32v2 decode at
app+`0x087130`: `if (r1 == 7)`, followed by `if (r1 != 6)` for the other
group. The earlier `0A` reading was incorrect. The `0x078E94` routine reads
the level at RAM `0x11B16`, adds `+1` or `-1`, sets pending-display byte
`0x11D1C`, and clamps the result to `0..30`. At a boundary, it resets an
active timer to 150 or requests tone `7` if no timer is active. When the
new level differs from both the current byte and setting `0xC0F9`, it
updates both and invokes audio/UI routines whose board effects need more
tracing. The `0x087086` wrapper for
`06 05/06` stores held-adjustment state `1/2`, calls the same step routine,
cancels a previous timer, then starts a 200-unit timer. The `06 02/04` paths
cancel that timer and clear the held state. The timer unit is inferred to be
milliseconds from its API; the physical timing has not been measured.

For `07 03`, app+`0x062C58` clears bits 6 and 7 of RAM `0x12674`. If
RAM `0x12C53` is zero, it sets that byte to 1 and clears `0x12C54`.
If the timer handle at `0x12C60` is zero, it starts a 50-unit timer with
callback app+`0x08A0DC` and stores the returned handle at **`0x12C60`**.
The independent Pi32v2 decode confirms the address: base `0x11AD0` plus
`1124 << 2`. Ghidra's pseudocode incorrectly prints `0x47CD0` for the
same store. The callback passes the low 16 bits of the handle to
app+`0x0248E4`, clears the handle, then calls app+`0x06162C(2)` for
parameter zero or ROM `0x0200010A` for parameter one. Those downstream
effects and the timer unit are not yet identified. The ported reducer
returns explicit timer and callback effects; a board adapter must apply
them and store the returned timer handle.

The replacement source validates the CRC and returns the recognized pair.
It also supplies a pure `0..30` volume/held-state reducer with timer and
boundary-feedback effects, plus the `07 03` state and timer transition,
for a future board adapter. It does not model the
separate saved-setting comparison or downstream audio/UI calls. No hardware
is invoked by this reducer, and the session still only emits the
raw action. Host tests cover CRC, all eight pairs, escaped serial payload,
corruption, the unsupported `0A 01` pair, and volume/timer transitions at
both boundaries. This is offline source analysis, not runtime validation.
