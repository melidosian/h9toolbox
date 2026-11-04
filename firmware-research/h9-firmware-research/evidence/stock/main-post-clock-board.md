# Board path after the main clock helper

Source: pinned stock main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The addresses below come from an independent Pi32v2 r3 decode. Relative
`call` targets include the instruction's four-byte PC adjustment.

At app+`0x05FC20`, the board routine calls the clock helper at
app+`0x0C54B2`. The next instructions at app+`0x05FC24..0x05FC42` are:

1. Call ROM `0x02000058` with selector `49`, then sign-extend its low byte.
2. If that byte is nonnegative, call ROM `0x02000058` with selector `51`.
3. If bit 7 of the second result is set, call ROM `0x020008FE` with
   arguments `(17, 1)`.
4. Initialize the following pair of local words to `0xFFFFFFFF`.

The path then reads MMIO `0x100100`. If its bit 0 is set, it extracts bits
21–22 of `0x1E511C` into a local flag; otherwise that flag remains `-1`.
It calls ROM `0x02000058` with selector `148`, then reads bits 8–11 of
`0x1F0200`. For values 0–9, mask `340` selects indices 2, 4, 6, 8 and
writes mode byte `2` to RAM `0x11B43`; mask `680` selects indices 3, 5,
7, 9 and writes mode byte `4`. Indices 0 and 1, and values above 9, branch
to app+`0x05FFE8`, set mode `1`, and rejoin at app+`0x05FC9A`. This fallback
was missed by the first source port. All three paths clear bits 1–7 of the
selector-148 result, normalize the local flag to either 1 or 0, and read
`0x1F0500`. The next branch tests the masked selector result and enters a
larger board setup.

`replacement-source/main/h9_main_post_clock_rom.*` ports the initial ROM
block and this classifier through app+`0x05FCB2`, with injectable ROM and
MMIO. Host tests cover all 16 index values, including fallback mode `1`,
and the flag normalization. The diagnostic board entry continues into the
bitmap branch described below; the meaning of the modes has not been
established on hardware.

At app+`0x05FCB2`, a zero masked selector-148 result skips the next ROM
read. Otherwise, the board routine reads ROM selector `192`. Values above
`55` branch to app+`0x05FFEC`. Value `56` sets the stock `r6` condition,
value `57` sets `r9`, and other values above `55` leave both as initialized.
Every case rejoins at app+`0x05FCD4`. For `0..55`, the helper at
app+`0x05FA44` uses the index's upper bits to select one of four halfwords
and clears bit `index & 15`, so the pair acts as a 64-bit bitmap. The
replacement applies the equivalent word-level bit clear and preserves the
two later-board condition flags. The source verifier pins the branch,
special-value handler, and helper bytes to the stock image. The hardware
meaning of these flags and the subsequent board actions are still unknown.

At app+`0x05FCD4..0x05FCEA`, stock starts the next ROM handoff. It selects
mask `0xFFFFFE3B` if bit 0 of the captured `0x1F0500` word is set, otherwise
`0xFFFFFFFF`, and passes it to ROM `0x02000A78`. It duplicates the returned
`r0` into `r1`, calls ROM `0x02000D18`, and branches on that call's `r0` and
`r1` results. `replacement-source/main/h9_main_post_clock_rom.*` now ports
this handoff through injectable callbacks and a Pi32v2 target binding. The
linked target wrapper passes the transformed word in both argument registers
and captures the two result registers as a 64-bit return. The diagnostic
board entry acts on the four observed result branches below. Their hardware
meaning has not been established on the radio.

