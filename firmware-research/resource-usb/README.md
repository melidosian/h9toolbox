# TD-H9 resource flash over USB (U0–U1)

The plan here is a USB route to the radio's 16 MiB SPI resource flash that
**never modifies the main firmware**. In USB Update mode (power on holding
**8**), the BR23 mask ROM accepts code into RAM and runs it. A small RAM
program can read the resource flash into RAM, and the computer fetches it
over USB. Everything lives in RAM and disappears at power-off. It works from a
Mac.

Status: U0 (offline analysis, RAM programs, client) is done. **U1 passed on
hardware on 2026-11-08 (macOS):** the mask ROM runs a RAM program and
returns to its USB command loop. U2 is next. The chronological
record is the [research log](../resource-flash-log.md); stock facts are in
[main-resource-flash.md](../h9-firmware-research/evidence/stock/main-resource-flash.md).

## U0 findings

**The vendor RAM loader cannot run our code.** The flasher's
`br23loader.bin` is stored encrypted with JieLi's CrcDecode keystream
(512-byte transfers). Decrypted at `0x12000`, its `0xFB` handler implements
ops 0–6 and `0x42`, but **not JUMP_TO_MEMORY (`FB08`)**. Its WRITE_MEMORY
(`FB06`) is a CRC-checked memcpy that resets the chip if the target lies in
its own window `0x12000–0x27FFF`. That window includes its flash-driver
function table at `0x14C00`, so redirecting the table is not possible either.
Its READ_MEMORY (`FD07`) reads any address. The loader can drive SPI0/1/2
(table at `0x14720`), but it picks the port from packed key/value settings
(`SPI`, `UTTX`, …), and the resource flash uses a GPIO chip select. Driving
that through the loader would be guesswork.

**The mask ROM can run our code.** Its UBOOT1.00 protocol has WRITE_MEMORY
(decrypts each transfer with the CrcDecode keystream), READ_MEMORY (on this
radio, encrypts what it returns with the same keystream; see the research
log), and JUMP_TO_MEMORY. Every CSW residue it reports equals the full
transfer length, so the transport counts the bytes actually carried. The flasher already uses
the first and last on this radio to start the loader at `0x12000`. U1 showed
that a jump **returns** to the ROM's command loop (entry `r0 = 0x11384`,
`r1` = entry address; the 16-bit argument is in neither). The ROM alone is enough: it is the immutable part this project
may reuse, so no vendor loader is needed.

**SPI2 bring-up, from the stock v1.0.38 app** (needed because the app does
not run in update mode):

| Item | Value | Stock source |
|---|---|---|
| SPI2 registers | `0x1E1E00`: `+0` control, `+4` baud divider, `+8` buffer | table `0x01EB2144` |
| Port | A: clear bit 16 of `0x1E511C` | callback `0x01E8C23A` |
| Port A pins | CLK GPIO 24, DO 26, DI 25 | pin table `0x01EB4F21` |
| Chip select | GPIO 23 (bank 1 bit 7, `0x1E5040`): input enable, output, high | app+`0x0884A8` |
| Control setup | `|=8`, `&=~0xC00`, `&=~4`, `&=~2`, `|=0x10`, `|=0x20`, `|=0x80`, `&=~0x40`, `|=0x4000`, baud, buffer `=0`, `|=1` | `0x01E4C1AE`, `0x01E4C2DA` |
| Byte exchange | write buffer, wait for control bit 15, set bit 14, read buffer | `0x01E4FF06`, `0x01E4BD0E` |

Stock does no explicit GPIO setup for SPI2 port A (only port B gets it), so
the peripheral is expected to drive its pins once mapped. It is not known
whether update mode leaves SPI2's clock enabled. The RAM program bounds every
wait and reports a timeout rather than hanging. U2 settles this.

## Files

- [`usb_msc.py`](usb_msc.py): USB mass-storage bulk-only transport over
  libusb. It provides the same `execute()` call as the vendored Windows and
  Linux SCSI ports, so the vendored `jltech.uboot` protocol code runs
  unchanged on a Mac. It only claims the VID:PID you name.
