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
next call begins at app+`0x05FE5C` and is not yet ported.
