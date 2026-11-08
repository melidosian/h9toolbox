# 16 MiB resource flash: partitions, driver, and Bluetooth clone path

Source: stock TD-H9 v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`, base
`0x01E00100`. Linear Pi32v2 decode. Static evidence; nothing here has been
run on hardware. The partition table and SPI NOR command set agree with the
earlier `td-h9-open-firmware` storage notes.

## Partition table

Seven 20-byte records at `0x01EB53F0` (name, origin, length, flags,
selector). They tile the whole 16 MiB external SPI NOR (a 25Q128 on the board
photo):

| Sel | Name | Origin | Length | Stock writer |
|---:|---|---:|---:|---|
| 0 | filesystem | `0x000000` | 1 MiB | allowed |
| 1 | voice | `0x100000` | 3 MiB | refused |
| 2 | font | `0x400000` | 4 MiB | refused |
| 3 | sys_data | `0x800000` | 1 MiB | allowed (SMS, Bluetooth pairing, APRS history) |
| 4 | rw_freq | `0x900000` | 1 MiB | allowed (the Bluetooth clone image) |
| 5 | resource | `0xA00000` | 3 MiB | refused (startup logo, bank 5) |
| 6 | record | `0xD00000` | 3 MiB | allowed |

`0x01E4FEF4(sel)` returns `-3` when bit `sel` of `38` (`0b100110`) is set, so
the stock writer refuses voice, font, and resource: stock never writes them.

## Routines

| Address | Signature | Behavior |
|---|---|---|
| `0x01E52CB2` | `(sel, offset, buf, len)` | Partition read. Requires halfword RAM `0x12234 == 0x5AA5`, checks bounds with `0x01E4FECC`, adds the origin, calls `0x01E4FF22` |
| `0x01E50128` | `(sel, offset, buf, len)` | Partition write. Same gate and bounds, then the protect mask. Per 4 KiB sector under the ROM lock: read into the static buffer `0x16E44`; program directly if the target bytes are `0xFF`, otherwise erase, merge, and program the whole sector |
| `0x01E4FF22` | `(buf, chip_addr, len)` | Read: ROM lock `0x02001634`; chip select GPIO 23 low; `03` + 24-bit address over SPI2; DMA `0x01E4C35A` for length ≥ 16 and aligned buffer, else bytes; select high; unlock `0x02001650` |
| `0x01E50000` | `(sector_index)` | 4 KiB erase: lock, write enable, wait, `20` + address, wait, unlock |
| `0x01E5006E` | `(src, chip_addr, len)` | Program in 256-byte pages: write enable, `02` + address + data, wait |
| `0x01E4FF90` | `()` | Write enable (`06`) |
| `0x01E4FFBE` | `()` | Busy wait: status `05` until bit 0 clears, at most 500,000 polls with ROM delay `0x020009BE(10)` |
| `0x01E4FF06` | `(byte) -> byte` | SPI2 byte exchange (buffer `0x1E1E08`, then `0x01E4BD0E(2, 1)`) |

`my_app` initializes the chip at app+`0x08849C..0x088518`: GPIO 23 input
enable, output, high; SPI2 setup `0x01E4C2DA(2)` (app+`0x0884BC`); then,
unless halfword `0x12234` already equals `0x5AA5` (check at app+`0x0884C8`),
a JEDEC ID `9F` read (app+`0x0884F2`, manufacturer `EF` and `C8` branches).
Where the `0x5AA5` marker is written has not been located. A task that
replaces `my_app` (Snake) skips all of this.

## Bluetooth clone protocol on the radio

GATT write callback `0x01E5DCA8` switches on `handle − 13`. Handle `0x14`
clears 80 bytes at RAM `0x127B0`, copies the write there, and posts event 17
with its length; it is presumably the programming characteristic (`FF02`),
which has not been checked against a handle capture. Handle `0x17` is the
APRS AX.25 path documented in the `td-h9-open-firmware` notes. The clone parser starts at `0x01E5D018` (state machine via
`tbb`):

- **Handshake:** 7 bytes with `P` at 0, `J` at 3, `0x14` at 6 → ACK; `02`
  → 8-byte identity via session callback `+8`; `06` → screen wake and ACK;
  `TID` (length ≥ 7) → session callback `+24`; `E` (length 1) → ACK and
  exit.
- **Read** `R a_hi a_lo 0x20` (length 4): replies `W a_hi a_lo 0x20`, then
  each byte via `0x01E52CB2(4, addr + i, &b, 1)` at app+`0x05D018` (so the
  clone image is `rw_freq`), then a byte-sum checksum.
- **Write** `W a_hi a_lo 0x20 data[32] sum` (length 37): addresses at or
  above `0x5000` need session flag bit 2 (`b[ctx+32] & 4`); lower addresses
  do not. Calls the session's write callback `[ctx+12](addr, data, 32,
  flags)` at app+`0x05D11C..0x05D125`, then ACKs `06` whatever the result.
- Both paths call a progress routine (`0x01E5CFD8`) whose percentage is
  clamped to 0–100.

## SPI2 bring-up (for running without the app)

| Item | Value | Source |
|---|---|---|
| Registers | `0x1E1E00`: `+0` control, `+4` baud divider, `+8` buffer | table `0x01EB2144` (SPI0 `0x1E1C00`, SPI1 `0x1E1D00`) |
| Port map | A: clear bit 16 of `0x1E511C` (B sets it) | callback `0x01E8C23A` |
| Pins, port A | CLK 24, DO 26, DI 25 | table `0x01EB4F21` (port B: 13, 62, 61) |
| Control | `|=8`, `&=~0xC00` (mode 0), `&=~4`, `&=~2`, `|=0x10`, `|=0x20`, `|=0x80`, `&=~0x40`, `|=0x4000`, baud `clk/20 MHz − 1`, buffer `=0`, `|=1` | `0x01E4C1AE`, `0x01E4C2DA` |
| Exchange | write buffer; wait for control bit 15; set bit 14; read buffer | `0x01E4BD0E` |

Stock configures GPIO directions only for SPI2 port B; for port A it relies
on the peripheral. All flash pins (23–26) are in GPIO bank 1
(`0x1E5040`: `+0` out, `+8` direction, `+0xC` input enable).

## RAM padding used by the service images

`.bss` is cleared up to `0x1E4D4`; `0x1E4D4..0x1E4D7` is a separate variable
(eight table entries at app+`0xB0968..` point at it); the heap's first block
is `(0x1E4D8 + 127) >> 7 = 0x3CA`, i.e. `0x1E500`. No instruction immediate
or aligned literal refers to `0x1E4D8..0x1E4FF`. It is not cleared at boot.
