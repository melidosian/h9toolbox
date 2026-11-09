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

## Phase 1: find dead code (offline estimate)

[`reachability.py`](reachability.py) walks the stock call index from the
reset entry, cuts edges out of `my_app`, and treats address-taken functions
as live.

- **First pass (wrong):** 389 KB not reached; largest run 75,748 B at
  app+`0x65EE0`. That run contains the `app_core` task entry
  (app+`0x066648`). Task entries are passed to the task launcher as
  immediates and never called directly, and the first pass only counted
  pointers to functions already in the call index. It would have trapped at
  boot.
- **Corrected:** pointers count as roots when they land on a function
  prologue (register push). Result: 2,186 starts, 1,847 live, about
  **96 KB not reached**. Largest runs: 8,174 B at app+`0x538E0`, 5,752 at
  `0x2E91A`, 4,962 at `0x192E6`, 4,690 at `0x5B192`, 4,552 at `0x64CC4`.
- Accepting every pointer-like word (no prologue check) leaves 77 KB, which
  is a floor.
- Lesson: `app_core` and the other stock tasks (`btstack`, `audio_dec`,
  `ui`, …) reach most of the code. Replacing `my_app` alone frees little
  flash. Large flash for our code needs either cutting tasks we do not need
  (patching their launch) or space outside the app (`0xDA000–0xFFFFF`,
  which holds the retained VM settings and is not yet mapped).
- Still an estimate: unresolved indirect jumps and ROM callbacks remain.

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
