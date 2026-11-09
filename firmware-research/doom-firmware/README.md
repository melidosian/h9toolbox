# Doom firmware (route 1): plan and resource-chip rules

Doom runs as its **own firmware image**:

1. While the radio runs H9 Toolbox Firmware, the game data goes onto the
   16 MiB resource chip over Bluetooth (the S2 service).
2. Then the Doom firmware is flashed to main flash over USB (hold-8).
3. To get the radio back, H9 Toolbox Firmware (or stock) is flashed again.

The resource chip keeps its contents across these swaps. Why not alongside
the radio: [../takeover/DOOM-SIZE.md](../takeover/DOOM-SIZE.md).

## Rules for the resource chip (the user's requirements)

1. **Everything we write can be restored exactly.**
2. **Stock firmware still boots and works with our data present.**

### Where data may go

Only sectors that are erased today, in partitions stock never writes (its
writer refuses selectors 1, 2 and 5) and only reads through its own indexes:

| Area | Range | Size | Why stock ignores it |
|---|---|---:|---|
| `resource` tail | `0xA1F000–0xCFFFFF` | 2,948 KiB | the 22 stock images and the boot logo end at `0xA1EFFF`; stock reads images by fixed offsets |
| `font` tail | `0x717000–0x7FFFFF` | 932 KiB | after the last font section (`0x400000 + 0x1A4D20 + …`, last used sector `0x716000`) |
| `voice` tail | `0x3A7000–0x3FFFFF` | 356 KiB | after the JLFS `tone` container (size `0x2A6690`, ends `0x3A6690`) |

All three were confirmed as single contiguous erased runs in the full-chip
dump of 2026-11-09 (SHA-256 `b6d60e2e…`).

### Never touched

- `filesystem`: a live FAT12 volume.
- `sys_data`, `rw_freq`: settings and channels.
- `record`: stock writes recordings there.
- Every used sector of `voice`, `font` and `resource`, the boot logo
  included.

### How it stays restorable

- **Gold copy:** the private full-chip dump (`b6d60e2e…`) holds every byte
  of the chip as it was before any Doom data.
- **Each write is recorded:** before writing, the tool reads the target
  range, checks it is all `0xFF`, and records range, pre-write hash and
  written hash in a manifest. It refuses to write over anything that is not
  erased.
- **Restore = erase those exact ranges back to `0xFF`**, verified by
  readback, through the same CRC-guarded S2 erase path. Because only
  erased sectors are ever used, restoring needs no saved data. The gold
  dump is the fallback for anything unexpected.

### Proving stock still boots

After the first data write, and before relying on it, run stock-derived
firmware (H9 Toolbox) with the data present and check:

- normal boot and the boot logo
- the voice prompts (number keys, Bluetooth connect)
- menu text in every font size the UI uses
- the Bluetooth programmer's read

Repeat once on exact stock v1.0.38.

## Game data: the DMC100-Doom converter (measured 2026-11-09)

[ArturBieniek4/DMC100-Doom](https://github.com/ArturBieniek4/DMC100-Doom)
(GPL-2.0-or-later, commit `0cdc78e`, kept outside this repo) is the
clamp-meter port from the article. Its converter (`tools/conv`, Python)
turns Freedoom 0.13.0's `freedoom1.wad` into compact packs for a 160-column
strip renderer.

| Pack | Bytes | Notes |
|---|---:|---|
| E1M1 | 2,802,000 | one map with menus and the title |
| E1 (9 maps) | 3,890,048 | `DOOM.DAT` 3,031,040 + `DOOMX.DAT` 859,072; maps 97–431 KB each |

- **Fit:** E1 needs 3,799 KiB. `resource` (2,948 KiB) plus `font`
  (932 KiB) give 3,880 KiB, so the **whole first episode fits** in the
  stock-safe tails without touching `voice`.
- **Split point:** `DOOM.DAT` is 12 KiB larger than the `resource` tail,
  so our data layer maps pack offsets onto the two chip ranges.
- **Running it on macOS** needed only tool paths, no source changes:
  - `HOSTCC=clang`
  - Xcode's `llvm-dwarfdump`, `llvm-size`
  - an `llvm-nm` wrapper that strips Mach-O's leading `_`
  - its `oracle/fetch.sh` checkout of doomgeneric `dcb7a8d`
- Its tests pass: `test_maps` (E1M1 and all 9 E1 maps), `test_trig`,
  `test_determinism`.
- Upload estimate over Bluetooth at 1.5–2 KB/s (write speed not yet timed):
  about 25–30 minutes for E1M1, 32–42 minutes for E1.

### Engine size on the TD-H9's CPU (measured)

DMC100-Doom's "ARM size gate" set (81 files: engine without the WAD code,
data layer, strip renderer, trig decoder, object store, libc and platform
stubs, generated tables), compiled with JieLi clang (`-target pi32v2
-mcpu=r3 -Oz -ffunction-sections -fdata-sections -ffreestanding`), all
compile cleanly. Linked with JieLi `ld --gc-sections` (its `size.ld`,
addresses moved), with no undefined symbols:

