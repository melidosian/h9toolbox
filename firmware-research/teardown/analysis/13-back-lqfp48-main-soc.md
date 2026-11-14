# 13: back face, 48-lead main chip (macro)

**Area:** back face, bottom end, just below the GigaDevice flash.
Received 2026-11-13. Visual only.

| Item | Reading | Confidence |
|------|---------|------------|
| Square chip, 12 leads on each visible side (48 total), pin-1 dot at lower left | **main processor**. No marking can be seen on its top in this photo: the surface looks blank or the print is too faint at this angle | lead count read; identity inferred |
| GigaDevice flash directly above | photo 12 | read |
| Metal-lid crystal at right, marked with a logo and a number starting `24.0` | 24.000 MHz crystal (clear in photo 14) | read |
| Long black two-lead part at the bottom with white letters, partly `…EAUE` | not identified (marking incomplete) | not identified |
| Fine-pitch chip at bottom left, mostly out of frame | not identified | |

## Link to firmware findings

- Firmware work identified the main MCU as a JieLi BR23-family chip (AC695x) from
  its USB loader name `BR23UBOOT1.00`. A 48-lead quad package beside a 24 MHz crystal
  and the 16 MiB flash matches that family's usual layout. So this is almost
  certainly the JieLi chip, but the photo does not show a part number.
- A retake with light raking across the top from the side may reveal a faint mark.
