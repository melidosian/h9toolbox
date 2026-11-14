# 02: front side, upper half, LCD lifted (darker, shows board revision)

**Area:** same region as photo 01, framed slightly lower and with less glare.
Received 2026-11-13. Visual inspection only.

## What this photo adds to 01

| Item | Reading | Confidence |
|------|---------|------------|
| Silk `H9PLUS-V1.2` | board name and revision. The PCB is called **H9PLUS**, revision 1.2 | read |
| Silk `25/12/24` under it | board date. Read as 2025-12-24 (year first) because the lot code above contains `260509`, which looks like 2026-05-09 | read; date order inferred |
| Silk `393856A260509P18411423` | same lot code as photo 01, confirmed | read |
| Second gold coil spring is not in frame, but the silk circle is: it outlines the speaker | inferred |
| Two large round dome keys at lower left | top of the three-key column (see photo 03) | read |
| Small SOT part and passives left of the speaker circle | not identified | |
| Light grey filled circle, lower right | printed blank spot, no part (label or QC mark area) | unverified |
| U.FL plug seated in its socket, lower right; a small white part just below it | antenna feed socket; the part below may be a matching component | inferred |
| Top-edge switch seen from a better angle: side-actuated tactile switch with a white plunger | confirms photo 01 | read |

Everything else (FPC connector, `BAT` pad, five `R200`, air coil, `332`, `717`,
gold pad, gold bar) matches photo 01. The transistor marked `6C` in photo 01 is
not legible here.

## Diagnostic notes

- The board silk says H9PLUS, not H9. Firmware and hardware notes should record the
  board as **H9PLUS-V1.2** in case other revisions differ.
