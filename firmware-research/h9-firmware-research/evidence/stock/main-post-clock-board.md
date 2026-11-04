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
`55` branch to an alternate board path. For `0..55`, it calls the helper at
app+`0x05FA44` on the two all-ones local words. The helper uses the index's
upper bits to select one of four halfwords and clears bit `index & 15`, so
the pair acts as a 64-bit bitmap. The replacement applies the equivalent
word-level bit clear and records whether this branch continues or diverts.
The source verifier pins the branch and helper bytes to the stock image.
The purpose of this bitmap and the subsequent board actions are still unknown.