At app+`0x05FCEA..0x05FD74`, the returned `r0` and `r1` select four
descriptor routes. If `r0 == 4`, nonzero `r1` narrows the input mask to
`0x1FFF` and chooses control word `0xFFFF0033`; zero `r1` leaves the mask
alone and chooses base/control words `0xFFFF0080`/`0xFFFF003F`. For other
`r0` values, nonzero `r1` looks up descriptors 15 and 53, clears bits
`0x8000` and `0x20` in each descriptor's words at offsets `+16` and `+20`,
narrows the input mask to `0x9FFF`, and chooses `0xFFFF0013`. Zero `r1`
instead uses descriptor 23, bit `0x80`, plus descriptor 53, bit `0x20`, and
chooses `0xFFFF001F`. The lookup helper at app+`0xBA` accepts indices
`0..56`; its table at app+`0x554` contains MMIO descriptor bases
`0x1E5000`, `0x1E5040`, `0x1E5080`, and `0x1E50C0`. The replacement encodes
that table as a bounded address calculation, since it cannot call a helper
in the stock app after replacement. Host tests cover all four branches and
missing descriptors; the target link verifies the lookup and board call
order. The first four common mask writes beginning at app+`0x05FD74` are
described below. None of these paths has been exercised on hardware.

At app+`0x05FD74..0x05FDA6`, the stock code calls the app+`0x48E` and
app+`0x51A` descriptor helpers to AND four words in order: descriptor 0
word `+20` with `~filtered_mask | 0xFFFF0000`, descriptor 0 word `+32`
with the complement of the bitmap's low 16 bits, descriptor 16 word `+20`
with the route's base word, and descriptor 16 word `+32` with the complement
of the bitmap's next 16 bits. The bounded lookup helper returns MMIO bases
`0x1E5000` and `0x1E5040` for these indices. The replacement now performs
these four read-modify-write operations in the same order, and its diagnostic
entry continues into the descriptor-32 branch below.
The stock instruction slices and helper tails are pinned by the link check;
host tests verify the masks and register words.

At app+`0x05FDA6..0x05FE04`, the code looks up descriptor 32. If found, it
retains the high 16 bits of word `+12` and replaces the low 16 bits with
`0x0040`. Before that write, bank-status word `0x1E0200` low nibble `0..2`
and a descriptor pointer equal to `0x1E5080` trigger three bit-6 clears at
descriptor offsets `+16`, `+0`, and `+8`, then a call to app+`0x44C84` with
argument zero. That local routine compares RAM byte `0xD975` to zero. On a
change, it clears the byte and updates control words at `0x1E19C0`;
RAM word `0xD90C` decides whether its first two clears occur. The later
register clears always occur when the byte changes. The hardware purpose
of these state and MMIO fields is unproven, so the replacement names them
by address and ports only the observed zero-argument path. Regardless of
whether descriptor 32 exists, the board code then ANDs descriptor 32 word
`+32` with the complement of the bitmap's third halfword, descriptor 48
word `+20` with the route control word, and descriptor 48 word `+32` with
the complement of the fourth halfword. The target binder, four branch
cases, and missing-descriptor behavior are host-tested. The diagnostic
entry continues into the two flag-controlled blocks below; no hardware
test has run this path.

At app+`0x05FE04..0x05FE5C`, the preserved `r6` and `r9` conditions gate
two similar MMIO sequences. When `r6` is zero, stock writes zero three
times to `0x1E1800`, once to `0x1E1804`, sets bits `0x900` twice and bit
`0x4` once at `0x1E5100`, then clears bits `0x10`, `0x40`, `0x200`, and
`0x2000` there. When `r9` is zero, it repeats the zero writes and `0x900`
sets, sets bit `0x8`, then clears `0x20`, `0x80`, `0x400`, and `0x4000`.
Both blocks are skipped independently when their respective flag is set.
The replacement preserves the volatile write order and takes the flags
from the selector-192 step. Host tests cover all four flag combinations;
the link check pins the stock instructions and verifies the repeated
compiled writes. The purpose of these registers is still unverified. The
next call begins at app+`0x05FE5C`.

