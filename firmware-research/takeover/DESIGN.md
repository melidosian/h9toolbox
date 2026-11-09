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

## Phase 1b: code only `my_app` needs (for the Doom firmware)

[`callgraph.py`](callgraph.py) builds a whole-image call graph instead of
using the incomplete call index:

- It linearly decodes every function extent and collects all direct `call`
  targets, iterating until no new targets appear: 3,507 functions, 8,227
  direct call edges.
- Roots: the reset entry and the 673 prologue pointers.
- "my_app only" = reachable with `my_app` minus reachable without it.

Results (stock v1.0.38):

| Set | Functions | Bytes |
|---|---:|---:|
| reachable with `my_app` | | 696,576 |
| **`my_app` only** | 185 | **69,136** |
| unreached | 105 | 114,828 (mostly constant data mis-attributed to the last function before it: 57 KB at `0xB5D70`, 26 KB at `0xAA394`; not usable as is) |

- **The largest `my_app`-only block is contiguous:**
  app+`0x79CC6–0x890E4`, about **62 KB** (the radio UI and loop, including
  `my_app` itself at `0x88180`).
- **Helpers stay outside it.** The stock helpers the C programs use are
  all outside the block: controller setup `0x4C1DA`, key decoder
  `0x514AC`, delay `0x515C6`, partition read `0x52BB2`, SPI read
  `0x4FE22`, LCD submit `0x4BB8E`/`0x4BC0E`. So is the pin helper
  `0x79BE4`, which is `my_app`-only but just before the block.
- **Outside references into the block:** 15 words point into it.
  - One is the task table's `my_app` entry (app+`0x5FFD0`), which the Doom
    firmware replaces.
  - The other 14 are coincidences: clock-frequency constants (32, 96, 100,
    120, 160, 240 MHz in a table at `0xD3B9C`; 32,000,000 = `0x01E84800`),
    odd values, halfword tables, and instruction bytes.
- **Constant data used only by the block:** 28 data addresses, about 19 KB
  attributed. 17 KB of that is one pointer table at `0xAA37C` whose real
  extent is unconfirmed, so it is not counted yet.

### Flash budget for the Doom firmware

| Region | Size | Status |
|---|---:|---|
| far region `0xEA000–0xFCFFF` | 76 KiB | proven on hardware (C runs there) |
| `my_app`-only block app+`0x79E32–0x7D64E`, `0x7D69E–0x890E4` | ~62 KB | **trap-tested on hardware 2026-11-09** (713 s, no trap) |
| **total** | **~138 KB** | |

- **Fits the core:** the DMC100 engine core is 112 KB on Pi32v2, plus our
  platform layer (~10 KB), which fits across the two regions.
- **Front end:** the menus, intermission, finale and saves (~40 KB) become
  overlays loaded from the resource chip into RAM, as DMC100 does, or move
  into further reclaimed space once that is proven.

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
