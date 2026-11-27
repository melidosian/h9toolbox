# Doom size check: doomgeneric compiled with JieLi clang

2026-11-09. Source: [ozkl/doomgeneric](https://github.com/ozkl/doomgeneric)
at `e257a09` (GPL-2.0; tarball SHA-256 `26f8f60d…`), kept outside this repo
in `~/Documents/ChatGPT/TID-H9/doomgeneric-src`.

All 72 files of its `SRC_DOOM` list, minus the X11 back end, compiled
**unmodified** with `clang -target pi32v2 -mcpu=r3 -Oz -ffunction-sections
-fdata-sections` and the toolchain's newlib headers. Every file compiled.
Sizes are summed over the objects, before linking: unused functions are not
yet removed, and libc is not counted.

| Section | Bytes | Biggest contributors |
|---|---:|---|
| code (`.text`) | 115,130 | g_game 7.2 K, p_enemy 5.6 K, wi_stuff 5.6 K, m_menu 5.5 K, am_map 4.5 K, sha1 4.5 K, p_map 4.2 K |
| constants (`.rodata`) | 100,755 | `tables.o` 66,820 (fine sine/tangent, tantoangle), f_finale 7.9 K |
| initialized data | 60,030 | `info.o` 40,236 (`states`/`mobjinfo`, never written without DeHackEd), sounds 6.3 K, m_config 4.7 K |
| zeroed RAM (`.bss`) | 243,693 | r_plane 134,152 (`visplanes[128]`, `openings[320*64]`), r_main 29 K, d_loop 20.6 K (netcode), r_bsp 12.6 K, r_things 11.2 K |
| zone heap | 6 MiB requested | `i_system.c` `DEFAULT_RAM 6` |

## Against what the radio has

| Resource | Doom as is | Available (measured) |
|---|---:|---:|
| Flash for code + constants + init data | ~276 KB | 72 KiB far region (0xEA000) + a few small caves |
| RAM | ~300 KB static + a multi-MB zone | ~31 KB free heap with the radio running; ~130 KB more if `my_app`'s RAM is reclaimed |

Nowhere close as is, by roughly 4× on flash and more on RAM.

## What a fitting port needs (the microcontroller Doom approach)

- **Static RAM:**
  - Render at the panel size (160×128, not 320×200): halves or better
    `visplanes`, `openings`, and the column and span buffers.
  - Fewer visplanes, drawsegs and vissprites.
  - Drop the netcode buffers (`d_loop`).
- **Zone heap:**
  - Read the WAD in place instead of caching lumps in RAM. Level geometry,
    textures, sprites and the colormap stream from the resource flash at
    about 2 MB/s (measured), with small caches only.
  - This is the big change, and how the known small-RAM ports work.
- **Flash:**
  - Make `states`/`mobjinfo` const.
  - Shrink or compute the trig tables: 16-bit entries, or quarter-wave
    symmetry, saves roughly 40–50 KB.
  - Drop sha1/statdump/endoom/joystick/cd and the config file code.
  - Link with section garbage collection.
  - Code alone (115 KB) still exceeds 72 KiB, so it also needs more main
    flash: either reclaim stock code (needs task-level cuts, since only
    ~96 KB is unreached and scattered) or run a separate Doom-only firmware
    image that drops the radio app's code wholesale.
