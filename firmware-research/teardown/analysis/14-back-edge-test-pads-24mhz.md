# 14: back face, edge test pads and 24 MHz crystal (macro)

**Area:** back face, bottom end, the long edge beside the flash and main chip.
Received 2026-11-13. Visual only.

| Item | Reading | Confidence |
|------|---------|------------|
| Two silk-boxed groups of bare gold pads: a group of 3 ovals, then a group of 1 square + 3 ovals | **7 unpopulated test/programming pads** next to the main chip. The square pad marks pin 1 of the 4-pad group. Two of the ovals have a via in them | read; functions unverified |
| Crystal with metal lid marked `24.000` under a maker logo | 24.000 MHz crystal for the main chip | read |
| Slot cut into the board edge between two large bare rectangles | board notch with plated lands either side; clears or grips a chassis/shield tab | read; purpose unverified |
| Two silk rings, one around a plain hole and one around a bare spot | tooling hole and marker | read |
| 8-lead chip with thick legs, centre left | the GigaDevice flash seen from the other side | read with photo 12 |
| 20-lead chip, top left, out of focus | the PY32F003 | read with photo 10 |
| Leads of the 48-lead chip, bottom | main chip | read with photo 13 |
| Small black two-lead part below the pads | diode | inferred |

## Diagnostic notes

- A 4-pad group beside the main chip is the right size for either USB (power, two
  data lines, ground) or a serial port (power, transmit, receive, ground). The
  3-pad group could be a serial/debug set. Which is which needs a continuity meter:
  find ground first (to the chassis), then battery/3.3 V, then watch the rest with
  a logic analyser at power-up.
- Together with the 5-hole header at the PY32F003, the board has two separate
  factory access points: one per processor.
