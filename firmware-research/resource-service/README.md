# TD-H9 resource-flash service (S1 read, S2 write)

Stock-derived v1.0.38 main images that let a computer read, and in S2 write,
the radio's 16 MiB SPI resource flash over the ordinary Bluetooth
programming protocol. Both were flashed and tested on one TD-H9 (2026-11-08):
S1 read the whole `resource` partition twice with matching hashes, and S2
wrote a custom boot logo. The service ships to users as part of
**H9 Toolbox Firmware** (stage `bb-s2`, below).

**To install H9 Toolbox Firmware, use the browser installer at
https://h9toolbox.com/firmware-update/** (Chrome or Edge, USB-C, PTT+2 HID
mode); change the boot logo at https://h9toolbox.com/boot-logo/ or in its
step 04. The commands below are for building and research.

- **S1** is read-only: nothing in it erases or writes that chip.
- **S2** contains the unchanged S1 code plus a guarded sector erase/program
  service. Do not install S2 until S1 has produced two matching dumps of every
  region you might write: those dumps are the only backup of this chip.

Stock startup, `my_app`, radio functions, the UI, and the normal Bluetooth
programmer are unchanged. S1 redirects one 4-byte call in the stock
clone-read handler to authored Pi32v2 code in an erased area of the app
(1,008 bytes with the bulk read). S2 also redirects the clone-write callback
to an 834-byte write module in a second erased area. Both areas are white
margins of the User Manual QR codes; the codes still scan.

## How it works

The stock BLE `R addr16 0x20` handler replies `W addr16 0x20` plus 32 bytes,
reading each byte with `0x01E52CB2(4, addr + i, &byte, 1)`. That function is
the bounds-checked reader for the external NOR: it requires the stock
flash-ready marker (halfword RAM `0x12234 == 0x5AA5`; `my_app` checks it
before its chip setup, and where it is written has not been located), adds the origin of partition `selector`, and calls the
low-level read `0x01E4FF22(buf, chip_address, len)` (ROM lock, chip-select
GPIO 23, SPI 2 command `03`). Selector 4 is `rw_freq` at `0x900000`, so the
existing clone image already comes from this chip.

The stock partition table at `0x01EB53F0` tiles the whole chip:

| Selector | Name | Origin | Length |
|---:|---|---:|---:|
| 0 | filesystem | `0x000000` | 1 MiB |
| 1 | voice | `0x100000` | 3 MiB |
| 2 | font | `0x400000` | 4 MiB |
| 3 | sys_data | `0x800000` | 1 MiB |
| 4 | rw_freq | `0x900000` | 1 MiB |
| 5 | resource | `0xA00000` | 3 MiB |
| 6 | record | `0xD00000` | 3 MiB |

The [service](src/resource_service.s) maps clone addresses as follows:

| `R` address | Behavior |
|---|---|
| `0x0000–0x3BFF` | Unchanged stock call (the web programmer reads `0x0000–0x313F`) |
| `0x3C00 + 32·k` | **Bulk read**: k 0–4 selects the maximum packet size, k 8–12 the same with bounded waiting for TX room; streams up to 2 KiB from the cursor; the reply reports what was sent |
| `0x4000 + 32·n` | Selects 32 KiB bank `n` (0–511) and returns a status block |
| `0x8000–0xFFFF` | Chip byte at `n·0x8000 + (addr − 0x8000)`, read through the stock partition reader using the partition that covers it |

Status block bytes: `0–3` `H9RS`; `4` version 1; `5–7` live JEDEC ID
(manufacturer, type, capacity), read with the stock `9F` sequence; `8` flash
ready (1/0); `9–10` selected bank, little-endian; `11` `0x5A`; the rest zero.
JEDEC bytes are zero if the flash is not ready. Data reads before a bank is
selected return `0xFF`.

The bank word and a tag live at RAM `0x1E4E0..0x1E4E7`, in linker padding
between the last stock `.bss` variable (`0x1E4D4`) and the first heap block
(`0x1E500`, from `(0x1E4D8 + 127) >> 7`). No stock instruction immediate or
aligned literal references that range; the builder rechecks the literals.
That RAM is not cleared at boot, so data reads require the tag.

The stock clone-read progress display clamps its percentage to 0–100, so the
high addresses do not draw out of range.

## Bulk read

The stock clone reply is limited to the 80-byte buffer at `0x127B0`, and
requests cannot overlap because writes land in the same buffer. The bulk
slot therefore sends extra notifications itself. It runs inside the GATT
write callback, where stock already submits its reply. For each packet it
reads 8-byte pieces through the stock partition reader, checks TX capacity
(`0x01E50286`, keeping 48 bytes for the stock reply), and submits with
`0x01E5D2E0(12, buf, len)`. The 12 is the programming notify handle that
stock uses at `0x01E5DD44`/`0x01E5DDD4`, which the builder pins.

