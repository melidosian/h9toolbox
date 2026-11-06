# Breathing LED pin and visible boot signal trace

Source: decrypted stock TD-H9 main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
App offsets below use the inferred `0x01E00100` image base. The Pi32v2
instruction decoder and the corrected read-only Ghidra project were used.
The stock software's **breathing LED GPIO is pin 9**, bank 0 bit 9 at
`0x001E5000`. This is a static code trace; no pin voltage or light output
has been measured on the radio.

## Breathing LED setting to GPIO

The menu handler at app+`0x074570..0x0745AA` stores the option in RAM
`0x12BD5` (base `0x11AD0` plus 4357), writes clone setting offset
`0x0CAF`, and packs the selected value into bits 4–6 of that setting.
The clone image has an eight-byte prefix, so this is clone-file byte
`0x0CB7`, the breathing LED setting. The handler slice SHA-256 is
`9476b08ee47eaf06872e08b0265fc670223c560056e808c55d2d9879d836e735`.

App+`0x0515E8` reads RAM `0x12BD5` and indexes a five-halfword interval
table at app+`0x0B07EA`: 0, 500, 1000, 1500, 3000. The shutdown path
at app+`0x05160A` installs that interval in RAM `0x11CCC` and initializes
the control bytes at `0x11CCE/0x11CCF`. The interval function's
app+`0x0515E8..0x05160A` SHA-256 is
`c2bb78a756476f9c07e42d14783439a331d6ab30270e77ee336c93cad4ade0a8`.

The standby tick routine at app+`0x087F64..0x088020` checks that setting
and the screen-off state. When its timer expires, it calls app+`0x05036A`
with nonzero argument 3, then later calls it with zero. The routine also
updates the timer and control bytes above. Its slice SHA-256 is
`2a04cc9a481a6a2101c3c1e9a7fa7eab6b1a8ec95b0a5da0b1b94044afbb3657`.

App+`0x05036A..0x050392` loads GPIO bank 0 using the stock pin-bank lookup
with argument 9. For a nonzero argument it sets bit `0x200` at
`0x001E5000`; for zero it clears the same bit. The writer SHA-256 is
`f50a245510d71612d856d16fa6056c764a41a526411970e623a56022e038bdb5`.
At app+`0x088686..0x08869A`, startup explicitly selects pin 9, sets bank
offset `+0x0C` bit `0x200`, and clears bank offset `+0x08` bit `0x200`.
This setup slice SHA-256 is
`5d57f5b2916854b00c33797a55b011c490e16f78e221a613957bb0c6bc03bb74`.

## Display commands are not LED pins

At app+`0x05040A`, the screen wake path calls app+`0x04EC8A` with `0x29`.
Independent Pi32 disassembly shows that routine first selects GPIO pin 18
through app+`0x0000BA`, changes its output state, and passes the command byte
to app+`0x04EC6E`. That byte-transfer wrapper calls ROM `0x02001634` around a
local transport call at app+`0x04BEC2`; the ROM call's role is not established
by this trace. A nearby screen-window writer at app+`0x04ECDD` passes `0x2A`,
`0x2B`, and `0x2C` through the same command function. These match the LCD's
display-on, column-address, row-address, and memory-write command sequence.
The `0x29` argument must not be treated as GPIO pin 41.

## Backlight PWM path

App+`0x050252` reads RAM byte `0x12BA9`, indexes the stock byte table at
app+`0x0AA1C8` (`00 05 0A 0F 1E`), doubles the result, and stores the timer
at RAM `0x11B60`. App+`0x05040A` invokes this on screen wake. App+`0x050392`
reads adjacent RAM byte `0x12BAA`, calculates a duty value, and calls the
PWM helper app+`0x079C80` using a two-word handle at RAM `0x1220C`.
App+`0x05160A` uses the same handle to turn that PWM path off. The backlight
pin assignment has not been traced from the handle's initialization.
The backlight reset path also calls the pin-9 helper with zero if the
breathing LED state is active.

## Early pin setup is still ambiguous

At app+`0x088180`, stock calls app+`0x079BE4` for pins 12, 37, 36, 10, and 1
with respective keys 4, 13, 10, 3, and 0. That helper configures GPIO banks
and registers the keys at RAM `0x12A30`; see `main-my-app-early-pins.md`.
These are unrelated candidate output pins; the breathing LED is traced
separately to pin 9. Their functions and the screen backlight pin are
still unassigned.

## Consequence for the boot probe

The early boot probe can use the stock pin-9 setup and a distinctive blink
pattern after writing `H9BT` and its complement to RAM `0xE2E0/0xE2E4`.
Actual light output still requires a physical boot test. The static trace
does not establish whether GPIO clocks are already enabled at the probe's
early execution point.
