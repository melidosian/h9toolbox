# H9 Toolbox module loader (offline experiment, not yet flashed)

Run programs stored on the 16 MB resource chip by copying them into RAM.
This is H9 Toolbox Firmware (stock radio + the S2 resource service) with
Brick Breaker taken out of main flash; Brick Breaker is now the first
module. Supersedes the plan in [`../resource-loader/DESIGN.md`](../resource-loader/DESIGN.md).

**On the radio:** hold **Back** for about half a second (as Brick Breaker's
launcher). A menu lists the modules on the chip, then **RADIO** (restart into
the radio) and **USB UPDATE** (the BR23 USB loader, as holding 8 at
power-on). **Up/Down** move, **Menu** runs. When a module returns, the menu
comes back. If no valid module bundle is on the chip, holding Back does
nothing and the radio keeps running.

## Layout

| Where | What |
|---|---|
| app+`0xA77F0` (Brick Breaker's game cave, 1,632 B before the S2 tables) | [`src/stub.s`](src/stub.s), 608 B: the hold-Back hook, bundle checks, loader, run loop |
| app+`0x088906` | the my_app loop hook, now calling the stub (was Brick Breaker's launcher at app+`0xA9D9E`, which is erased again) |
| app+`0x05FA9E` | heap part-1 end `0x2BF00` → `0x29F00`: 8 KiB of RAM (`0x29F00–0x2BEFF`, 64 of 527 heap blocks) for modules |
| chip `0xCC0000` | "modules" upload slot: `H9DATA01` directory ([`dist/doom/upload.mjs`](../../dist/doom/upload.mjs) `SLOTS.modules`), bundle at `0xCC1000`, up to 124 KiB |

The game slot's resource area now ends at `0xCC0000` (2.6 MB; the Doom pack
is 1 MB). Music stays at `0xCE0000`.

## Bundle (`H9MODS01`)

```
+0   "H9MODS01"   +8 u16 version 1   +10 u16 count (1-255)   +12 u32 bundle bytes
+16 + 32*i:  name[16] (ASCII, NUL-padded)  u32 offset (from bundle start, 4-aligned)
             u32 size  u32 bss  u16 entry (offset from 0x29F00)  u16 CRC-16/XMODEM of the body
```

Module 0 is the menu. The stub accepts a bundle only if the directory
(magic, one segment at `0xCC1000`, CRC-16) is valid, its `H9DATA-COMPLETE`
marker is set (written only after the uploader reads everything back), the
bundle header matches and its length equals the upload. Before running a
module it checks size + bss ≤ 8 KiB, entry < size, offset + size inside the
slot, copies the body to `0x29F00` with the stock NOR read, zeroes bss and
checks the CRC in RAM.

## Module interface

```c
unsigned module_main(const struct h9_api *api, unsigned self);   /* modules/common/h9mod.h */
```

- Linked for `0x29F00`; text and read-only data only (no initialised globals
  yet; the build rejects `.data`/`.bss`). Stack is my_app's task stack.
- `api->read(dst, chip_address, length)` is the stock NOR read
  `0x01E4FF22`; `api->bundle` is the bundle's chip address.
- `modules/common/h9lib.s` gives C the LCD window/write, key scan, delay,
  LED and USB-loader helpers (copied from the hardware-proven c-hello path).
- The stock radio app (`my_app`) no longer runs once a module starts: the
  stub drops its frame, exactly as Brick Breaker did. app_core, Bluetooth and
  the other tasks keep running.
- The menu returns the module index to run, or 0 for RADIO. Brick Breaker
  never returns (power-cycle or choose nothing; same as before).

## Build

Docker (image `h9-jieli`, from [`../toolchain/Dockerfile`](../toolchain/Dockerfile)) and
the extracted JieLi toolchain are needed for the menu. Build H9 Toolbox
Firmware first; this build starts from its pinned app.

```sh
python3 -I ../resource-service/build.py --stage bb-s2 --stock-image <app.bin> --stock-package <TD-H9-V1.0.38.fw>
JIELI_TOOLCHAIN=<extracted toolchain> python3 -I build.py --stock-image <app.bin> --stock-package <TD-H9-V1.0.38.fw>
python3 -I test_build.py
```

On Windows, extract the toolchain outside OneDrive (it is large) and set
`$env:JIELI_TOOLCHAIN`; Docker cannot mount the session temp folder, so build
inside the repository.

The build checks: the base is the pinned H9 Toolbox app (`1334a32c…`) and
its hook calls Brick Breaker's launcher; Brick Breaker's caves are erased in
stock; `game.s` still assembles to the hardware-tested game
(`19ab1b31…` at the old cave) before it is relinked for RAM; the stub fits
before the S2 tables; modules have no data sections or undefined symbols;
and the package changes exactly H9 Toolbox's sectors plus the heap sector
`0x64000`.

| Output (build 1, 2026-11-10) | |
|---|---|
| Package SHA-256 | `efb32b8e1f8adaac63b485b52fd5fcf314c26164ea785ea6af7277537ecd97df` |
| Main flash SHA-256 | `79858310c73879a360e3f8c0cfb003a0043d74593a05421ddeb6f81b728bbcc1` |
| Changed sectors (from stock) | `0x5000`, `0x62000`, `0x64000`, `0x8D000`, `0xAA000`, `0xAC000`, `0xAE000`, `0xAF000`, `0xD9000` |
| Stub | 608 B, SHA-256 `967bd18b…` |
| Bundle `h9_modules.bin` | 3,468 B, SHA-256 `feedc6b1…`: MENU 1,760 B, BRICK BREAKER 1,628 B |

## Hardware trial (not done; needs the user's go)

1. **Install** the `.fw` like H9 Toolbox Firmware (hold-8 `h9_flasher.py flash`
   with a new backup, or PTT+2 HID). Boot: channel screen, receive audio,
   menus, short Back taps work. Holding Back with no bundle: nothing happens.
2. **Heap check** (the risk: Bluetooth needs ~30 KB of heap; idle use left
   ~30 KiB free before the 8 KiB carve, and peak use was never measured):
   connect the web programmer and read the radio twice, run the resource
   reader, leave it receiving for a while. A reboot loop or a reset during
   Bluetooth use means the slot must shrink (4 KiB) or move to the allocator.
3. **Upload** `h9_modules.bin` with the data-upload prototype
   (`prototypes/data-upload`, slot *Modules*) over Bluetooth.
4. **Menu:** hold Back. Expect the menu with BRICK BREAKER, RADIO, USB UPDATE.
5. **Run** Brick Breaker; play; power-cycle.
6. **RADIO:** expect a restart into the radio (ROM `0x0200000A`, the stock
   allocation-failure reset; not yet tried as a deliberate exit).
7. **USB UPDATE:** expect the BR23 USB loader (`4C4A:2342`).

Recovery: the same as H9 Toolbox Firmware (hold 8, or PTT+2 HID restore).
The modules slot is removed with the uploader's restore (slot *Modules*).

## Open questions

- Is 8 KiB safe beside Bluetooth (step 2)?
- Is ROM `0x0200000A` a clean restart when called on purpose?
- Initialised data for modules (copy `.data` in the stub or in a module
  crt0) and a way for a module to keep state across runs.
- The site installer (`dist/firmware-update/`) and an upload page for
  modules, once the trial passes.