- Packet: `B7 5A seq len` + `len` data bytes. `len` is the smaller of the
  slot's size (16/48/112/176/240) and the queue room minus 68, rounded down
  to a multiple of 8. That keeps 64 bytes free for the stock 37-byte reply
  plus the stack's per-notification overhead. A 40-byte reserve left the
  reply without room on hardware, and stock drops a reply that does not fit.
- Wait slots (k 8–12): when the queue lacks room for 8 data bytes, the radio
  polls up to 100 times, with a 20,000-iteration delay between polls, before
  stopping.
- Reply bytes: `0–3` `H9BK`; `4–7` start (24-bit) and status; `8–11` count
  (bits 0–11), first TX capacity (12–21), polls that ended with room
  (22–31); `12–15` cursor (24-bit) with tag `B7`. Status: 0 ok, 2 flash,
  3 queue full, 4 bad slot, 5 no bank selected, 6 submit failed.
- Measured on macOS (2026-11-08): link payload 182 bytes, TX capacity 494;
  the queue does not drain inside the callback, so about 416 bytes per round
  trip.
- Slot 18 (`R 0x3E40`) requests a fixed 15 ms interval with latency 0. macOS
  accepted it: the round trip fell from about 123 ms to 63 ms, raising the
  bulk read to 6.4 KiB/s (2026-11-08).
- Slot 16 (`R 0x3E00`) requests a 15–30 ms connection interval with slave
  latency 0 through `ble_user_cmd_prepare(11, 2, handle, &params)`. macOS
  chose 30 ms from that range. Stock
  asks for 20–30 ms with latency 10 (table `0x01EA2A38`). Slot 17
  (`R 0x3E20`) restores stock's parameters by calling `0x01E50316`.
- The status-slot read (bank select) also sets the cursor to the bank start.
- RAM: `0x1E4D8–0x1E4DF` (result) and `0x1E4FC–0x1E4FF` (cursor), in the
  same unreferenced padding.

## S2 write service

S2 links [the S1 reader](src/resource_service.s) unchanged in the first cave
and [the write module](src/resource_write.s) in the second erased cave
(app+`0xA9D9E`, 1,004 bytes, the cave Brick Breaker used for its launcher),
through [`resource_service_s2.ld`](src/resource_service_s2.ld).
The `R` hook points to a wrapper that calls the S1 reader, then reports
version 2 and the write state in status bytes 12–19. The stock `W` callback
call at app+`0x05D11C` is
`r7 = [r4+12]; r1 = r5 + 4; r2 = 32; r0 = r6; call r7`. S2 keeps the first
three instructions and replaces `r0 = r6; call r7` (4 bytes) with one call.
The service sets `r0 = r6` itself and calls `r7` for every address outside
`0x4000–0x4FFF`, so ordinary channel writes are unchanged.

`W` addresses below `0x5000` skip the stock session write-permission bit, so
the service uses `0x4000–0x4FFF`. The stock handler always ACKs a `W`; read a
status slot to see the result.

| `W` address | Payload (32 bytes) | Action |
|---|---|---|
| `0x4000` | `H9WR`, op 0 | Disarm |
| `0x4000` | `H9WR`, op 1, sector address (24-bit LE), CRC-16 (LE) | Erase the 4 KiB sector only if its current CRC-16/XMODEM equals the supplied CRC, then verify the erased CRC `0xE03E` |
| `0x4000` | `H9WR`, op 2, start address (24-bit LE) | Arm programming from a 32-byte boundary to the end of that sector |
| `0x4020–0x4FFF` | 32 data bytes | Program at the write pointer only if those 32 bytes are erased, read back and compare, then advance |

Safeguards:

- Only voice, font, and resource (stock selectors 1, 2, 5) can be armed or
  erased. Stock marks exactly these read-only in its writer (`mask 38` at
  `0x01E4FEF4`) and never writes them. Filesystem, sys_data (messages,
  pairing, APRS history), rw_freq (channels and settings), and record are
  refused.
- Erasing requires proof that the host has read the exact current sector.
- Programming never overwrites unerased bytes and never crosses its sector.
- Every `W` payload checksum is checked. Any failure disarms and records the
  reason.
- Flash I/O uses the stock erase (`0x01E50000`, which waits for completion)
  and page-program (`0x01E5006E`) routines, plus the stock reader in 8-byte
  pieces. Every transfer stays on the stock CPU byte path, not DMA.

