# 09: back face seen edge-on, board lifted from the chassis

**Area:** first view of the **back (component) face**. The board is lifted away from
the cast chassis (left, with part of the rear label and barcode). The end in view is
the **bottom end of the radio**: the row of five plated holes here is the same
5-hole header seen from the front at the lower-left keypad edge.
Received 2026-11-13. Visual only.

| Item (top to bottom of frame) | Reading | Confidence |
|------|---------|------------|
| Row of 5 plated holes at the board corner | the 5-hole header, directly beside the first chip | read |
| 20-lead chip with a logo and three text lines | Puya PY32F003 (read clearly in photo 10) | read |
| Board edge with exposed plated copper and a notch | grounded edge; notch clears a chassis feature | read |
| 8-lead wide chip | the GigaDevice flash (photo 12) | read with photo 12 |
| Two boxed groups of bare pads at the edge | test/programming pads (photo 14) | read |
| Square chip with leads on four sides | 48-lead main chip (photo 13) | read with photo 13 |
| Metal-lid crystal | 24.000 MHz crystal (photo 14) | read with photo 14 |
| Two gold spring pins sticking out past the board edge | spring contacts at the bottom of the radio | read; purpose unverified |
| Chassis label with barcode, `Radio`, CE mark | rear product label | partly read |

## Diagnostic notes

- All the digital parts (small MCU, flash, main chip, crystal) sit together at the
  bottom end of the back face, behind the keypad.
- The board can be lifted off the chassis with these parts exposed and no shield can
  over them in this area.
