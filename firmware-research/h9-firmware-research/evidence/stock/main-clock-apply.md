# First main clock-apply transition

Source: pinned main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The independent Pi32v2 r3 decoder and a read-only Ghidra export were used
for the app-relative instructions and the short delay helper.

After the source match and timing calculation, the board entry performs the
following sequence at app+`0x0C558A..0x0C55A2`:

1. Calls ROM `0x02001634` with no explicit arguments, then executes `csync`.
2. Clears bits 2 and 3 of MMIO word `0x1E000C`, then sets bit 1.
3. Calls app helper `0x01E4AB58` with argument 100. Its thunk branches into
   a loop that executes one `nop`, decrements the argument, and repeats until
   zero.
4. Clears bits 6, 7, and 8 of `0x1E000C`.

The next call at app+`0x0C55A2` targets `0x01EC5388`, now mapped in
[the ROM mode trace](main-clock-rom-mode.md). A further call at
app+`0x0C55A6` re-runs the already mapped register selector at
app+`0x0C5492`.

`replacement-source/main/h9_main_clock_apply.*` implements the first four
steps with injected ROM, sync, delay, and MMIO operations. Its target binding
uses the observed ROM and MMIO addresses and Pi32 `csync`/`nop`
instructions. Host tests verify callback and register-write order. The
diagnostic board entry now calls this stage after the timing calculation,
then calls the ROM mode helper and repeats the selector. The ROM routines'
internal behavior and electrical clock state have not been measured; the
linked image remains nonflashable.