Status bytes 12–19 (version 2): `12` result, `13` state (2 = programming),
`14–16` write pointer, `17` completed operations (mod 256), `18–19` last
sector CRC. Results: 0 ok, 1 bad magic, 2 flash not ready, 3 partition
refused, 4 misaligned, 5 sector CRC mismatch, 6 erase verify failed, 7 not
armed, 8 bad `W` checksum, 9 out of range, 10 target not erased, 11 program
verify failed, 12 bad op, `0xFE` no command since boot. State lives at RAM
`0x1E4E8..0x1E4FB` in the same linker padding as the bank word and carries
its own tag.

The [resource service page](../../prototypes/resource-reader/README.md) runs
the S2 test: it confirms the chosen sector is erased, programs a test
pattern while checking status after each block, reads it back, erases it with
the pattern's CRC, and confirms `0xFF` again.

## Build

Python 3.11+ and the exact official inputs are required:

```sh
python3 build.py --stage s1 --stock-image /path/to/app.bin --stock-package /path/to/TD-H9-V1.0.38.fw
python3 build.py --stage s2 --stock-image /path/to/app.bin --stock-package /path/to/TD-H9-V1.0.38.fw
python3 ../brick-breaker/tools/h9-flasher/h9_flasher.py inspect build/h9_resource_service_s2_OFFLINE_EXPERIMENT.fw --stock-base /path/to/TD-H9-V1.0.38.fw
```

The builder uses the pinned assembler, repacker, and flasher in
`../brick-breaker/tools`. It rejects any change to the stock app hash, the
hooked call and its argument setup (`app+0x05D010`), the erased cave, the
first bytes of each stock routine the service calls, the flash-ready check,
the partition table, the stock write-protect mask, the erase and program
entry bytes, both hooked call sites, or the expected changed-sector set.

| Item | S1 | S2 |
|---|---|---|
| Package SHA-256 | `200ce8e6ccc610875b1c269458213336c63212d79445e3a95a8a20194ec8f4cc` | `29c500417b65bcc7a672919827bd53e19d7ab591ce5b0f95106d93b8f61ceaf7` |
| Main flash SHA-256 | `c442fdfb2c2e998f92aa00d4c7fa52e4ec25bf312763b96f06802ad933fe23db` | `e61d181b76582bfca06f75852d3cbba5c4020fe5f88c5c37b5c1f97cc884cd96` |
| Changed 4 KiB sectors (from stock) | `0x5000`, `0x62000`, `0xAC000`, `0xD9000` | those plus `0xAE000`, `0xAF000` |
| Code | 1,008 bytes at app+`0xA77F0`, SHA-256 `c7382849…9fd0` | the same S1 code, plus the 834-byte write module at app+`0xA9D9E` (second cave, 1,004 bytes), SHA-256 `27bd7444…a23d` |
| Entries | `R` → `0x01EA78F0` | `R` → `0x01EA9E9E`, `W` → `0x01EA9EF6` |

Offline checks passed for both stages:
- a second build reproduces each package hash, and S1 is unchanged by the S2 work
- the flasher verifies an app-only stock rebuild
- the assembler reproduces the original stock call bytes from their displacement, so each new call resolves to its service entry
- the patched images differ from stock only in the hook bytes and the cave
- the linked code was disassembled and reviewed
- the pinned HID restore plan validates against the stock flash image

## Hardware trial

1. **Flash** in BR23 USB Update mode (power on holding **8**) with
   `../h9-flasher/h9_flasher.py` and a **new** backup path. On macOS, run it
   with `sudo` and `--device 4C4A:2342` (libusb transport, see the flasher
   README); on Windows, run it from an Administrator PowerShell with
   `--device '\\.\PHYSICALDRIVEn'`. The flasher saves and hash-checks the backup and its
   manifest before erasing, writes, and reads back the whole main image:

   ```powershell
   py -3 h9_flasher.py flash 'C:\path\to\h9_resource_service_s1_OFFLINE_EXPERIMENT.fw' --stock-base 'C:\path\to\TD-H9-V1.0.38.fw' --device '\\.\PHYSICALDRIVE1' --backup 'C:\path\to\h9-before-resource-service.bin' --yes
   ```

   Confirm the disk number first.
2. **Normal boot:** channel screen, receive audio, and settings as before.
3. **Passthrough:** read the radio with the web programmer and compare the
   image with a read made before flashing. They should match.
