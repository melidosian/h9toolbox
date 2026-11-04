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

The path then reads MMIO `0x100100`, conditionally extracts two bits from
`0x1E511C`, and calls ROM `0x02000058` with selector `148` before a larger
branching board setup. The meaning of the selectors and resulting board
state has not been established. `replacement-source/main/h9_main_post_clock_rom.*`
ports the first four steps with injectable ROM callbacks. The diagnostic
board entry stops after that block; the later MMIO and selector-148 path
have not been implemented or tested on hardware.
