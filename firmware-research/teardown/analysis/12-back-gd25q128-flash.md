# 12: back face, GigaDevice SPI flash (macro)

**Area:** back face, bottom end, between the PY32F003 and the 48-lead main chip.
Received 2026-11-13. Visual only.

| Item | Reading | Confidence |
|------|---------|------------|
| 8-lead wide chip, text `GigaDevice` / `25Q128ESIG` / `ENK091` / a fourth line that looks like `AJ2428` | **GigaDevice GD25Q128E**, 128 Mbit (16 MiB) serial NOR flash, 8-lead wide package. Last line is a lot/date code | first two lines read; `ENK091` read; fourth line uncertain |
| Three fine parallel traces leaving the chip toward the top right | part of its serial bus | read |
| Boxed column of bare gold pads at the right edge: one group with a square pad and three ovals visible | test/programming pads (see photo 14) | read |
| Top of the 48-lead chip, bottom | main chip (photo 13) | read |
| Out-of-focus chip with large white characters, far left | not legible | not identified |

## Link to firmware findings

- The handbook's "Resource chip: 16 MiB SPI NOR on SPI2" is now matched to a
  physical part: this GD25Q128E. The size agrees. The JEDEC ID that the firmware
  work read back can be compared with GigaDevice's ID for this part to close the
  loop (not done in this note).
- The main 1 MiB flash (ID `0x856014`) has no separate chip in view. It is likely
  inside the main chip's package. Unverified.
- Being a standard 8-lead part with exposed legs, this chip can be read or written
  directly with a clip-on programmer, with the radio unpowered, as a recovery route.