4. **Service check:** `python3 prototypes/resource-reader/start.py` from the
   repository root, then Connect and Check service. Expect version 1, flash
   ready, and a JEDEC capacity byte of `0x18` (16 MiB).
5. **Mapping check:** read custom range `900000`/`3140` (the start of
   `rw_freq`). It must equal bytes `8..` of the web programmer's `.img`
   from step 3, which the stock handler reads from the same chip.
6. **First read:** custom range `a00000`/`a000` (start of `resource`,
   where the logo descriptor points), read twice. The two passes must match.
   Note the KiB/s figure to estimate larger reads.
7. **Backup reads:** each partition, or the whole chip, read twice with
   matching hashes. Keep every `.bin` with its manifest. `sys_data` and
   `rw_freq` contain personal data.

The dumps are the chip backup S2 and S3 restore from. Until two-pass dumps
exist, do not write to this chip by any means.

S2 trial, only after the S1 dumps above:

1. Flash the S2 package the same way (new backup path). Repeat steps 2–4;
   Check service should report version 2 and *no command since boot*.
2. Read a writable region again and choose a sector that is erased in both
   the earlier dumps and this read. The page suggests one.
3. Run the sector test. It must end with *S2 test passed* and the sector
   back at `0xFF`.
4. Re-read the whole region and compare it with the S1 dump. It must match
   byte for byte.

## Recovery

The image changes only main-flash application sectors; the boot area
`0x0000–0x4FFF` and the resource chip are untouched.

- USB Update mode: `h9_flasher.py restore` with the same `.fw`, stock base,
  device, and the backup created by `flash`.
- PTT+2 HID (solid green top LED), requires `hidapi==0.15.0`:

  ```sh
  python3 restore_hid.py inspect --backup /path/to/h9-before-resource-service.bin --package build/h9_resource_service_s1_OFFLINE_EXPERIMENT.fw
  python3 restore_hid.py restore --backup /path/to/h9-before-resource-service.bin --package build/h9_resource_service_s1_OFFLINE_EXPERIMENT.fw --yes
  ```

  It accepts only a backup identical to stock v1.0.38 main flash and either
  pinned service package, rewrites the four sectors with the app header
  last, and rescans all 213 accessible sectors by CRC.

Main-flash recovery does not restore the resource chip. If an S2 operation
stops part-way, the sector holds a partial pattern; erase it from the page
(the radio requires the current CRC first). For S3 content changes, the S1
dumps are the source to rewrite from.

## Limits

- Reading is 32 bytes per Bluetooth round trip, like the clone read. A full
  chip is 524,288 blocks; measure the rate in step 6 before planning it.
- Bank selection is global. Do not run two reader sessions at once.
- The service exists only while this image is installed. Restoring stock
  returns `R` above `0x3FFF` and `W` at `0x4000–0x4FFF` to stock behavior.
- S2 holds the ROM lock while a sector erases, as the stock writer does
  (up to the stock 500,000 status polls). The Bluetooth task waits during
  that time; the page allows 2.5 s per ACK.
- S2 has not been tested in any form that executes its code. Its logic was
  checked by disassembly review and the host tests of the page's frames.
- The partition roles come from the stock table; their content formats
  (fonts, logo descriptors, voice prompts) are not decoded yet.

## H9 Toolbox Firmware: merged Brick Breaker + S2 (`--stage bb-s2`)

One package with the unchanged, hardware-tested Brick Breaker plus the S2
read/write service, for rewriting the boot logo at any time. The builder
first runs Brick Breaker's own builder (its app and package hashes are
pinned), then assembles the service with `COMPACT` defined (no slot 16, no
in-radio wait; same reply format) into the space Brick Breaker leaves in the
User Manual QR-code margins (see [`resource_service_bb.ld`](src/resource_service_bb.ld)).
Only the two clone hooks and the three service regions may differ from
Brick Breaker. The QR codes still scan.

| Item | H9 Toolbox Firmware (Brick Breaker + S2) |
|---|---|
| Package SHA-256 | `b9d24cb43937621358053356382ab72608bfbab677468febe4db867242098dc5` |
| Main flash SHA-256 | `83f79fb3281c37ab22a4fd3f154a6c52a50d3cf0d8063b546eb47ce3c603bac6` |
| Changed sectors (from stock) | `0x5000`, `0x62000`, `0x8D000`, `0xAA000`, `0xAC000`, `0xAE000`, `0xAF000`, `0xD9000` |
| Entries | `R` → `0x01EA52A8`, `W` → `0x01EA5300` |

```
python3 -I build.py --stage bb-s2 --stock-image <stock app.bin> --stock-package <TD-H9-V1.0.38.fw>
```