- [`rom.py`](rom.py): mask-ROM session. INQUIRY must report `BR23 UBOOT1.00`
  before any vendor command. Uploads are encrypted per 512-byte transfer. All
  RAM access is limited to `0x12000–0x14BFF`, the window the flasher already
  uses. It also detects whether the ROM decrypts uploads.
- [`resource_usb.py`](resource_usb.py): `list`, `plan`, `u1 --stage probe|read|full`, and `u2` (JEDEC ID).
- [`src/u1_marker.s`](src/u1_marker.s): 24-byte U1 program. It writes `H9U1`,
  `r0`, `r1`, and a run counter to RAM `0x13000` and returns. It touches no
  flash, GPIO, or peripheral.
- [`src/spi2_rom.s`](src/spi2_rom.s): 378-byte SPI2 program for U2/U3. It
  sends only the read commands JEDEC ID (`9F`) and read (`03`), with up to
  4 KiB into RAM `0x13400`. It is built and reviewed but not used by U1.
- [`test_resource_usb.py`](test_resource_usb.py): 23 offline tests, including U2 against a fake ROM that emulates the SPI2 program (pass, timeout, silent bus, wrong capacity, unstable ID, no return). The probe stage is shown to send only INQUIRY and the read stage only INQUIRY and READ_MEMORY. A fake
  ROM at the SCSI level runs the real `u1` command for a ROM that returns,
  one that never returns, one that stores uploads as sent, and a non-ROM
  device (which receives no vendor commands). They also check bulk-only
  framing, the RAM window, pinned program bytes, and that the cipher matches
  the flasher's loader file.

## Mac setup

Homebrew `libusb` and Python 3.11+:

```sh
brew install libusb
python3 -m venv ~/h9usb
~/h9usb/bin/pip install -r firmware-research/resource-usb/requirements.txt
cd firmware-research/resource-usb
~/h9usb/bin/python -m unittest test_resource_usb
~/h9usb/bin/python resource_usb.py plan
```

On Linux the same commands work. On Windows use `--scsi '\\.\PHYSICALDRIVEn'`
with the flasher's existing SCSI path.

## Running U1

1. Radio off, USB-C data cable connected to the Mac.
2. Power on holding **8**. If macOS offers to initialize an unreadable disk,
   choose **Ignore**. Never initialize or erase it.
3. `~/h9usb/bin/python resource_usb.py list` and note the radio's VID:PID.
4. Run the stages in order, each only after the previous one passed. Run
   them as root, because macOS only lets libusb detach its disk driver for
   root:

   ```sh
   sudo ~/h9usb/bin/python resource_usb.py u1 --stage probe --device VID:PID --report u1-probe.json
   sudo ~/h9usb/bin/python resource_usb.py u1 --stage read  --device VID:PID --report u1-read.json
   sudo ~/h9usb/bin/python resource_usb.py u1 --stage full  --device VID:PID --report u1-full.json
   ```

| Stage | Sends | Proves |
|---|---|---|
| `probe` | SCSI INQUIRY only | The Mac transport reaches the ROM (`BR23 UBOOT1.00`). The report records VID:PID, descriptor strings, endpoints, and whether the macOS driver had to be detached |
| `read` | INQUIRY, then READ_MEMORY of 16 bytes at `0x12000` and `0x13000`, each twice | The ROM answers READ_MEMORY, which this radio's ROM has never been sent. Two reads must match |
| `full` | Adds a RAM write/read-back check, a 24-byte upload, and two jumps | How READ_MEMORY returns data (uploads are always sent encrypted), and whether the jump returns to the command loop (marker counter 1, then 2) |

The tool reattaches the macOS driver when it exits.

