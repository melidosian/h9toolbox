# H9 C hello: the first C firmware for the TD-H9

A bootable experiment whose drawing and input logic is **C**, compiled with
JieLi's Pi32v2 clang in Docker ([../toolchain](../toolchain)). Everything
hardware-specific is the startup proven by the stage-23 Snake trial
([replacement-source](../replacement-source)): stock reset and `app_core`,
the stock pin registrations and controller setup, the panel reset and init
table, the backlight descriptor and the LCD transfer path. `entry.s` copies
that code unchanged and then calls `h9_main()` in [main.c](main.c).

On boot it draws a full-screen gradient and a white square. **Up**, **Down**,
**Menu** (left) and **Back** (right) move the square, and the LED toggles on
each press. Like stage 23 it replaces stock `my_app`: the radio is not a
radio while this is installed. Reflash H9 Toolbox Firmware (or stock) over
USB to go back.

## C API (entry.s)

| Function | Does |
|---|---|
| `h9_lcd_window(x0, y0, x1, y1)` | open a pixel window (x 0–159, y 0–127) |
| `h9_lcd_write(buffer, length)` | send ≤128 bytes of big-endian RGB565 |
| `h9_key()` | stock key scan; 0 none, 13 Up, 14 Down, 12 Menu, 15 Back |
| `h9_delay(units)` | the stock delay |
| `h9_led(on)` | the GPIO 9 LED |

Calling convention (JieLi clang): arguments `r0–r3`, result `r0`, `r4–r12`
preserved; the wrappers save `r4–r14`.

## Build

```
export JIELI_TOOLCHAIN=~/path/to/jieli-linux-toolchains-20260730.1
python3 -I build.py --stock-image <stock app.bin> --stock-package <TD-H9-V1.0.38.fw>
```

The builder checks that the C object needs only the five API symbols and has
no data sections, links it after `entry.o` into the stage-23 cave
(app+`0xA77F0`), patches the `my_app` hook (app+`0x088180`), repacks the
official package, and requires exactly the stage-23 sectors (`0x5000`,
`0x8D000`, `0xAC000`, `0xD9000`). The build is reproducible, including the
C compile.

| Item | Value |
|---|---|
| Package SHA-256 | `dd24a8adca9540d8fe80bba7f24f136888857c5f551d2a5fcf584e92b979afe0` |
| Main flash SHA-256 | `e12e2700d1ccc354339a277514b2b394016b2011a4c4697017ff5ca3d12a7ad2` |
| Code | 1,062 bytes, `h9_main` at `0x01EA7C4A` |

Not yet run on a radio.
