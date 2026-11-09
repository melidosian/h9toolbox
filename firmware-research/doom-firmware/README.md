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

## Game data

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
