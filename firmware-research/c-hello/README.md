# H9 C hello: the first C firmware for the TD-H9

A bootable experiment whose drawing and input logic is **C**, compiled with
JieLi's Pi32v2 clang in Docker ([../toolchain](../toolchain)). Everything
hardware-specific is the startup proven by the stage-23 Snake trial
([replacement-source](../replacement-source)): stock reset and `app_core`,
the stock pin registrations and controller setup, the panel reset and init
table, the backlight descriptor and the LCD transfer path. `entry.s` copies
that code unchanged and then calls `h9_main()` in [main.c](main.c).

On boot it measures the hardware, draws a full-screen gradient, and shows a
white square that **Up**, **Down**, **Menu** (left) and **Back** (right)
move; the LED toggles on each press.

## On-screen readouts

All in ticks of the stock timer-3 interrupt counter (RAM `0x1D8B0`):

| Line | Measures |
|---|---|
| FILL | ticks to draw the full 160 × 128 gradient (display throughput) |
| SQ | ticks for sixteen 16 × 16 squares (per-transfer overhead) |
| LOOP | busy-loop iterations per tick (a CPU speed estimate) |
| DLY | ticks taken by `h9_delay(100)` (the stock delay unit) |
| TICK | live counter (bottom, yellow): note it twice about 10 s apart on a stopwatch to get ticks per second |

If the counter never moves, the screen shows `TICK STUCK`.

## Preview on the Mac

`test/host_preview.c` compiles the same `main.c` natively with stand-in
hardware calls that draw into a 160 × 128 framebuffer (and fail on any
out-of-window or oversized write), then saves the screen:

```
cc -std=c99 -Wall -Werror -DH9_HOST_TEST -o build/preview test/host_preview.c main.c && build/preview build/preview.ppm
``` Like stage 23 it replaces stock `my_app`: the radio is not a
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
| Package SHA-256 | `941f40c2dc87fde4c29fcdd94ab09cba611a984f6bfeffb09cf268c7ed69f4f5` |
| Main flash SHA-256 | `941e25869feeecdc6f37be4f19eb6a1c7e647ebfdba9bb17f76037ad4fec314e` |
| Code | 1,680 of 1,704 bytes, `h9_main` at `0x01EA7D56` |

Not yet run on a radio.
