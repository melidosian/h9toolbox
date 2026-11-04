# APRS controller status reply

Source: APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
The Thumb routine at `0x0800586C..0x08005A82` constructs 48 bytes on its
stack and sends serial command `0x13` through `0x08004894`. All addresses in
this note refer to the controller image, whose inferred flash base is
`0x08002000`.

Bytes 0 through 39 copy ten 32-bit little-endian words from RAM
`0x20001624..0x2000164B`. The routine first calls `0x08003890` for a tick
count ([tick trace](aprs-systick.md)), then subtracts the word at
`0x20001644` from it and narrows to 16
bits. Byte 40 copies RAM `0x2000164C`. Bytes 41 and 42 both copy the low
byte of the halfword at RAM `0x20000130`; byte 43 copies its high byte.
Bytes 44 and 45 copy RAM `0x20000132` and `0x20000133`. Bytes 46 and 47 are
the low and high bytes of the narrowed tick difference. The repeated low
byte at 41 and 42 is present in the stock instructions; it is not a presumed
16-bit field layout.

`replacement-source/aprs/h9_aprs_status.*` implements this byte layout from
an explicit snapshot. The host test sends the encoded record through command
`0x13` and parses the resulting frame. The semantic meanings of the ten
words and four trailing state bytes, and the sources that update those RAM
locations are not fully traced. The stock startup data initializes the tick
step to 1 ms; see [the startup trace](aprs-startup.md). The diagnostic board
entry therefore still leaves its `read_status` callback unset; it must not
invent a live status value from zero-initialized replacement state.
