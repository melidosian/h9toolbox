# Takeover firmware: reclaim stock code and RAM (hybrid of routes 1 and 2)

Goal: when our program replaces stock `my_app` (as Snake and C-hello do),
most of the stock radio code and its RAM use become dead weight. Reclaim
both: **flash** for our resident code (route 1) and **RAM** for programs
loaded from the 16 MiB resource chip (route 2), aiming at large programs
(eventually Doom-class) while keeping the stock startup, drivers and
interrupt handling we still depend on.

## What we keep

- Stock reset, clock and `app_core` setup up to the `my_app` call.
- The RAM-code image (`0x0–0xDA77`, copied at boot) and the IRQ vector table.
- Drivers the program uses: panel and controller setup, LCD transfer
  (`0x01E4BC8E`/`0x01E4BD0E`), key scan (`0x01E515AC`), delays, the resource
  flash reader/writer, and (while it stays useful) the Bluetooth service.
- Anything those reach, directly or through interrupts and ROM callbacks.

## Phase 1: find dead code (offline, done as a first estimate)

[`reachability.py`](reachability.py) walks the stock call index from the
reset entry, cuts edges out of `my_app`, and treats every function whose
address appears in the image as live (48 such). First result:

- 1,560 call-index functions: 682 live, 878 not reached.
- About **389 KB not reached** out of 811 KB of flash code below the RAM-code
  image. Largest contiguous candidate runs: 75,748 B at app+`0x65EE0`,
  32,760 B at `0x1C65C`, 23,902 B at `0x2642A`, 17,212 B at `0x33DE`,
  16,974 B at `0x46326`, …

This is an estimate, not proof. The index resolves only direct calls (1,477
indirect jumps are unresolved), extents run from one call target to the next
(so they include inline data), and the ROM can call back into the app
through tables set up at run time. Large candidate runs may be the Bluetooth
stack, audio or the scheduler, which the kept code may still need.

## Phase 2: prove regions unused on hardware ("trap test")

For a candidate region, build a takeover image (C-hello as the program)
where the region is overwritten with a trap: every 2 bytes decode to a jump
into a small handler that draws a red screen with the trapping address and
blinks the LED. Run the demo, use Bluetooth, idle. If no trap fires, the
region is unused in takeover mode and can hold our code. Regions are tested
largest first; each test costs one USB flash.

## Phase 3: RAM in takeover mode

Stock `.bss` (`0xE2E0–0x1E4D3`) and the heap belong to code that no longer
runs, but interrupts, `app_core` and Bluetooth may still use parts. Measure
with the RAM-read service in a takeover image (if Bluetooth still runs in
takeover mode), or with a C program that paints candidate RAM and checks it
later. Expected outcome: a large RAM region (tens of KiB or more) for loaded
programs, far beyond the 8 KiB slot needed while the stock radio runs.

## Phase 4: resident loader plus loaded programs

Put the loader (see `../resource-loader/DESIGN.md`) and a small runtime (LCD,
keys, timing, resource-flash reads, a frame-buffer blit) into reclaimed flash.
Programs are C, built with the Docker toolchain, linked for the reclaimed RAM
region and stored in `resource` (2.9 MiB free), written over Bluetooth and
started from a menu or at boot.

## Prerequisite

C-hello must boot (it proves C, the ABI and the API wrappers on hardware).
