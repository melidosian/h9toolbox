# 08: front face, speaker contact area and antenna coax socket (macro)

**Area:** front face around the speaker circle, wider than photo 07.
Received 2026-11-13. Visual only.

| Item | Reading | Confidence |
|------|---------|------------|
| Two gold coil springs soldered to round pads | speaker contacts, confirmed as springs soldered directly to the board | read |
| Gold U.FL plug seated on a socket, coax leaving to the right; small parts around it | patch-antenna feed. The parts at the socket are its matching/coupling components; no amplifier chip is visible on this face at the socket | read; role inferred |
| White-bodied part just right of the U.FL socket | not identified (possibly a filter or coupling part) | unverified |
| Trimmer with cross slot, white body | adjustable part, left of the speaker circle | read |
| Black 6-lead SOT part, upper left, with passives | same part as in photo 07; marking not legible | not identified |
| Empty two-pad footprint in a silk box above the speaker circle | unpopulated | read |
| Silk `H9PLUS-V1…`, `25/12/24`, lot code beginning `393856A260509P1841…` | matches photos 02 and 04 | read |
| Solder bump at the board edge | same as photo 07 | |
| Gold bar with screw | see photo 01 | |

## Diagnostic notes

- Traces from the three-key column run down toward the 6-lead SOT area, not straight
  to a connector, consistent with keys being read through a resistor network
  (firmware: two ADC ladders). Not traced in detail.