| Section | Bytes |
|---|---:|
| code | 80,036 |
| constants | 31,842 |
| data | 240 |
| bss | 13,396 |

- The same objects before dead-code removal: Pi32v2 119,415 B
  code+constants against **126,403 B for Cortex-M4 Thumb-2** (Apple clang,
  same flags). Pi32v2 is about 5.5% smaller.
- Not counted: the front end (menus, intermission, finale, saves; overlays
  in DMC100), input and our platform layer.
- DMC100 ships 99 KB resident plus 42 KB of overlays, so a complete
  TD-H9 port is probably 140–170 KB of code and constants.
- RAM follows DMC100's 32 KB design (static data plus frame region plus
  level arena).

Earlier raw-WAD estimate, kept for comparison (`wad_tool.py`):

- Target: shareware `doom1.wad` (4,196,020 bytes) or the free Freedoom.
- The first firmware has no sound, so the sound and music lumps can be left
  out to fit `resource` alone. Otherwise the WAD is split over the three
  tails.
- A small directory record at the start of the `resource` tail lists the
  pieces.
- Not committed to this repo.

## Firmware architecture (open)

- **Stock base:** keep the stock app's startup and drivers, replace
  `my_app` with Doom, and cut unneeded tasks (audio, APRS, Bluetooth,
  radio UI) to free flash and RAM.
- **From scratch:** our own app image with ported drivers (clock, LCD, SPI2,
  keys, timer). It gets all ~850 KiB of app flash and nearly all RAM, but
  every driver must be ported and proven.

## First-pass firmware build (2026-11-09, offline)

`build.py` builds DMC100-Doom's engine (its size-gate set: engine, data
layer, strip renderer, object store, trig, libc stubs, generated tables)
with the TD-H9 platform layer in `port/` (GPL-2.0-or-later, derived from
DMC100's stubs):

| File | Does |
|---|---|
| `port/td_platform.c` | I_* hooks: 35 Hz time from the 100 Hz stock counter, keys to Doom keys (Up/Down move, Menu/Back turn, 3 = use, other keys fire), errors on screen; front end stubbed (no menus or intermission yet) |
| `port/td_pump.c` | the LCD pump: each 320x240 column-major strip downsampled to the 160x128 panel and written as 1-pixel-wide windows (no frame buffer) |
| `port/td_nor.c` | `nor_read` on the stock partition reader; `fat_mount`/`fat_open` present the uploaded pack (directory at `0xA1F000`, COMPLETE required, one segment) as DOOM.DAT |
| `port/td_main.c` | entry from the cave: .data/.bss setup, hold-3 USB recovery, status screen (heap use, pack size), `img_init`, `D_DoomMain` with `-warp 1 1` |

Layout (all proven separately on hardware):

- Startup: C-hello's `entry.s` in the app cave, 738 B, from the 4-byte
  hook at `my_app`'s entry.
- Code: by object, packed with `ld --gc-sections` into the far region
  (69,634 B used) and the trap-tested `my_app`-only block (R2A 39,186 B,
  R1 4,790 B).
- RAM: the heap end is lowered `0x2BF00` → `0x22000`. Core
  `0x22000–0x255C0`, frame region 15,208 B, level arena 11,736 B.

The first package is `74edf3c4…` (flash `702d8eb6…`), with far blob
`49bfa937…` (69,898 B). The flasher's inspect accepts it as a stock-derived
app-only build. `test/test_pack_find.c` checks the directory reader against
sectors made by the uploader's own code.

Not yet known (hardware questions for the first run):
- whether the stock tasks fit in the smaller heap
- whether the `my_app` task stack is big enough for the renderer
- speed
- the key codes besides 3