| Result | Meaning | Next |
|---|---|---|
| `probe` fails to open | Not root, or another app holds the device | Rerun with `sudo`; close disk utilities |
| `probe` refuses the device | INQUIRY is not `BR23 UBOOT1.00` | The report shows what answered; it was sent nothing else |
| `read` fails | The ROM does not support READ_MEMORY | Record it. U1 cannot verify the marker; rethink the route before `full` |
| `full` passes | The ROM runs RAM programs and returns | U2: JEDEC ID through `spi2_rom` |
| `full`: jump gets no reply | The jump does not return, or the code crashed | A crash resets the radio into normal firmware. The `upload` line shows `program` and decoded `ram`; they must match before concluding the jump does not return |

Power-cycle the radio when finished. Nothing is written to either flash
chip. Keep the JSON reports: they are the evidence for the
[research log](../resource-flash-log.md).

## Running U2

With the radio in USB Update mode (power-cycle into it again if needed):

```sh
sudo ~/h9usb/bin/python resource_usb.py u2 --device 4C4A:2342 --report u2-jedec.json
```

The tool:

1. Checks INQUIRY and learns the read cipher.
2. Uploads [`spi2_rom`](src/spi2_rom.s) (378 bytes) to `0x12000` and verifies
   the decoded RAM.
3. Twice: writes the parameter block at `0x13040` (`H9U2`, op 1 for the
   JEDEC ID, baud divider 31), jumps, and reads the block back.

The program configures chip select GPIO 23 and SPI2 port A (GPIO 24–26) as
stock does and sends only `9F` and three dummy bytes. Nothing is erased or
written on either flash chip.

| Result | Meaning | Next |
|---|---|---|
| `pass`, JEDEC `xx xx 18` | SPI2 works in update mode and reaches a 16 MiB chip | U3: chip reads |
| `SPI2 timeout` | Control bit 15 never set: SPI2's clock or port mapping is likely off in update mode | Find the SPI2 clock gate in stock board init, or bit-bang the same pins |
| `nothing answered` (`000000`/`FFFFFF`) | SPI2 clocks, but no chip responds | Check chip select and pin mapping, and try `--divider 255` |
| Capacity byte not `18` | Something answered but is not a 16 MiB part | Record the ID; compare with the board photo |
| `did not answer` | The program crashed (the radio resets to normal firmware) | Power-cycle, record it, and review the program |

**First hardware run (2026-11-08):** status ok with ID `FFFFFF`. SPI2 is
clocked in update mode, but nothing drives MISO. Diagnose in this order, from
a fresh power-on into update mode:

```sh
sudo ~/h9usb/bin/python resource_usb.py u2 --op regs    --device 4C4A:2342 --report u2-regs.json
sudo ~/h9usb/bin/python resource_usb.py u2 --op bitbang --device 4C4A:2342 --report u2-bitbang.json
sudo ~/h9usb/bin/python resource_usb.py u2 --op bitbang-swap --device 4C4A:2342 --report u2-bitbang-swap.json
sudo ~/h9usb/bin/python resource_usb.py u2 --op pins    --device 4C4A:2342 --report u2-pins.json
```

| Op | What it does | Read on flash only |
|---|---|---|
| `regs` | Word reads of GPIO banks 0–1, `0x1E5100..0x1E511C`, SPI2 control/baud; shows out/in/dir/ie/pu/pd for pins 23–26 | yes, no SPI traffic |
| `bitbang` | `9F` by hand on CS 23, CLK 24, MOSI 26, MISO 25 with SPI2 off. MOSI is released after the command, so a wrong guess cannot fight the flash's output | yes |
| `bitbang-swap` | The same with MOSI 25 / MISO 26 (first `bitbang` read FFFFFF) | yes |
| `pins` | SDK-style pin setup, then stock SPI2 and `9F`. Run it only after `bitbang` reads a real ID, because it drives pin 26 throughout | yes |

## Next

- **U2:** built; see above.
- **U3:** read the whole chip twice in 4 KiB jumps, compare the passes, and
  check that `0x900000` matches the web programmer's clone image.
- **U4:** sector erase/program with the S2 safeguards.
