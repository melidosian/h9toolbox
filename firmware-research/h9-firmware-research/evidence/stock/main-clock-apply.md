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

After the ROM mode helper and repeated selector, stock continues in the
same clock helper at app+`0x0C55AA..0x0C563E`. Independent Pi32 decoding
shows this order:

1. Write the observed masks and values to MMIO `0x1E0040` and `0x1E0044`,
   including the final `0x26` low-field value.
2. Call the source writer at app+`0x0C52D4`; set bit 6 and clear bit 7 of
   `0x1E0014`; call the latch helper at app+`0x0C5308`.
3. Set bit 0 of `0x1E0040`, delay 10 NOPs, set bit 1, then delay 20 NOPs.
4. Call the auxiliary helper at app+`0x0C5330`, set bits 6–8 of
   `0x1E000C`, and call ROM `0x02001650`.
5. Clear bits 10–11 and then bits 2–3 of `0x1E0010`, and return.

`replacement-source/main/h9_main_clock_apply.*` implements both the first
transition and this final sequence. It uses the separately ported source,
latch, and auxiliary helpers. Injected register and ROM callbacks allow host
tests to check the intermediate write/call order; the Pi32 link check
verifies the ROM target. The diagnostic board entry now runs through the
helper's return. The ROM routines' internal behavior and electrical clock
state have not been measured; the linked image remains nonflashable.
