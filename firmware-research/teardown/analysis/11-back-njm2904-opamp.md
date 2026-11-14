# 11: back face, op-amp and surrounding small ICs (macro, shallow focus)

**Area:** back face, analogue section; exact position on the board not yet placed
(no wide shot of the back). Only a narrow band is in focus. Received 2026-11-13.

| Item | Reading | Confidence |
|------|---------|------------|
| 8-lead small chip marked `2904` and `JRC`, with a middle line that looks like `5BF` (seen upside down) | **JRC (New Japan Radio) NJM2904**: dual general-purpose op-amp. Typical use in a handheld radio: microphone amplifier, audio filtering, or tone/data shaping | `2904` and `JRC` read; middle line uncertain |
| Two small 5- or 6-lead parts below it | regulators, switches or transistors; no marking legible | not identified |
| Chip with many fine leads, left, out of focus, shows a logo | not legible | not identified |
| Large chip, right edge, out of focus | not legible | not identified |
| Many resistors and capacitors, some in silk boxes | filter and bias networks | read |

## Diagnostic notes

- A dual op-amp next to dense passive networks is the audio/baseband area. The AFSK
  audio for APRS has to pass between the small MCU and the RF chip somewhere here.
- This photo needs a sharper retake and a wide shot to place it on the board.
