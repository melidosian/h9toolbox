# TD-H9 teardown notes

Photos of the opened TD-H9 (TIDRADIO) and the image analysis for each one.
Everything here is **visual inspection only**: part markings read from a photo
are marked *(read)* when legible, *(inferred)* when deduced from package, layout
or known firmware behaviour, and *(unverified)* when a guess. Anything confirmed
by firmware or hardware tests links to the evidence note that confirms it.

## Layout

- `photos/NN-short-name.jpg` : original photos, numbered in the order received. **Not published yet**:
  kept locally and git-ignored. The "photo N" references below and in the analysis notes
  refer to these numbers.
- `analysis/NN-short-name.md` : per-photo analysis (area, board, components, purpose, diagnostic notes)
- this file : the index and the running board/component summary

## Index

| # | Area / board | Analysis |
|---|--------------|----------|
| 01 | Front face, upper half under the LCD (close-up, lit) | [analysis](analysis/01-front-upper-closeup-lit.md) |
| 02 | Same area, shows board revision silk | [analysis](analysis/02-front-upper-closeup-board-rev.md) |
| 03 | Whole front face, LCD removed; front case; LCD | [analysis](analysis/03-front-full-board-lcd-removed.md) |
| 04 | Front face with LCD in place | [analysis](analysis/04-front-board-lcd-in-place.md) |
| 05 | Overview: front case beside board | [analysis](analysis/05-overview-front-case-and-board.md) |
| 06 | Top end: antenna connector, ceramic patch antenna, volume shaft | [analysis](analysis/06-top-end-gps-patch-antenna.md) |
| 07 | Front: three-key column, backlight LEDs (macro) | [analysis](analysis/07-front-key-column-leds-closeup.md) |
| 08 | Front: speaker springs, antenna coax socket, trimmer (macro) | [analysis](analysis/08-front-speaker-ufl-area.md) |
| 09 | Back, bottom end, edge-on: first view of the ICs | [analysis](analysis/09-back-edge-view-ics.md) |
| 10 | Back: Puya PY32F003F18P6 and 5-hole header | [analysis](analysis/10-back-py32f003-and-header.md) |
| 11 | Back: JRC 2904 op-amp, analogue section (soft focus) | [analysis](analysis/11-back-njm2904-opamp.md) |
| 12 | Back: GigaDevice 25Q128ESIG flash | [analysis](analysis/12-back-gd25q128-flash.md) |
| 13 | Back: 48-lead main chip, no visible marking | [analysis](analysis/13-back-lqfp48-main-soc.md) |
| 14 | Back: 7 edge test pads, 24.000 MHz crystal | [analysis](analysis/14-back-edge-test-pads-24mhz.md) |

## Running component summary

Status after photos 01–14. Front face well covered. Back face: only macro shots of
the bottom end so far; no wide shot, and the RF section has not been photographed.

### Chips identified by marking

| Part | Marking read | What it is | Role |
|------|--------------|-----------|------|
| Puya PY32F003F18P6 | `PUYA PY32F003 F18P6 5L1TA1B` (photo 10) | ARM Cortex-M0+ microcontroller, 20 leads | second processor; leading candidate for the APRS MCU (not confirmed) |
| GigaDevice GD25Q128E | `GigaDevice 25Q128ESIG ENK091` (photo 12) | 16 MiB serial NOR flash, 8 leads | the "resource chip" the firmware work reads and writes |
| JRC NJM2904 | `2904 JRC` (photo 11) | dual op-amp | audio/baseband (exact job unknown) |
| Crystal | `24.000` (photo 14) | 24 MHz | clock for the main chip |
| Transistor | `6C` (photo 01) | BC817-40 by marking code | likely LCD backlight switch |

### Seen but not identified

- 48-lead square main chip beside the flash and crystal: no visible marking. Almost
  certainly the JieLi BR23-family chip known from firmware work (photo 13).
- Part marked `717` (photo 01); 6-lead part near the speaker (photos 07, 08);
  long two-lead part marked `…EAUE` (photo 13); several small parts near the
  op-amp (photo 11).
- RF transceiver chip, power amplifier, GPS receiver: not photographed yet.

### Access points for diagnostics

- 5-hole header beside the PY32F003, reachable from both faces (photos 03, 10).
- 7 bare pads in two groups (3 + 4) at the board edge beside the main chip (photo 14).
- `BAT` test pad under the LCD (photo 01).
- GD25Q128E legs are exposed for a clip-on programmer (photo 12).
- U.FL socket for the patch antenna (photo 08).
- None of these has been probed.

### Board facts

- Silk: `H9PLUS-V1.2`, `25/12/24`, lot `393856A260509P18411423`.
- One main PCB in a cast metal chassis, plus a small carrier board for the ceramic
  patch antenna at the top (photo 06).
- Digital parts are grouped at the bottom end of the back face, behind the keypad.
- Front face: 19 dome keys, white backlight LEDs, microphone, trimmer, LCD connector,
  five `R200` power resistors and an air coil under the LCD.
- Speaker is in the front case and connects through two soldered coil springs.
- Main antenna connector is bolted to the chassis with two screws.

### Inferred or unverified

- PY32F003 is the APRS MCU; the 5-hole header is its debug port.
- The 7 edge pads carry USB and/or serial lines of the main chip.
- The 1 MiB main flash is inside the main chip's package.
- Patch antenna is for GPS.
- `R200` group and coil belong to the transmit supply/RF path.
- Gold bar under the LCD clamps or grounds a part on the back.
- Case moulding reads `H9WIFI`.

### Still needed

- A flat, wide shot of the whole back face, then the RF section and any shield cans.
- Sharper retake of the op-amp area (photo 11).
- Main chip top with light from the side, to look for a faint marking.
- GPS receiver chip.
- Speaker label, case moulding text, LCD flex markings.