At app+`0x05FE5C..0x05FE94`, stock calls ROM `0x020000A0` with
`(0x809B, 0)`, then calls the trampoline at app+`0x031194`. Its backward
jump enters app+`0x031162`, a normal returning setup routine: it registers
interrupt 3 with priority 1 and handler app+`0x03126C`, clears timer word
`0x10F0F0`, loads `0x10F0F4` with signed `RAM[0xD9D8] / 100 - 1`, and writes
`0x41` to `0x10F0EC`. The handler itself saves processor state, updates
timer and scheduler words, and returns with `rti`.
The board routine then reads `0x1E00C0` without retaining the value, reads
ROM selector 49, and writes its low byte to RAM `0x11B40` if bit 7 is
clear. If bit 7 is set, it calls ROM control `(49, 64)`, reads selector 18,
and writes that low byte instead. `replacement-source` now models this
stage with an injected interrupt-registration callback and explicit handler
pointer. Host tests cover both selector branches and timer arithmetic. The
diagnostic target entry now calls the stage and installs its replacement ISR.

The handler at app+`0x03126C..0x0312D2` saves processor status and registers
0–4, increments RAM `0xC0A4`, sets timer-control bit `0x40` at `0x10F0EC`,
and increments RAM `0x1D8B0`. It calls ROM `0x02001D2E`; a zero result
returns immediately. Otherwise it calls ROM `0x02001634`, executes `csync`,
and calls ROM `0x02001E2C`. If that result is nonzero, it compares one word
each at RAM `0xD5E8` and `0xD5BC`, invoking `swi 0` on a mismatch. It then
calls ROM `0x02001650`, restores processor state, and returns with `rti`.
The source ports these branch and register effects in
`main/h9_main_timer3_irq.*`, with injectable ROM operations and a fault
callback. The Pi32 target adds the stock save/restore shell and a specific
IRQ-3 registration using stock table value `1` at app+`0x0A2A25`. Its
linked vector write is `0x2BF0C` and its control field at `0x10F100` is
`0x3000`. The wider clock contract and live interrupt behavior remain
unverified; this linked image is still diagnostic and not flashable.

At app+`0x05FE94..0x05FEAE`, stock passes IRQ indices `0` and `1`, each
with priority argument `2` and handler RAM address `0x1622`, to the same
registration function at app+`0x02465A`. That function writes the handler
pointer into `0x2BF00 + 4*index`, then updates one four-bit field in the
eight control words beginning at `0x10F100`. Its 64-entry mapping table at
app+`0x0A2A22` yields field values `1` for IRQ 0, `15` for IRQ 1, and
`3` for IRQ 3. `main/h9_main_irq_registration.*` ports the whole table,
bounded vector update, and ROM-guarded control write; the target timer-3
stage now uses it instead of a hardcoded IRQ-3 write. The RAM handler at
`0x1622` saves the full register and processor state, calls another routine,
then loops. The four-byte relative call at RAM `0x162E` has displacement
`-1,528,554`; adding the four-byte PC adjustment and wrapping in the Pi32
32 MiB call window yields `0x01E8C348` (app+`0x08C248`). The target reads
saved registers and status, prints diagnostic text, checks stack-guard
words, and continues dumping state. This is a static target inference, not
a tested crash path. The replacement now installs IRQs 0 and 1 with a
RAM-resident exception entry whose first 12 bytes match the stock save
sequence. It records 24 saved words in RAM and halts. The stock printer is
still unported.

At app+`0x05FEAE..0x05FF36`, stock performs ten status handshakes. The
set-wait helper at app+`0x05E340` reads bit 0 of `0x101044` until it is 1;
the clear-wait helper at app+`0x05E354` reads until it is 0. Each helper
first reads without a write, then writes `0xE7` to `0x101044` after every
failed read. Stock has no visible retry bound. The board routine waits for
set, ORs one bit into a control register, then waits for clear, in this order:
`0x101000` gets bits 29, 28, 27, 26, 25, 24; `0x10F0D0` gets bits 0, 1, 2;
`0x10F1C0` gets bit 0. It next calls app+`0x05E368`.

`main/h9_main_clock_handshake.*` ports the ten stages with a configurable
retry limit. The target entry currently uses 100,000 polls per wait and
halts on timeout. Host tests exercise every stage and a stuck status bit;
the Pi32 link check pins both stock byte ranges, checks the mask constants,
and inspects generated pointer assignments. Its adjacent address calculation
and store can issue as a Pi32 parallel pair: that store reads the old base
while the later store reads the new address. The hardware meaning of these
gates and safe timeout length are unverified.

At app+`0x05FF36..0x05FF52`, stock passes app base `0x01E00100` and the
source of its RAM-code copy `0x01EC628C` to app+`0x05E368`. That helper waits
for status bit 0 at `0x101044` to become set, writes the app base to
`0x101074`, and writes the RAM-code source to `0x101070`. The caller then
reads `0x101074`, `0x101070`, `0x10107C`, and `0x101078` in order without
using their values. `h9_main_clock_map_sources` ports this stage with the
replacement linker symbols `h9_main_reset` and `__ram_code_load_start`.
The linked target check verifies those symbols and all four read operations.
At app+`0x05FF52`, the next stock stage sets `r0=0` and `r1=-1`, then
`rep 2 16` applies `[r0++=4] = r1` sixteen times. It overwrites RAM words
`0x00..0x3C` with `0xFFFFFFFF`, exactly the 64-byte prefix left blank in
the copied stock RAM code. It waits for status bit 0 to set, clears bit 10
at `0x101040`, writes `0` to `0x101054` and `0x66BF` to `0x101050`, then
sets bits 11 and 10 at `0x101040` before waiting for status to clear. A
second set-wait clears bit 8, writes `0` to `0x10105C` and `0x66BF` to
`0x101058`, sets bits 9 and 8, then waits for clear. The call targets at
app+`0x05FF5E`, `0x05FF7E`, `0x05FF82`, and `0x05FF96` resolve to the same
status helpers. This sequence ends at app+`0x05FF9A`; its hardware purpose
remains unknown.

`main/h9_main_post_map_gate.*` now models the two handshakes and register
writes. The target calls `h9_main_vector_prefix_ones` first, a Pi32 assembly
routine whose `rep 2 16` fills the 64-byte prefix at RAM address zero. Host
tests check that the prefix fill happens before any status poll, that both
register phases occur in order, and that a failed poll stops later writes.
The target link check pins the stock instruction range and checks the linked
repeat opcode, control addresses, masks, and call order. No radio execution
has validated these control writes; the linked image remains diagnostic.

The next board block at app+`0x05FF9A..0x05FFE4` calls local helper
app+`0x05E37E` with `(r0,r1,r2)=(1,0xDA90,0xE290)`, then ROM
`0x0200391A` and `0x02002CAE`. It calls app+`0x02CF22` twice with different
app pointers, then ROM `0x02003180` and `0x02001650`, before entering an
`idle` loop. The local helper at app+`0x05E37E..0x05E39E` waits for status
bit 0 to set. If its mode argument is zero, it writes `r2` to `0x10F0E0`;
it always writes `r2` to `0x10F0E4` and `r1` to `0x10F0E8`, then tail-jumps
to the clear-wait helper. The board call uses mode 1, `r2=0xE290`, and
`r1=0xDA90`, so `0x10F0E0` is left alone. The replacement now ports both
branches in `h9_main_post_map_window_run` and the diagnostic entry calls
the mode-1 path. Host tests cover both modes and timeout before writes;
the linked Pi32 check pins the stock helper/caller bytes and control
addresses. It stops before ROM `0x0200391A`.

App+`0x02CF22` walks a descriptor table beginning at app address
`0x01EA2B64` and calls further helpers. Its effects and required
replacement resources are not yet traced.
