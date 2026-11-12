# Resource flash and UI boundary: research log

Chronological record of the resource-flash and replacement-UI work. Each
entry says what was found, how, and whether hardware was involved. Static
facts live in the evidence notes:
[UI/radio superloop](h9-firmware-research/evidence/stock/main-ui-radio-superloop.md)
and [resource flash](h9-firmware-research/evidence/stock/main-resource-flash.md).
Unless stated otherwise, the input is stock v1.0.38 `app.bin`
(`11a07bc0…f9cd3`), read-only.

## 2026-11-07 Planning: where a replacement UI can meet the radio

- Stock `my_app` is a superloop that interleaves key handling, drawing, TX
  setup, channel-to-RF copies, audio-source requests, and the LED and standby
  ticks. One callee (`0x01E819F0`) both draws and changes RF output bits.
  There is no function-level UI/radio boundary.
- Snake replaces the `my_app` entry, so the loop and the stock resource-flash
  setup never run. Brick Breaker stops the loop once the game starts. Neither
  shows custom code running beside a working radio.
- Free erased space in the app totals about 3.5 KiB in three gaps, plus a
  1 KiB zero run; the package format keeps the app size fixed. This limits a
  larger UI.
- `pi32-call-index-v2.json` lacks the `my_app` loop call sites, so caller
  searches through it are incomplete.
- Recommended first UI milestone: a read-only overlay at the proven Brick
  Breaker hook that returns to the stock loop on every pass (M1).

## 2026-11-07 S0: the resource flash is reachable through the app

- The seven-partition table at `0x01EB53F0` tiles the 16 MiB chip. The
  Bluetooth clone image is partition `rw_freq` at `0x900000`: the clone `R`
  handler reads each byte with `0x01E52CB2(4, addr + i, …)`.
- The stock writer refuses voice, font, and resource (mask `38`). These are
  the partitions stock never writes.
- Read, erase, program, write-enable, busy-wait, and JEDEC routines were
  located (chip select GPIO 23, SPI2). The stock `res_nor`, `fat_nor`, and
  `sdfile` drivers and a `usb_msd` task exist; the USB-disk lead was not
  pursued.
- 44 bytes of RAM padding between `.bss` and the heap (`0x1E4D8..0x1E4FF`)
  have no code references.

## 2026-11-07 S1: read-only service image (built, not flashed)

- One 4-byte call in the clone `R` loop (app+`0x05D018`) is redirected to a
  402-byte shim. Addresses below `0x4000` behave as stock; `0x4000+32n`
  selects a 32 KiB bank and returns status (`H9RS`, live JEDEC ID, ready
  flag); `0x8000–0xFFFF` return chip bytes through the stock reader.
- Package `aeb9ce21…`, 4 changed sectors. The build is reproducible, the
  flasher verifies it as an app-only stock rebuild, the call encoding was
  checked against stock bytes, and the pinned HID restore plan validates.
- Correction during the work: short (2-byte) Pi32 calls resolve to
  `site + 2 + displacement`, not `+4`. An early annotation script used `+4`
  for every call. The S1 shim only uses targets taken from 4-byte calls, so
  it is unaffected.
- `test/firmware-update.test.mjs` "browser readback reconstructs…" already
  fails on `main` at `24ec51a` (`verified-known-image-seed` vs
  `verified-official-image-seed`). It is unrelated to this work.

## 2026-11-08 S2: guarded write service (built, not flashed)

- The clone `W` callback call at app+`0x05D11C` has room for a 4-byte call
  if the service performs `r0 = r6` itself. `W` addresses below `0x5000`
  skip the session write-permission bit, so the service uses
  `0x4000–0x4FFF`.
- Erase needs the CRC-16/XMODEM of the sector's current contents (erased
  sector CRC `0xE03E`). Programming only touches erased bytes and is read
  back. Only voice, font, and resource are allowed. Flash I/O goes through
  the stock erase/program routines in 8-byte pieces, so it stays off DMA.
- Package `d44e907c…`; the S1 code is byte-identical inside it.

## 2026-11-08 Bluetooth throughput

- No measured rate exists. At 32 bytes per round trip, an estimated
  30–100 ms per block puts a two-pass whole-chip dump at about 9–30 hours.
  Targeted per-sector backups (seconds to minutes) are practical. The S1
  trial step that reads the logo area measures the real rate.

## 2026-11-08 U0: USB route through the mask ROM

- The flasher's RAM helper `br23loader.bin` is stored encrypted with the
  JieLi CrcDecode keystream, restarting every 512 bytes. Decrypted, it runs
  at `0x12000`.
- The helper implements `FB` ops 0–6 and `0x42`, **not** `FB08`
  (JUMP_TO_MEMORY). Its WRITE_MEMORY is a CRC-checked memcpy that resets the
  chip for targets in `0x12000–0x27FFF`. That window includes its
  flash-driver function table at `0x14C00`, so that table cannot be
  redirected. Its READ_MEMORY reads any address.
- The helper can drive SPI0/1/2 (base table at `0x14720`) and chooses the port
  from packed key/value settings (`SPI`, `UTTX`, `UTBD`, `RESET`, `USBDP`,
  …; build date `Sep 14 2020`). Using it for the GPIO-selected resource flash
  is not established.
- Therefore the route uses the mask ROM's own WRITE_MEMORY, READ_MEMORY, and
  JUMP_TO_MEMORY. Open question: does the ROM's jump return to its command
  loop?
- SPI2 bring-up for update mode is ported from stock: base `0x1E1E00`, port A
  via `0x1E511C` bit 16, pins CLK 24 / DO 26 / DI 25, chip select 23. Stock
  sets GPIO directions only for port B. Whether SPI2's clock is enabled in
  update mode is unknown.
- macOS offers no SCSI pass-through to USB disks. Bulk-only transport over
  libusb (Homebrew 1.0.30, pyusb 1.3.1) works with the vendored protocol
  code; detaching the macOS disk driver needs root.
- Correction: the S0/S1 notes said `my_app` sets the `0x5AA5` ready marker
  after its JEDEC read. Only a **check** was observed (app+`0x0884C8`, which
  skips chip setup when the marker is already set); where it is written is
  not yet located. The notes were corrected.

## 2026-11-08 U1: staged ROM test (ready, not run)

- `resource_usb.py u1 --stage probe|read|full`. Probe sends only INQUIRY and
  records the USB descriptors; read adds two READ_MEMORY of RAM
  `0x12000`/`0x13000`; full checks the cipher, uploads a 24-byte marker
  program, and jumps twice. All of it is RAM-only.
- READ_MEMORY has never been sent to this radio's ROM; the flasher only used
  WRITE_MEMORY and JUMP. The read stage checks it in isolation.
- 14 offline tests pass, including a fake ROM that never returns from the
  jump and a non-ROM device that receives no vendor commands.

## 2026-11-08 U1 hardware, macOS (one TD-H9)

Radio in USB Update mode (power on holding 8), USB-C data cable to an
Apple-silicon Mac. macOS asked to allow the new USB accessory; the user
allowed it. macOS mounted no volume (`diskutil` listed none).

- **Device identity:** `resource_usb.py list` (descriptors only) found
  `4C4A:2342` with manufacturer and product strings `BR23UBOOT1.00`. The
  PTT+2 HID recovery mode uses the same VID with PID `4155`.
- **Driver:** without root, libusb's detach of the macOS mass-storage driver
  failed with "Access denied (insufficient permissions)" before any command
  was sent. The tool then saved no report on an open failure; that was fixed
  so every attempt leaves one.
- **Probe stage (INQUIRY only), run with sudo: pass.** USB 1.10 (full
  speed), interface 0, mass-storage subclass 6 (SCSI transparent), bulk
  endpoints IN `0x81` and OUT `0x01`, 64-byte maximum packet. The macOS driver
  was detached and reattached on exit. INQUIRY: vendor `BR23`, product
  `UBOOT1.00`, revision `1.00`.
- At USB 1.1 full speed, bulk-only transport tops out near 1 MB/s in
  practice, so a 16 MiB read is a few minutes at best. The SPI read rate in
  the RAM program and the per-command overhead will set the real figure.

- **Read stage (INQUIRY plus READ_MEMORY `FD07`): pass.** This is the first
  READ_MEMORY sent to this radio's mask ROM. 16 bytes at `0x12000`:
  `3fe96acd1e87b6eb76e45da7c9cc3ff6`; at `0x13000`:
  `2b9480ccb13ea215ea92b7a62dbad5f7`. Each address read the same twice. Neither
  matches the RAM helper in encrypted or decrypted form. That is consistent with
  untouched RAM after a fresh power-on (no helper was uploaded this session),
  which also suggests the ROM does not use `0x12000`/`0x13000` itself.
  *Correction (attempt 2 below):* READ_MEMORY returns RAM encrypted with the
  CrcDecode keystream, so these bytes are the ROM's encrypted view, not raw RAM.
  Decoded, `0x12000` holds `0x01EB4D10 0x01EB4650 0x01EB4698 0x01EB46E0`
  (pointers into the app's address space) and `0x13000` holds
  `0x00013004 0xFFFFFFFF 0x00013004 0x00013004` (self-referencing list
  heads). This is the stock app's RAM, not power-on noise; see attempt 3.

- **Full stage, attempt 1: stopped before any jump.** The first WRITE_MEMORY
  (`FB06`, 16 bytes to `0x13100`, for the cipher check) completed with CSW
  status 0, but the transport reported a nonzero residue. The vendored
  protocol treats that as "Short data write". The cause was in the new macOS
  transport: it returned the CSW residue field, whereas the vendored Windows
  port, which the flasher used successfully on this radio, computes residue
  from the bytes actually carried and ignores the CSW field. The transport
  now does the same and records every command where the CSW residue
  disagrees. The full stage reads back everything it writes, so an
  unaccepted write is still caught. Pending the rerun, the working
  hypothesis is that the BR23 ROM reports a bogus CSW residue for
  WRITE_MEMORY.

- **Full stage, attempt 2: the radio reset into the normal firmware.** Same
  power-on as attempt 1, with the residue fix.
  - Every command's CSW residue equalled the full transfer length (12 of 12,
    for example `FD07`, 16 bytes carried, CSW residue 16). The BR23 ROM's CSW
    residue field is meaningless, for reads as well as writes. The transport
    fix was correct.
  - The cipher check wrote the pattern encrypted and read back the
    *encrypted* pattern; it then wrote the plain pattern and read back the
    plain pattern. The tool concluded "stores as sent" and uploaded the
    program **unencrypted**. That conclusion was wrong. The flasher's
    encrypted RAM helper runs, so WRITE_MEMORY decrypts, so after the first
    write RAM held the plain pattern. The only consistent reading is that
    **READ_MEMORY encrypts what it returns with the same per-transfer
    CrcDecode keystream**. The check had assumed raw reads.
  - The ROM therefore stored the decrypted (scrambled) program at `0x12000`
    (`4f4419e7 8d3e15d3 …`). On the jump the CPU executed one byte load
    (`r7 = b[r4+4]`), then an undefined instruction at `0x12002`. The radio
    left USB Update mode, pyusb reported "Other error" for the jump, and the
    radio booted to the normal channel screen. Those bytes contain no flash
    access. The radio booting normally confirms the main firmware is intact.
  - Whether the jump returns is **still unknown**: the code that ran was not
    the marker program.
  - Fix: writes are always encrypted. The check now only decides whether
    reads are encrypted (encrypted pattern back) or raw (plain pattern back)
    and stops on anything else. Decoded reads decrypt. The fake ROM now
    models encrypting reads, and a regression test asserts the program is
    never sent unencrypted. 16 tests pass.
  - Also learned: a crash in USB Update mode resets the radio into its normal
    firmware. That is the benign failure mode U1 was designed around.

- **Full stage, attempt 3: pass.** The radio was powered off, then put back
  in USB Update mode. Reads were identified as encrypted ("writes decrypted,
  reads encrypted"). The decoded program in RAM matched the 24 uploaded bytes
  exactly. Both jumps returned in about 1 ms, the marker read `H9U1` with
  counters 1 and 2, and INQUIRY still answered afterwards. **The BR23 mask
  ROM's JUMP_TO_MEMORY calls the RAM program and returns to its USB command
  loop.** The resource-flash route can rely on the ROM and our own code
  alone, with no vendor RAM helper and no main-firmware change.
  - Entry registers: `r0 = 0x11384`, `r1 = 0x12000` (the entry address). The
    16-bit jump argument (`0x4839`) was in neither register. Where it goes
    is untested; the SPI2 program takes its inputs from a RAM parameter
    block instead.
  - RAM before the upload read identically in all three sessions, across
    two power-offs, one of them after the attempt-2 reset. Decoded, it is
    the stock app's data (see the read-stage correction). **RAM survives the
    radio being switched off**; the app's previous state is still present in
    USB Update mode. Code must not assume RAM is cleared, which the tag
    checks in S1/S2 already handle.
  - The CSW residue again equalled the full length on all 14 commands.

**U1 conclusion:** pass on one TD-H9 (macOS, USB 1.1 full speed). Next is
U2, a JEDEC ID read through the SPI2 RAM program.

## 2026-11-08 U2 tooling (built, not yet run)

- `resource_usb.py u2` uploads `spi2_rom`, writes the parameter block at
  `0x13040` (op 1, divider 31), jumps, and reads back status and JEDEC ID,
  twice. It refuses a timeout, an all-zero or all-one ID, an ID that changes,
  or a capacity other than 16 MiB.
- The U1 session code (report, transport, CSW notes) was factored into a
  shared runner. U2 reuses the U1 upload path that worked on hardware. 23
  offline tests pass.

## 2026-11-08 U2 hardware: SPI2 runs, the bus reads FFFFFF

- Same Mac and radio, USB Update mode. The cipher check and the 378-byte
  `spi2_rom` upload verified. Op 1 (stock SPI2 setup, divider 31) ran twice
  and returned in about 1 ms each time with **status ok and JEDEC
  `FFFFFF`**. INQUIRY was not re-run because the tool stopped on the ID.
- **SPI2 is clocked in USB Update mode:** control bit 15 set for every byte,
  so the shift register runs. The data line read all ones, so no chip drove
  MISO. The likely causes are that the pins are not configured as they are in
  normal boot (board init writes GPIO bank masks that never run in update
  mode), or that the flash is not powered or selected.
- Static follow-up:
  - Stock reads GPIO input at `[bank+4]` (app+`0x052558`, pin 6).
  - Board init at app+`0x05FD46` clears the pull-up/down of pin 23 (and 53).
  - A UART-style pin routine at `0x01E789C0..0x01E78A04` sets pin 25 as output
    and 26 as input with pull-up (also pairs 9/10 and 36/37). That is a UART
    pin option, not evidence of the SPI wiring.
  - Stock's SPI0 port-D row `[24, 26, 21, 25, 20, 2]` and SPI2 port-A row
    `[24, 26, 25]` share an order (CLK, DO, DI, …). That supports CLK 24,
    MOSI 26, MISO 25.
  - `my_app` also calls `0x01E7A1FE` (sends `0x01E786BA(85, 1, &1)`) before
    the flash setup, and `0x01E7A216` reads a 2-byte ID with command `90`
    after it. Neither has been identified as a power switch.
- New `spi2_rom` ops (712 bytes, `eebf23af…`), all read-only on flash:
  - op 3 snapshots GPIO banks 0–1, `0x1E5100..0x1E511C`, and SPI2
    control/baud by word reads.
  - op 5 bit-bangs `9F` on CS 23 / CLK 24 / MOSI 26 / MISO 25 with SPI2
    disabled, releasing MOSI to input after the command byte so a wrong
    data-pin guess cannot cause contention.
  - op 4 applies SDK-style pin setup (CLK and DO outputs, DI input, input
    enable on, pulls off) before the stock SPI2 setup.
  - `resource_usb.py u2 --op regs|bitbang|pins`. 25 offline tests pass.

## 2026-11-08 U2 register snapshot (op 3), fresh power-on into update mode

Word reads only. Bank 1 = GPIO 16–31 at `0x1E5040`:

| Offset | Bank 1 | Bank 0 | Reading |
|---|---|---|---|
| `+0` out | `00000000` | `00000000` | all low |
| `+4` in | `00000FCF` | `0000FC77` | bank 1: GPIO 16–19 and 22–27 high |
| `+8` dir | `0000FFFF` | `0000FFFF` | **every pin an input** |
| `+12` input enable | `0000FFFF` | `0000FFFF` | all enabled |
| `+16` pull-up | `00000002` (GPIO 17) | `00002000` (GPIO 13) | |
| `+20` pull-down | `00000000` | `00000000` | |
| `+24`, `+32` | `0000FFFF` | `0000FFFF` | unidentified |

- GPIO 23–26 are inputs and all read **high with no internal pulls**,
  which implies external pull-ups, consistent with the resource flash's
  chip select and SPI lines being on these pins.
- `0x1E5100..0x1E511C`: `766C 1 0 0 0 7F F 0`. `0x1E511C` bit 16 is clear,
  so SPI2 already maps to port A by default.
- SPI2 control `00008000` (pending flag set, not enabled), baud `0`.
- **Conclusion for the U2 FFFFFF:** in update mode nothing sets the SPI pins'
  directions, and the SPI2 peripheral does not override GPIO direction.
  CLK and MOSI never reached the bus, so the flash saw no clock and MISO
  stayed at its pull-up. Normal boot must configure these pins elsewhere
  (board init or an SDK pin routine not yet located). The `bitbang` and
  `pins` ops set the directions explicitly.

## 2026-11-08 U2 bit-bang (op 5): still FFFFFF

- Same session as the snapshot. CS 23 and CLK 24 were driven as outputs,
  MOSI 26 was driven during `9F` and then released, and MISO was sampled on
  25 with SPI2 disabled. Two runs, about 2 ms each: **`FFFFFF`**. The pins
  being undriven was therefore not the only problem.
- **Pin table re-read (correction):** each SPI controller's entry at
  `0x01EB4EE4 + 28·n` is a 4-byte port count followed by 6-byte port records
  in the order **[CS, CLK, DO, DI, D2, D3]**:
  - SPI0: count 4; A `[51, 50, 49, 48, 23, 53]`, B `[13, 14, 49, 48, 15, 53]`,
    C `[–, 6, 8, 3, 7, 2]`, D `[–, 24, 26, 21, 25, 20]`.
  - SPI1: count 2; A `[–, 18, 17, 16, –, –]`, B `[–, 35, 37, 36, –, –]`.
  - SPI2: count 2; A `[–, 24, 26, 25, –, –]`, B `[–, 13, 62, 61, –, –]`.
  The earlier reading started one byte late (stock reads the CLK byte at
  `+5`). It happened to give the same CLK/DO/DI values, so CLK 24 stands.
  Whether "DO" is the MCU's output or the flash's is not settled by the
  table.
- GPIO 23–26 read high through external pull-ups, which suggests the flash
  supply is up. Swapped data pins became the leading hypothesis.
- Op 5 now takes the data-pin masks from the parameter block (`+32` MOSI,
  `+36` MISO). The program refuses anything other than GPIO 25/26 in either
  order. Both pins start as inputs and MOSI is driven only during the
  command byte. `u2 --op bitbang-swap` sends MOSI 25 / MISO 26. Program 852
  bytes (`a28c8d7c…`); 26 offline tests pass.

## 2026-11-08 U2: bit-bang with swapped data pins, and a snapshot afterwards

- `bitbang-swap` (MOSI 25, MISO 26), two runs: **`FFFFFF`**. Both data-pin
  orders fail.
- Op 3 snapshot right after the bit-bang ops (same session):
  - bank 1 out `00000680`, in `00000E4F`, dir `0000FE7F`, ie `0000FFFF`,
    pull-up `00000002`. **The writes took effect:** GPIO 23 and 24 are
    outputs; 25 and 26 are back to inputs.
  - GPIO 24 (CLK): driven low, reads low. Normal.
  - **GPIO 23 (CS): driven high (`out=1`, `dir=0`) but reads low (`in=0`).**
    Before any program ran, it read high as an input. Something else
    drives the pad low and overrides the GPIO output.
  - Other registers unchanged from the fresh snapshot. SPI2 is disabled (the
    bit-bang op cleared bit 0); control `00008000`.
- **Hypothesis:** stock's SPI0 port-A record `[51, 50, 49, 48, 23, 53]` lists
  GPIO 23 and 53 as SPI0 data lines 2 and 3 (quad mode for the internal
  flash). Board init clears exactly those two pins' pulls. If the mask ROM
  runs the internal flash in quad mode in USB Update mode, SPI0 owns GPIO 23,
  so the external flash's chip select never behaves as commanded. In the
  running app, SPI0 presumably uses fewer data lines, leaving 23 as a GPIO.
- To test it, op 3 now also captures GPIO banks 2–3 (GPIO 53) and SPI0/SPI1
  control and baud (892-byte program, `6dd35f35…`). Stock's SPI2 mode routine
  (`0x01E4C1AE`) uses control bits 10–11 as the data width (0, then `0x400`,
  then `0x800`), so SPI0 bits 10–11 should show the mode.

## 2026-11-08 U2: SPI0 snapshot; correction to the GPIO 23 reading

- Extended op 3, same session: **SPI0, SPI1, and SPI2 control are all
  `00008000`** (bit 0 clear, so disabled; width bits 0) with baud 0. The
  quad-SPI0 hypothesis is **refuted**: no SPI controller is enabled in USB
  Update mode.
- **Correction:** "GPIO 23 driven high but reads low" was not evidence of
  contention. Bank 3 (GPIO 48–63): out `00000010`, in `0000002F`, dir
  `0000FFEF`, pull-up `00000018`, `+28` `00000010`. The ROM drives GPIO 52 as
  an output high, and it also reads 0. **The `+4` input register reads 0 for
  pins configured as outputs.** GPIO 23 was probably high as commanded.
- Bank 2 (GPIO 32–47): all inputs, in `000000FF`. GPIO 53: input, reads 1.
- Unchanged lead: bank-1 inputs read `0FCF` before any program ran, so
  **GPIO 20 and 21 are low** while 16–19 and 22–27 are high, all with pulls
  off. Stock's SPI0 port-D option `[–, 24, 26, 21, 25, 20]` offers a quad
  flash on these pins. A HOLD# or data line on 20/21 is possible.
- New op 6 (`--op sample` / `sample-swap`, 932-byte program `72971c29…`): the
  op-5 bit-bang that also stores the full bank-1 input word before chip
  select falls and after every clock rise (33 words at RAM `0x13600`). It
  drives nothing new: only CS 23, CLK 24, and MOSI during the command. The
  report lists which GPIO 16–31 change and their response bits. 27 offline
  tests pass.

## 2026-11-08 U2: sampled bit-bang; the flash never answers

- `sample` (MOSI 26) and `sample-swap` (MOSI 25), two runs each. Bank-1
  input before chip select: `0E4F`. During the command it was `0A4F` (MOSI 26
  driven) or `0C4F` (MOSI 25 driven), only because a pin set as an output
  reads 0. **All 24 response clocks read `0E4F`: no GPIO 16–31 changed.**
  The flash drove nothing in either data-pin order.
- That leaves two possibilities: the flash is not responding in update mode
  (unpowered, held, or never selected), or **GPIO outputs do not reach the
  pads in this mode**. Output pins always read 0, so the second has never
  been checked. If outputs do not drive, chip select never falls, which
  explains every result.
- Stock GPIO usage, from all pin-bank lookups followed by register writes:
  - **GPIO 27** is set to output high at `0x01E886E6`, right after `my_app`
    stores `0x5AA5` (`h[r0+16] = 23205` at `0x01E886DE`), and driven low at
    `0x01E50608`/`0x01E50716`. It is likely the flash's WP#/HOLD#. It comes
    after the stock JEDEC read and reads high (external pull-up) in update
    mode.
  - **GPIO 7:** routine `0x01E88246` counts RAM `0x10CF4`, holds the pin low,
    and pulses it high every 301 calls. It looks like a heartbeat or
    watchdog kick, not a supply enable.
  - Other `my_app` outputs: GPIO 18 (LCD command/data), 22 (LCD reset, low),
    and 0, 3, 6, 8, 9, 34, 35 after the flash setup. No pin is identified as
    the flash supply.
- New op 7 (`--op led`, program 994 bytes `da0d4f16…`): sets GPIO 9 (top LED,
  proven by the boot probe) as an output and pulses it twice, each half-cycle
  4,000,000 loop iterations. A visible blink shows that outputs drive pads in
  update mode. The run time also gives a rough CPU speed. 28 offline tests
  pass.

## 2026-11-08 U2: LED test; outputs work, the flash stays silent

- `--op led`: the user saw the top LED blink twice and then turn off. **GPIO
  outputs drive the pads in USB Update mode.** Chip select, clock, and MOSI
  from the bit-bang tests reached the board.
- Timing: 16,000,000 iterations of a two-instruction loop took 2.669 s
  including USB overhead, about 6 million iterations per second. That
  suggests a CPU clock of roughly 12–24 MHz in this mode. The bit-bang clock
  was therefore about 50 kHz.
- **Conclusion so far:** with correct pins, outputs that drive, and both
  data-pin orders tried, the resource flash does not respond in USB Update
  mode, although stock reaches it in normal boot. Something stock does
  first, such as power, a power domain, or an enable, is missing.
- JieLi boards sometimes power SPI flash from a high-drive GPIO. In update
  mode the ROM drives GPIO 52 as an output high with bank-3 `+28` (likely
  high drive) set. The only stock pin given the same treatment, GPIO 21 at
  `0x01E78844`, belongs to a UART pin-setup switch (`0x01E787C4`) called
  only by the UART open routine `0x01EC58BC` (the APRS link). **It is not
  the flash supply.**
- Not yet examined: non-GPIO power-domain or LDO registers in board init and
  `app_core` initialization. Direct register writes in board init are not
  covered by the pin-lookup scan.

## 2026-11-08 Decision: Bluetooth route, with a fast bulk read

The user chose the Bluetooth route (S1/S2) while the USB route waits on the
flash-power question, and asked for more speed.

- **Stock reply path:** the parser's send-byte callback `0x01E4FE82` (session
  `+4`, block variant `0x01E4FE9E` at `+8`) appends to the **80-byte** buffer
  at RAM `0x127B0`, the same buffer incoming GATT writes land in. It silently
  drops bytes beyond 80. After `0x01E5D018` returns, the GATT callback checks
  TX capacity (`0x01E5D2CA` → `0x01E50286` = `ble_user_cmd_prepare(9, …)`) and
  sends the reply with `0x01E5D2E0(12, 0x127B0, len)`. Handle 12 is the
  programming notify handle; handle 17 serves the other write path.
- `0x01E5D2E0(handle, buf, len)` checks for a connection (`h[0x11AD0+138]`)
  and notification enable (`0x01E17C28(handle+1)`), then calls
  `ble_user_cmd_prepare(10, 4, handle, buf, len, 0)`. It returns 0 on success.
- Consequences: pushing extra bytes through the stock reply caps at about 2×.
  Overlapping requests would corrupt the shared buffer. The parser runs
  inside the GATT write callback, where stock already submits
  notifications.
- **Bulk read (S1, inherited by S2):** `R` at `0x3C00 + 32·k` streams from a
  cursor (set by the bank-select status read) as notifications on handle 12.
  Packets are tagged `B7 5A seq`, with 16/48/112/176/240 data bytes for `k`
  0–4. Each request sends up to 2 KiB, checking capacity before every packet
  and keeping 48 bytes free for the stock reply. The 32-byte reply reports
  `H9BK`, status, start, count, the TX capacity seen, and the new cursor.
  Bulk state uses `0x1E4D8–0x1E4DF` and `0x1E4FC–0x1E4FF` (no code
  references). S1 is now 783 bytes and S2 1,618 bytes. Both builds
  reproduce, and the flasher and the HID restore plan accept them.
- The page matches every byte to the reply's start, count, and cursor. It
  steps the packet size down if a notification is refused or truncated,
  waits when the queue is full, restarts the bank on any other error, and
  falls back to 32-byte reads on an image without bulk support.
- **macOS flashing:** the generic `h9-flasher` gained `scsiio/libusbport.py`
  (darwin only), using a copy of the hardware-tested `usb_msc.py`. A test
  keeps the two copies identical. Windows/Linux paths and the Brick Breaker
  copy are unchanged.

## 2026-11-08 Mac flasher: probe and read-only backup

- Radio power-cycled back into USB Update mode. `h9_flasher.py probe
  --device 4C4A:2342` (libusb port): `BR23 UBOOT1.00 1.00`.
- `h9_flasher.py backup` (read-only) uploaded the 11,264-byte RAM helper
  through the ROM, started it, and read SPI NOR ID `0x856014` and chip key
  `0xF181`. It then read all 892,928 bytes of main flash. **The flasher's
  RAM-helper commands work over the macOS transport.**
- **Finding:** the backup's SHA-256 is
  `741bbeb87eb84cd7c6988054c56a267bb73c89bbe5cc74dc4e5bc0d75ce7e193`, the
  pinned **Brick Breaker** flash image, not stock (`74243eae…`). The radio
  had Brick Breaker installed. Flashing S1 therefore writes six sectors
  (S1's four, plus `0x8D000`/`0xAE000` back to stock) and replaces Brick
  Breaker. The pinned S1/S2 HID fallback assumes a stock radio, so the
  first-line recovery is `h9_flasher.py restore` with the backup taken by
  the flash command.

## 2026-11-08 S1 (with bulk read) flashed from macOS

- `h9_flasher.py flash` (package `93f7a701…`, `--stock-base` stock v1.0.38,
  libusb port): uploaded the RAM helper, read ID `0x856014` and chip key
  `0xF181`, and saved backup `~/h9-backups/h9-before-s1-20261108.bin` (Brick
  Breaker image `741bbeb8…`) with its manifest. It then updated **6 of 218
  sectors**, verified each sector, and verified a full 892,928-byte readback
  `152b3bd42b99b71b21b74ba30cb1e9221038642325901482d9cd3cceffec79d1` (the S1
  target). **First flash write through the macOS transport.**
- Recovery to the previous state: `h9_flasher.py restore` with the same
  package, stock base, and that backup.

## 2026-11-08 S1 over Bluetooth: service check passes; first bulk read fails

- **Check service** (status slot 0): version 1, flash ready yes, **JEDEC
  `C8 40 18` = GigaDevice, 16 MiB** (a GD25Q128-class part). This is the
  first live identification of the resource flash, and the first proof that
  the S1 hook and its JEDEC routine run on hardware.
- **Fast read of `resource` failed:** "Invalid read header at 0x3c80:
  00 00 00 00". The stock reply header (`W 3C 80 20`) was preceded by
  untagged zero bytes. The working explanation: the stack splits each
  243-byte notification to the link MTU, so only the first fragment carries
  `B7 5A`, and the continuation fragments (zeros at the start of `resource`)
  landed in the reply queue. Not yet confirmed.
- Page fix, no reflash: tagged packets are reassembled from the following
  fragments up to the expected length; the first notification sizes are
  logged, revealing the MTU; an incomplete packet (truncation rather than
  splitting) steps the packet size down. Console logging added: debug for
  every BLE write and notification, info for every bulk reply, warn for
  retries, and error for failures, plus `window.h9` for live state.

## 2026-11-08 S1 bulk read works: link MTU and TX capacity measured

- With fragment reassembly, the fast read of `resource` (3 MiB, two passes)
  runs at **about 2.1 KiB/s** with 240-byte packets: one packet, 240 bytes,
  per request. Each request is one round trip of about 110 ms.
- First request: **TX capacity 494 bytes**; notification sizes **182, 61,
  37**. A 243-byte tagged packet arrives as 182 + 61, so **the link's ATT
  payload is 182 bytes** (MTU 185 with macOS), and the stock reply is 37
  bytes. This confirms the fragmentation explanation for the first failure.
- With the 48-byte reply reserve, a second 243-byte packet never fits in
  494. The page now derives the link payload from the first notification
  and switches to the largest size that is not split: 176-byte packets
  (179 with the tag), two per request, 352 bytes per round trip.
- Firmware headroom identified, not implemented: size the last packet to the
  remaining capacity (about 440 bytes per request), or test whether the
  capacity recovers while still inside the GATT callback, which would let
  one request stream the full 2 KiB.

## 2026-11-08 Bulk read v2: fill the queue, optional waiting (built)

- The user asked for more speed. Changes to the bulk sender:
  - Each packet carries min(slot size, room − 44) bytes in multiples of 8,
    with a 4-byte header `B7 5A seq len`, so a round trip fills the 494-byte
    queue: about 450 bytes instead of 240.
  - Slots k 8–12 poll for room (at most 100 polls of 20,000 iterations) to
    test whether the queue drains while still inside the GATT callback.
  - The reply word packs count (12 bits), first capacity (10), and polls
    that ended with room (10).
- The page starts with 176-byte packets in wait mode. If no poll ever finds
  room, it switches to the no-wait slots. It reassembles split packets by
  their length byte.
- S1 is 867 bytes (package `400b12bd…`); S2 is 1,702 of the 1,704-byte cave
  (package `4f87f98e…`). Both rebuild byte for byte and pass flasher
  inspection. Flashing the new S1 over the installed S1 changes sectors
  `0x5000`, `0xAC000`, `0xD9000`.

- **Flashed from macOS:** backup `~/h9-backups/h9-before-s1v2-20261108.bin`
  (previous S1, `152b3bd4…`) with its manifest. Updated **3 of 218 sectors**
  and verified a full readback of `1d96c1500bad991a7efbed66bbe7a35a11f7b6ccd57bc128a47d80886145d23f`.

## 2026-11-08 Bulk v2 on hardware: every request timed out; reserve raised

- Read of `resource`, wait slot with 176-byte packets: all six attempts at
  `0xA00000` ended with "Timed out waiting for radio (0/4 bytes)". **No
  reply header arrived at all**, and re-entering programming and selecting
  the bank worked each time.
- Explanation: v2 kept only 40 bytes after its last packet for the 37-byte
  stock reply. Stock checks capacity (`0x01E5D2CA`) before sending its reply
  and **silently skips it when it does not fit**. The capacity evidently
  includes per-notification overhead beyond the data bytes. v1, which kept
  48 bytes after a full packet, always got its reply.
- Fix (v3): send only when room ≥ 76 and use room − 68 data bytes, so 64
  bytes stay free. With capacity 494 this should give about 176 + 176 + 48 =
  400 bytes per round trip. Packages: S1 `1160c6d7…` (flash `8b3c9f56…`), S2
  `e60d4cd5…`. Both rebuild byte for byte; the constants were checked in the
  disassembly.
- The timeouts left the radio otherwise untouched: the next request still
  entered programming mode normally.

- **v3 flashed from macOS:** backup `~/h9-backups/h9-before-s1v3-20261108.bin`
  (v2, `1d96c150…`) with manifest; updated 3 of 218 sectors; full readback
  verified `8b3c9f560d9d4f11c7836dfffbc8b7990ce62ab88bb396f3b56e2a93f5666c8a`.

## 2026-11-08 Bulk v3 on hardware: 416 bytes per request, 3.4 KiB/s

- Fast read of `resource` (macOS, Chrome): first request in wait mode with
  176-byte packets: **416 bytes** sent, TX capacity 494, **polls that found
  room: 0**. Notification sizes 180, 180, 68, 37: three unsplit packets
  (176 + 176 + 64 data bytes) and the stock reply.
- **The BLE TX queue does not drain while the GATT write callback runs.**
  Waiting inside the callback (up to 100 polls) never found room. Each round
  trip is therefore capped by the 494-byte queue. The page switched to the
  no-wait slots as designed.
- Throughput: **3.4 KiB/s** with 416 bytes per request, about 120 ms per round
  trip, against 2.1 KiB/s for v1 (240 bytes per request). That is about 15 min
  per pass for the 3 MiB `resource` partition and about 80 min for the whole
  chip.
- Remaining levers: the round-trip time (write without response, and the BLE
  connection interval, which only the radio can request to change) or
  overlapping requests (each callback copies its reply into the stack before
  returning, but a second request arriving before the queue drains could have
  its reply dropped).

## 2026-11-08 BLE connection parameters: stock uses slave latency 10

- The stock calls to `ble_user_cmd_prepare` (`0x01E16652`) use classes 1–4
  (advertising), 5 (disconnect), 7–10 (send path), 11, and 19. That matches
  JieLi's SDK enum, where **11 = `BLE_CMD_REQ_CONN_PARAM_UPDATE`**.
- Class 11 has one caller, stock `0x01E50316`. It reads index `b[0x11AD4]`
  (0–2; a caller at `0x01E5DF32` increments it, stepping down the table
  after a rejection) and the connection handle `h[0x11AD0+138]`, then sends
  `(11, 2, handle, &table[index])`. Table `0x01EA2A38` (min/max in
  1.25 ms units, latency, timeout in 10 ms units):
  - 0: 16/24 (20–30 ms), **latency 10**, 600 (6 s)
  - 1: 12/28 (15–35 ms), latency 10, 600
  - 2: 8/20 (10–25 ms), latency 10, 600
- **Slave latency 10 lets the radio skip up to ten connection events when it
  has nothing to send.** A request arriving at an idle radio can wait up to
  about 11 intervals, which fits the measured ~120 ms round trip.
- S1 v4 (984 bytes, package `4b2eaaa3…`, flash `ae4ec5a8…`): bulk slot 16
  (`R 0x3E00`) requests 15–30 ms with latency 0 and a 6 s timeout (within
  Apple's accessory limits), using a constant table at `0x01EA7CC0`. Slot 17
  (`R 0x3E20`) calls stock `0x01E50316` to restore stock parameters. The
  builder now pins `0x01E16652` and `0x01E50316`.
- The page requests the fast link after selecting the bank, waits 1.5 s,
  logs the average round trip at 50 and every 500 requests, and restores the
  stock link afterwards.
- **S2 no longer fits** the 1,704-byte cave with these additions (the builder
  refuses it). Its write module has to move to the second erased cave
  (1,004 bytes at app+`0xA9D9E`, sector `0xAE000`) before S2 can be rebuilt.

- **S1 v4 flashed from macOS:** backup `~/h9-backups/h9-before-s1v4-20261108.bin`
  (v3, `8b3c9f56…`) with manifest; updated 3 of 218 sectors; full readback
  verified `ae4ec5a8231192384abb68ffa72a3f2bdd8164af7f1e408979b9fba8760c0248`.

## 2026-11-08 S1 v4: fast-link request accepted by the stack, no speed change

- The write characteristic supports both write and write without response
  (the page used write with response).
- Custom range `0xA00000`, 256 KiB, one pass. Link request (slot 16):
  **status ok** (the stack accepted the request). After waiting 1.5 s, the
  first bulk request again carried 416 bytes with capacity 494; the
  notifications were 180, 180, 68, and 37 bytes. **Average round trip over 50
  requests: 122 ms (3.3 KiB/s), unchanged.** The user reported 3.0–3.1 KiB/s.
- The browser cannot see the negotiated interval. Two readings fit:
  (a) macOS chose 30 ms within the requested 15–30 ms, so only slave latency
  changed and that was not the limit; (b) about one notification per
  connection event, so four notifications per request at 30 ms is about
  120 ms. Both point to the connection interval, not latency, as the limit.
- Next checks: write without response (no reflash), and a fixed 15 ms
  request (min = max = 12), or a packet capture (Apple PacketLogger) to read
  the actual interval and packets per event.

- **Write without response: no change.** 256 KiB at `0xA00000` in 81 s, 631
  requests at 416 bytes each, average round trip **124 ms**, no size
  reductions or retries; SHA-256 `da95ba28…406f`. Of the 8,192 32-byte
  blocks, 4,356 are erased (all `0xFF`). The user saved the `.bin` and
  manifest locally.
- Every request produces exactly four notifications (180, 180, 68, 37):
  **about one notification per connection event, at about 31 ms each.**
  macOS evidently used 30 ms from the 15–30 ms request. The connection
  interval is the remaining lever.

- S1 v5 (1,008 bytes, package `200ce8e6…`, flash `c442fdfb…`): slot 18
  (`R 0x3E40`) requests a fixed 15 ms link (min = max = 12, latency 0,
  timeout 6 s) from an 8-aligned table at `0x01EA7CD8`. The page's link
  setting now offers fixed 15 ms (default), 15–30 ms, or the stock link.

## 2026-11-08 S1 v5: fixed 15 ms link doubles throughput

- Flashed S1 v5, then read 256 KiB at `0xA00000` in one pass with the link
  set to fixed 15 ms. Slot 18 returned ok; after 1.5 s the first request
  again carried 416 bytes (capacity 494; notifications 180/180/68/37).
- **Average round trip over 50 requests: 63 ms (6.4 KiB/s)**, against 122–124
  ms with stock parameters or the 15–30 ms request. **macOS accepts a fixed
  15 ms, latency-0 interval**, and the round trip halves. That is still four
  notifications per request at about one per connection event.
- Throughput so far over BLE on macOS: 32-byte reads (not measured, estimated
  0.3–1 KiB/s); bulk v1 2.1 KiB/s; bulk v3 3.4 KiB/s; v5 with the fixed 15 ms
  link **6.4 KiB/s**. At that rate: 256 KiB about 40 s, `resource` (3 MiB)
  about 8 min per pass, the whole 16 MiB chip about 43 min.
- Ceiling for this design: each request is bounded by the 494-byte TX queue
  (about 416 data bytes) and needs four connection events at 15 ms. The only
  remaining lever is overlapping requests, which risks the stock reply
  being dropped when the queue is still full. It is not attempted.

## 2026-11-08 S2 rebuilt across two caves

- The user is backing up `resource` (two passes) at about 6.5 KiB/s.
- Page: the progress line now shows the time left in this pass and the total,
  including the remaining passes at the current rate.
- S2's write module now goes in section `.s2`, linked by
  `resource_service_s2.ld` into the second erased cave (app+`0xA9D9E`, 1,004
  bytes). The S1 code stays in the first cave, **byte-identical to the S1 v5
  on the radio** (`c7382849…`). The builder handles multiple caves (each
  checked erased in stock and bounded) and per-stage sector sets. The cave
  starts at flash `0xAEE9E`, so the 834-byte module also reaches sector
  `0xAF000`.
- S2 package `29c50041…`, flash `e61d181b…`, sectors `0x5000, 0x62000,
  0xAC000, 0xAE000, 0xAF000, 0xD9000`; rebuild identical; flasher
  inspection passes. `resource_s2_read` (`0x01EA9E9E`) calls the S1 reader at
  `0x01EA78F0`; `W` enters `resource_s2_write` (`0x01EA9EF6`). The HID restore
  plan now takes each package's own sector set (S1 4, S2 6) and validates.
- Flashing S2 over the installed S1 v5 changes `0x5000, 0x62000, 0xAE000,
  0xAF000, 0xD9000`; `0xAC000` is identical.

## 2026-11-08 Boot logo located in the resource dump

- Two 256 KiB dumps of `0xA00000`, taken with S1 v3 and with S1 v5, are
  identical (SHA-256 `da95ba28…406f`), so the bulk path reads consistently.
- 4 KiB sector map of `0xA00000–0xA3FFFF` (`0` all zero, `#` data, `.`
  erased): `000####000` then `#` through sector 31, then erased.
- **The startup logo is a raw 160 × 128 RGB565 bitmap, big-endian (MSB
  first), with no header, at `0xA00000–0xA09FFF`: 40,960 bytes, exactly 10
  sectors.** Rendered this way it shows the TIDRADIO logo and "Talk It
  Direct" on black; the zero sectors are the black bands above and below.
  Other resources start at the sector boundary `0xA0A000`, so no other data
  shares the logo sectors. (The image is vendor artwork and is not
  committed; render the local dump to see it.)
- **Correction:** `make_boot_logo_patch.py` and the earlier notes assumed a
  128 × 160 little-endian image. Rendered that way the data shows as stripes;
  160 × 128 big-endian is correct, matching the MSB-first byte order found for
  the panel in the Snake work.
- Consequence: replacing the boot logo means rewriting these 10 sectors
  (erase with the CRC check, program, read back) through S2. No descriptors
  or pointers change.

## 2026-11-08 Full `resource` backup verified (S1 v5, macOS)

- `resource` (`0xA00000`, 3 MiB), two passes, fast read with the fixed 15 ms
  link and write without response: **both passes SHA-256 `8acaed7c…c89c`, 0
  differing blocks**, 477 s per pass (6.4 KiB/s), 7,562 requests at 416 bytes
  each, round trip 63 ms, no retries or size changes. Saved locally by the
  user.
- The first 256 KiB matches both earlier dumps exactly. Logo region
  `0xA00000–0xA09FFF` SHA-256 `54d2077d…4574`.
- **Partition usage:** only sectors `0xA00000–0xA1EFFF` (31 sectors, 124 KiB:
  the logo plus other images) hold data. **737 of 768 sectors are erased.**
- **S2 test sector chosen: `0xCFF000`**, the last sector of `resource`.
  It is erased in the backup and far from any data.

## 2026-11-08 S2 on hardware: first resource-flash write passes

- S2 (package `29c50041…`, flash `e61d181b…`) flashed from macOS; the service
  reported version 2.
- **Sector test at `0xCFF000`: passed.** Erased check: CRC `0xE03E`. Pattern
  programmed in 128 `W` blocks, with a status check after each, and read
  back: CRC `0xC082`, which equals the page's CRC-16 of `testPattern(0xCFF000)`.
  Erase with the pattern CRC, then a full `0xFF` read-back. Times: about 28 s
  to program and verify 4 KiB (one status read per block), about 9 s to erase
  and verify.
- **This is the first erase/program of the resource flash: guarded by the
  CRC proof, partition-limited, and checked block by block.**

## 2026-11-08 Boot logo writer (page section 5) and KD9CRQ logo

- Page section 5 reads the logo, previews it, prepares a new one (image fitted
  to 160 x 128 on black, or a 40,960-byte `.bin`, or a dump starting at
  `0xA00000`), and rewrites only the sectors that differ. Each sector: erase
  with the CRC of the contents read this session, then program with a status
  check after **every** 32-byte block (the path the sector test proved), then
  read back the whole logo at the end. Unit tests cover the RGB565
  big-endian conversion, `.bin` handling and the sector plan.
- User-supplied logo `KD9CRQ_TD-H9_boot_RGB565_BE.bin` (outside the repo):
  40,960 bytes, SHA-256 `d5d9d97c…4540`. It equals its 160 x 128 PNG converted
  to RGB565 big-endian (0 differing pixels); all 10 sectors differ from stock.
  Expected write time: about 10 x (9 s erase + 28 s program), plus a 7 s
  read-back, so about 6–7 minutes.

## 2026-11-08 Custom boot logo written on hardware

- **The KD9CRQ boot logo (`d5d9d97c…4540`) was written to `0xA00000` through S2
  and verified byte for byte by a full read-back.** The radio booted and
  showed the new logo (user confirmed: "It worked!").
- All 10 sectors were rewritten, about 10 s each including the erase with
  the CRC check and 128 blocks with a status check after each. The total was
  1 min 40 s, far under the 6–7 min estimate. The logo read and read-back
  each took 9 s at 6.0–6.7 KiB/s.
- This confirms the stock logo renderer does no content check beyond the
  descriptor length (unchanged) and a successful read.
- The stock logo is kept locally in the `resource` backup (first 40,960
  bytes, `54d2077d…4574`).

## 2026-11-08 Merging Brick Breaker and S2: space survey; the "caves" may be icon pixels

- Space needed: Brick Breaker game 1,628 B plus launcher 94 B; S1 1,008 B plus
  S2 834 B, so 1,842 B for the service. Erased runs of at least 256 B in the
  stock app: `0xA77F0` (1,704), `0xA9D9E` (1,004), `0xA51A8` (857). With
  Brick Breaker installed, 76 + 910 + 857 = 1,843 B remain, in three pieces.
  That is a one-byte margin, so not a workable merge as-is.
- The packer keeps app.bin at its stock size (resources follow it), so
  growing the app would move other files. Not pursued.
- **New finding (unconfirmed on hardware):** a pointer table at app+`0xB1F24`,
  next to the strings `Signal`, `GNSS Status` and `%3d.%01d`, lists
  `0x01EA2FB2`, `0x01EA55FA` and `0x01EA7C42` (app+`0xA2EB2`, `0xA54FA`,
  `0xA7B42`). These are 0x2648 bytes apart, which is 4,900 RGB565 pixels,
  i.e. 70 x 70. The bytes around every "cave" are grey-scale RGB565 values
  (`DEDB`, `2965`, `528A`, `B596`, `0020`). So the three 0xFF runs are very
  likely **white pixels inside three 70 x 70 stock icons**, probably on the
  GNSS status screen, rather than unused flash:
  - `0xA51A8` lies in icon A
  - `0xA77F0` spans the end of icon B and the start of icon C
  - `0xA9D9E` lies in icon C
- If so, Brick Breaker, S1 and S2 each draw machine code as noise in white
  areas of those icons. No code loads `0xB1F24` by absolute address, so the
  screen that draws them is not yet traced. **Hardware check:** open the GNSS
  status screen with S2 (or Brick Breaker) installed and look for noise in
  icon backgrounds.

## 2026-11-08 The three "code caves" are QR-code margins (decoded)

- **Correction to the entry above:** the three 70 x 70 images are **QR codes**,
  not GNSS icons. They are RGB565 big-endian, like the boot logo. Decoded
  with OpenCV's WeChat QR decoder (the plain detector read only A and C):
  - A (app+`0xA2EB2`): `https://cdn.shopify.com/s/files/1/0096/6479/0607/files/H9_GMRS_Version_User_Manual.pdf?v=1775805601`
  - B (app+`0xA54FA`): `https://cdn.shopify.com/s/files/1/0096/6479/0607/files/H9_Ham_Version_User_Manual.pdf?v=1775805601`
  - C (app+`0xA7B42`): `https://www.youtube.com/playlist?list=PLsw2V3krXH6ye3StOAfDGCxCev0M6bIIA`
  The screen that shows them is not yet traced; the table's neighbouring
  strings (`GNSS Status`, `Signal`) belong to other screens.
- The 0xFF runs are the QR codes' **white quiet zones**:
  - `0xA51A8`: bottom margin of A (pixels 4475–4899)
  - `0xA77F0`: bottom margin of B, then top rows 0–6 of C
  - `0xA9D9E`: from pixel 4398 (row 62, col 58) to the end of C, so a few
    pixels of the QR's last module row as well as its bottom margin
- Effect of installed builds (rendered from each raw app):
  - Brick Breaker changes 0 / 424 / 435 pixels of A / B / C
  - S2 changes 0 / 425 / 496 pixels
  - The damage shows as coloured noise bands in the margins
  - **All three codes still decode with both builds**, rendered as drawn
    with no added margin
- Consequence for a merge: using `0xA51A8` would add a noise band to the
  bottom margin of QR A, like B's today. That is cosmetic and scanning is
  expected to survive (still to be checked on the screen).

## 2026-11-08 Hardware: QR codes still work with the margin noise

- The QR codes are on the radio's **User Manual** menu. With the installed
  build, the top and bottom white space shows noise, and **the QR codes still
  scan fine** (user check). The user approved filling the QR white space
  with code. That includes `0xA51A8` (QR A's bottom margin), for a merged
  Brick Breaker + S2 build.

## 2026-11-08 Merged Brick Breaker + S2 build (offline)

- Stage `bb-s2` in `resource-service/build.py`. It starts from the exact
  hardware-tested Brick Breaker app, built by Brick Breaker's own builder and
  pinned: app `5c459345…`, package `208409d8…`. It then adds the S2 service
  in space Brick Breaker leaves erased. The builder rejects any byte change
  outside the two clone hooks and the three service regions.
- `resource_service.s` gains `.ifdef COMPACT`, defined only for this stage:
  - no slot 16 (15–30 ms link request) and no `link_params`
  - no in-radio wait: k 8–12 behave like k 0–4 and report 0 polls, so
    existing pages switch to no-wait after the first request
  - code in `.svc`, byte tables in `.tables`
  - every other reply byte is unchanged
  - S1 v5 and S2 still rebuild byte-identical to the tested packages
    (`200ce8e6…`, `29c50041…`)
- Layout (`resource_service_bb.ld`):
  - `.s2`, 834 B: app+`0xA51A8` (QR A margin, 857 B)
  - `.tables`, 48 B: app+`0xA7E50` (after the 1,628 B game, 72 B)
  - `.svc`, 888 B: app+`0xA9E00` (after the 94 B launcher, 906 B)
  - Entries: `R` → `0x01EA52A8`, `W` → `0x01EA5300`; Brick Breaker's
    `my_app` hook is unchanged.
- Package `b9d24cb4…`, flash `83f79fb3…`, 8 sectors from stock: `0x5000,
  0x62000, 0x8D000, 0xAA000, 0xAC000, 0xAE000, 0xAF000, 0xD9000`. Coming from
  Brick Breaker, 7 sectors are written (`0x8D000` is already identical).
- Checks:
  - rebuild identical, and `h9_flasher inspect` verified
  - HID restore plan (8 sectors) validated offline
  - game, launcher and hook bytes identical to Brick Breaker
  - disassembly reviewed: S2 calls the reader at `0x01EA9F00`; all table
    references fall in `0x01EA7F50–7F7F`; the compact bulk path checks TX
    room once
  - all three QR codes still decode, with 417 / 424 / 903 pixels changed
- RAM: Brick Breaker uses `0xC3AF`/`0xC3B2`, the stock PWM block at `0x12208`
  and GPIO/LCD registers; the service uses `0x1E4D8–0x1E4FF`. No overlap.

## 2026-11-08 Web flasher: Brick Breaker + Boot logo replaces Brick Breaker (local, untested)

- At the user's direction, the web HID flasher (`dist/firmware-update/`) now
  hosts only **Brick Breaker + Boot logo** (package `b9d24cb4…`) in place of
  the original Brick Breaker.
- Hosted seeds: stock sectors (`ae543e82…`, 8 sectors, rebuilds exact stock)
  and the original Brick Breaker's 7 differing sectors (`1cc3367b…`, rebuilds
  `741bbeb8…`). With them the page recognises, backs up, and **upgrades a
  radio still running the original Brick Breaker directly**.
  - `planUpdate` accepts a sector holding the original image's bytes and
    rejects CRC collisions with it.
  - The live check rebuilds the radio image from stock, build or original
    sectors before `knownAppMatches`.
- Old hosted files are removed. `brick-breaker/publish_browser_assets.py` is
  retired; `resource-service/publish_browser_assets.py` writes all three files
  and checks each rebuild exactly.
- Tests (`test_browser_flasher.mjs`, 8 pass):
  - plan from stock
  - direct upgrade from the original Brick Breaker
  - interrupted upgrade with mixed sectors
  - unknown contents refused
  - archives
  - simulated install, upgrade and restore, byte for byte
  - tampering rejected
- Stock restore point for the test: `~/h9-backups/h9-stock-v1.0.38-restore-point.bin`.
  The user's radio readback taken under the original Brick Breaker
  (`741bbeb8…`, identical to the pinned image) with the 8 stock sectors
  applied; byte-identical to the official v1.0.38 image, boot area included.
- Served locally at `http://127.0.0.1:8781/firmware-update/` for the first
  browser HID trial. Not yet deployed.

## 2026-11-08 Web flasher: quick HID backup

- User report: the HID backup takes 10+ minutes.
- Cause: on a radio holding a known image, the firmware area is verified with
  about 100 CRC requests, but the 38 settings sectors above `0xDA000` are
  recovered from shifted windows. That is 3 groups x 4,095 = **12,285
  requests**, about 49 ms each by the user's timing. The page never writes
  that area.
- Change:
  - `readAccessibleFlash(..., { settings: false })` returns the application
    area only (`0x5000–0xD9FFF`) after the known-image check, with a CRC
    rescan.
  - New archive format `h9toolbox-hid-backup-v2` (application only, marked
    as excluding settings). `loadHidArchive` accepts v1 and v2, and checks the
    metadata length before parsing.
  - The page defaults to the quick backup. A checkbox restores the full
    settings-area backup.
  - A radio not matching a known image still gets full recovery
    (65,520 requests).
- Also added step labels and measured time-left estimates to the web
  flasher's progress line.
- Test: a quick backup of a known application uses fewer than 120 window
  requests and round-trips through the v2 archive; an unknown application
  fails the known-image check.

## 2026-11-08 Hardware: web HID flasher works both ways on macOS

- **First hardware validation of the browser HID installer** (local copy at
  `http://127.0.0.1:8781/firmware-update/`, Chrome on macOS, PTT+2 HID mode).
  The user reported:
  - Restoring stock over Brick Breaker + Boot logo worked.
  - Installing Brick Breaker + Boot logo over stock worked.
  - The quick (application-only) HID backup ran "super quick".
- Restore point: `h9-stock-v1.0.38-restore-point.bin`.
- This is the first complete browser install and restore on hardware for any
  package. It also validates WebHID on macOS, which the site footer listed as
  untested.

## 2026-11-08 Boot logo on the website: step 04 and /boot-logo/ (local, untested on hardware)

- `dist/boot-logo/radio.mjs`: the Bluetooth session ported from the hardware-
  proven prototype, with an injected transport:
  - handshake
  - fast bulk read (fixed 15 ms link requested then restored, packet
    reassembly, MTU-based packet size, retries) with a 32-byte fallback
  - guarded logo write: CRC-checked erase, status check after every block,
    full read-back
- `dist/boot-logo/logo-tool.mjs`: UI shared by Firmware update **step 04**
  and the new standalone **/boot-logo/** page.
  - Any browser-decodable image (PNG/JPG/WebP/GIF/BMP), with EXIF
    orientation, halving downscale, fit/fill/stretch, background colour for
    transparency and bars, optional sharp pixels, and Floyd–Steinberg dither
    to RGB565.
  - A saved `.bin` puts a logo back.
  - **Write stays disabled until the current logo has been saved** in this
    connection.
  - Progress with measured time left.
  - A clean stop resumes; any other failure requires a re-read before
    resuming.
- Tests: `test/boot-logo-radio.test.mjs` (7 pass) drives `radio.mjs` against
  a simulated S2 radio (status slots, bank cursor, bulk streaming split at the
  MTU, link slots, guarded erase/program with the firmware's result codes):
  - service check
  - fast read, including with a 100-byte MTU
  - fallback with no bulk read
  - writing only the changed sectors, including an erase-only sector,
    without touching neighbouring data
  - refusal when the radio changed since the read (no erase)
  - stop and resume
- Browser check (Chrome, local):
  - both pages mount with no console errors
  - a 1200x900 PNG with a transparent hole converts with correct bars,
    background and gradient
  - Write stays locked before a read and save
- The update page's wording now reflects the hardware-validated install and
  restore.

## 2026-11-08 Renamed to "H9 Toolbox Firmware"; success panels

- The merged build is now presented as **H9 Toolbox Firmware**: stock v1.0.38
  with Brick Breaker (hold Back) and the boot-logo service.
  - Hosted files: `h9-toolbox-firmware-v1.0.38.fw` and
    `h9-toolbox-firmware-stock-sectors.bin`
  - Build output: `h9_toolbox_firmware_*` (stage `bb-s2`)
  - HID archives of it: `TD-H9-H9-Toolbox-Firmware-…`
  - **The package bytes are unchanged** (`b9d24cb4…`, flash `83f79fb3…`).
    Earlier log entries keep the old name.
- The web flasher shows a large result panel after Install or Restore:
  - success: the sector count, CRC verification, time, and what to check on
    the radio, with a "Continue to step 04" button
  - failure: the error and how to resume
- The logo tool shows a result panel after a write ("all 40,960 bytes
  read back and match"), and a "Verified on radio" badge and outline on the
  radio preview once the read-back matches.
- Checked in Chrome with sample content. Fixed a contrast bug: the badge
  inherited the caption's grey text.

## 2026-11-08 Web flasher: settings-area backup option removed

- At the user's request, the "also back up the settings area" checkbox is
  gone. The page always makes the quick firmware-area backup when the radio
  holds a known image. A radio with unknown firmware still gets full
  recovery automatically; at about 49 ms per request, that is 65,520 requests
  or roughly 53 minutes, and the page now says "about an hour".
- `readAccessibleFlash`'s `settings` option and the v1 archive format remain
  for compatibility and tests.

## 2026-11-08 Web flasher: step 04 collapsed unless H9 Toolbox Firmware is confirmed

- Step 04 (boot logo) sits in a collapsed `<details>` by default. It opens
  when the page sees H9 Toolbox Firmware on the radio:
  - after a verified install of the build
  - when an Inspect, backup, or final CRC scan matches the build image
- It stays closed, with a note to install first, when the radio matches
  stock or the original Brick Breaker. Unknown or mixed radios leave it
  unchanged.
- The user can always open it by hand (e.g. firmware already installed,
  logo only).
- Checked in Chrome: closed by default, tool mounted inside.

## 2026-11-08 Web flasher: backup without choosing firmware first

- User question: why must firmware be selected before a backup? It needn't
  be. The backup used the target only as a reference image. It now loads the
  three hosted references (stock rebuilt from the build plus its seed, H9
  Toolbox Firmware, and the original Brick Breaker; all hash-checked) when
  none are loaded.
- The protected-boot placeholder comes from a known image; the boot region
  is identical in all of them and is never written.
- The archive made on the page is kept (`createdBackup`). When firmware is
  chosen afterwards it is checked against that target, without the user
  re-selecting the file. A chosen backup file takes precedence.
- A page-made backup of a modified radio is kept but is not offered as the
  stock restore point for an install; the page shows a note, not an error.

## 2026-11-08 Web flasher: steps reordered; Inspect folded into the write

- New order:
  1. **Connect radio** (button renamed from "Connect HID radio"), then
     **Back up the radio**
  2. **Select firmware**, with the restore-point backup; the backup made in
     step 01 is used automatically
  3. **Write firmware**
  4. Boot logo
- The chosen firmware card animates: a one-time pulse, a green outline and a
  "✓ Selected" badge, with the other option dimmed. Reduced motion is
  respected.
- The Inspect button is gone. **Install** and **Restore** each run the same
  checks first (`inspectRadio`): full CRC scan, pinned 8-sector plan, live
  shifted-window check, interrupted-sector rule. They then write the sectors
  that still differ, or report "already installed / already matches" and
  write nothing.
- This replaces the old inspect-then-rescan comparison; the safety checks
  are unchanged.
- Step 03 shows what is still missing (connect, select, back up, tick the
  box) or "Ready. The radio is checked again before anything is written."
- Checked in Chrome: selecting the hosted build animates the card, dims
  stock, relabels the button "Install H9 Toolbox Firmware", and shows
  "Connect the radio in step 01."

## 2026-11-08 Web flasher: backup moved into step 03; Install gated on it

- Step 03's buttons are now **Backup radio**, **Install [H9 Toolbox
  Firmware | stock v1.0.38]**, **Restore backup**; step 01 is connect only.
  The restore-point file stays in step 02.
- `radioBackedUp` is reset on every connect and set when a page backup of
  the connected radio succeeds. **Install stays disabled until it is set**,
  and still needs a valid stock restore point.
- The colour follows the next action: Backup is the primary button until it
  succeeds, then Install (once enabled). The hint under the buttons says
  what is missing.
- Checked in Chrome: Backup coloured, Install and Restore grey, no console
  errors.

## 2026-11-08 Web flasher: reinstall/update on a radio already running H9 Toolbox

- Problem: a radio already running H9 Toolbox Firmware could not reinstall
  or update without the user finding a separate stock backup file. Its own
  page backup (an H9 Toolbox image) is not a stock restore point.
- Fix: when the page backup shows the radio runs a **known** modified image
  (H9 Toolbox Firmware or the original Brick Breaker; matched by sector and
  shifted-window CRCs) and no file is chosen, the restore point is exact
  official stock v1.0.38 rebuilt from the hosted, hash-checked files
  (`source: 'hosted-stock'`).
  - That is exactly the HID-writable firmware area a stock radio holds; this
    was confirmed on the user's radio earlier (restore-point derivation).
  - Restore is labelled "Restore stock v1.0.38".
  - A radio with unrecognised firmware still needs the user's own stock file.
- Reinstall on a radio already matching the package checks every sector and
  reports "already installed … up to date" without writing. An update or
  partial install writes only the sectors that differ.
- After a page backup, the restore point is chosen by the same
  `validateSelectedBackup` path as a chosen file.
- Test added (9 pass): a build radio plans 0 sectors to install and 8 to
  restore against rebuilt stock; the original Brick Breaker plans 7 to
  install. Browser load check is clean.

## 2026-11-08 Bluetooth programmer: H9 Toolbox detection, fast read, boot logo (simulated, not yet on hardware)

- `dist/app.js` probes the H9 Toolbox status block (`R 0x4000`) as the
  first read of each session.
  - Stock firmware returns ordinary clone bytes (rejected by the parser,
    session continues) or nothing.
  - On a timeout the page exits, waits 300 ms and re-enters before the
    normal read. That costs about 2.5 s only for stock radios that ignore
    the address.
- **Fast read:** the clone image is the start of `rw_freq` (chip
  `0x900000`; the stock clone reader uses selector 4). With the service, the
  programmer bulk-reads `0x900000 + 0x3140` through the shared
  `ResourceRadio` (fixed 15 ms link requested, then restored). It then
  compares 4 blocks (first, last, two random) with normal clone reads. Any
  error or mismatch logs why and falls back to the standard 394-block read.
  **The rw_freq mapping is inferred from the stock code and still needs a
  hardware confirmation;** the cross-check guards it.
- Writes are unchanged (stock `W` path).
- `ResourceRadio` shares the connection by receiving every notification (each
  reader clears its own queue per session).
- **Boot logo card (step 06)** appears once H9 Toolbox is detected and uses
  the logo tool's new `connection` mode: the host supplies the session and
  busy state, and the tool hides its own Connect controls. The two block
  each other while busy.
- Timed progress (step label, measured time left, "done in") for reads and
  writes, as on the firmware update page. Logo-tool styles (result panel,
  badge) moved into `logo.css` so the tool is self-contained.
- The simulator moved to `test/support/simulated-radio.mjs`. It gained a
  stock mode and clone reads from `rw_freq`, and runs in the browser behind
  a fake Web Bluetooth device.
- Browser results (real programmer page, simulated radios):
  - H9 Toolbox: detected, fast read in 15 radio requests instead of 394,
    4/4 cross-check blocks match, logo card shown, logo read byte-exact
    (SHA-256 matches), buttons interlock
  - stock answering the probe: 395 requests, no card
  - stock ignoring the probe: timeout, re-sync, normal read, no card; ETA
    shown ("about 9 s left" … "Read complete in 16 s")

## 2026-11-08 Free RAM survey and loader design

- Goal: run code from the ~2.9 MiB of free `resource` space. The CPU cannot
  execute from the SPI chip, so modules must be copied into RAM.
- Static RAM map in
  `h9-firmware-research/evidence/stock/main-ram-map.md`:
  - RAM code `0x0–0xDA77` (copied from app `0xC618C`)
  - startup stack below `0xE290`
  - `.data`/`.bss` `0xE2E0–0x1E4D3`
  - heap `0x1E500–0x2BEFF` and `0x2D280–0x2FF7F` (527 × 128-byte blocks,
    67,456 B)
  - IRQ vector table `0x2BF00–0x2BFFF` (new finding)
  - probable main stack `0x2C000–0x2D1FF`
  - retained record `0x2FF80+`
- **No large fixed region is unused**; free RAM is the heap's free blocks.
  The used-block counter is the halfword at `0x1D7D0 + 216` (`0x1D8A8`); the total is at `0x1D8AA`.
- The block allocator is RAM code `0x166A`; no direct callers, so its
  calling convention is unconfirmed.
- Design (`resource-loader/DESIGN.md`): reserve a fixed RAM slot by lowering
  the heap part-1 end at app+`0x05FA9E` (`r5 = 0x2BF00` → e.g. `0x29F00`,
  8 KiB). Modules are linked for that address and stored in `resource`
  behind a directory at `0xA20000`, each with a header and CRC. A
  ~120–160-byte flash stub loads them (dev trigger: write-service op 3).
- Phases: (0) measure peak heap use with an S2 + RAM-read research image;
  (1) reserve and check stability; (2) loader plus a trivial module;
  (3) move Brick Breaker into a module to free the first cave.

## 2026-11-08 JieLi C toolchain running on macOS (branch charlie/loader-toolchain)

- JieLi's official Linux toolchain 2.5.3
  (`jieli-linux-toolchains-20260730.1.tar.xz`, SHA-256 `a1545cdd…`, MD5 equal
  to the server ETag) runs on Apple Silicon in Docker (Colima, vz + Rosetta).
  The image holds only Debian plus python3/make; the toolchain is mounted
  read-only (`firmware-research/toolchain/h9cc`). It is not committed.
- `clang version 4.0.1`; all of clang/ld/objdump/objcopy resolve their
  libraries in Debian bookworm amd64.
- `examples/gradient.c` compiles with `-target pi32v2 -mcpu=r3 -Oz` to 66
  bytes. Our Python Pi32v2 objdump decodes it identically and our linker
  places it at `0x29F00`, so loader modules can be written in C and linked
  with the existing build tools. No compiled C has run on the radio yet.

## 2026-11-08 Phase 0 research image: S2 + RAM read (offline, not yet flashed)

- `resource_service.s` gains `.ifdef RAMREAD`:
  - Bulk slot 19 turns the cursor set by the last status read into a RAM
    cursor (tag `0xB8`); bulk slots then copy RAM words instead of calling
    the chip reader, limited to `0x30000`.
  - Its 252-byte frame keeps the tag at `[sp+248]`.
  - Status 5 if no bank was selected; normal builds answer slot 19 with
    status 4.
  - S1, S2 and H9 Toolbox Firmware rebuild byte-identical (RAMREAD is not
    defined for them).
- Stage `s2-ram`: package `eb281b43…`, flash `ba05f7c8…`, first cave
  1,088 bytes, same 6 sectors as S2. Flasher inspection verified; HID
  restore plan pinned and validated; disassembly reviewed.
- Shared modules: `RAM_SELECT_ADDRESS`, a `cursorRam` flag in
  `parseBulkReply`, and `ResourceRadio.readRam()`, which re-selects RAM after
  any retry. The simulator gained `ramread` and `dropBulk` fault injection.
  Its bulk-slot rule was fixed: only slots 0–4 and 8–12 stream; others answer
  status 4, as in the firmware.
- `prototypes/ram-probe/`:
  - **Heap now** reads `0x18000–0x1D8BF` and parses the heap control
    block: used at `0x1D8A8`, total at `0x1D8AA`, bitmap bits set = free,
    bounds at +200/+204.
  - Checks bounds `0x1E500`/`0x2FF80`, total 527, and bitmap vs counter.
  - Labelled history with the lowest free value highlighted, a 15 s watch,
    and a 192 KiB dump.
- **Correction:** heap part 2 starts at `0x2D200`, not `0x2D280`. The stock
  setup rounds `0x2D27F >> 7` down (block `0x5A4`), so 436 + 91 = 527
  blocks. `main-ram-map.md` and the counter addresses were fixed too (the map
  said `0x1D8A6`/`0x1D8A8`).
- Tests: 8 radio tests, including RAM read with a forced retry; 2 heap tests
  against the stock heap setup. Browser check with a simulated radio: 40/527
  used, all checks ok.

## 2026-11-09 Full 16 MiB chip map (user's full-chip read)

- The user read the whole chip in one fast pass (SHA-256 `b6d60e2e…`, 2,584 s,
  40,330 requests, 64 ms). The `resource` range matches the two-pass backup
  except the 10 rewritten logo sectors.
- New: `h9-firmware-research/evidence/stock/resource-flash-map.md`.
  - `filesystem` is a FAT12 volume with 16 MP3 prompts (digits and Bluetooth,
    connection and FM announcements; MPEG-2 Layer III 16 kHz 32 kbps mono)
    and 843 KiB free.
  - `voice` is a JLFS container `tone` with 81 files (RIFF/WAVE PCM 11,025 Hz
    plus two MP3s); all header CRCs valid.
  - `font` has magic `DDCCBBAA` and seven section offsets (not yet decoded).
  - `record` is empty.
- Free space writable by the service: about 2.9 MiB in `resource`, 932 KiB
  in `font`, 356 KiB in `voice`.

## 2026-11-09 Clone image confirmed at rw_freq 0x900000 (hardware)

- A programmer backup (`.img`, 12,616 bytes: 8-byte identity `TDH9`… `4E`,
  then 12,608 bytes) equals the full-chip dump's `0x900000–0x90313F` byte for
  byte. **The Bluetooth clone image is the start of `rw_freq`**, which the
  programmer's H9 Toolbox fast read relies on. This was previously inferred
  from the stock reader's selector 4.
- `sys_data` (`HW102` header, a text-record sector, two full/partial sector
  pairs that look like growing logs) and the rest of `rw_freq` (one block at
  `0x905000`) are stock-owned: their erased space is not usable for our data.

## 2026-11-09 Hardware: the radio acknowledges an exit late (programmer fast read fix)

- The live programmer detected H9 Toolbox Firmware but its fast read failed
  with "Unexpected TD-H9 identity: 06 54 44 48 39 ff ff ff" and fell back to
  the standard read (15.7 s).
- Cause: **the radio sends an `06` for the exit command `0x45`, late.** The
  programmer exited its detection session and started the fast session
  immediately. The late `06` was taken as the handshake ACK, so the real
  handshake ACK sat in front of the 8-byte identity. Pages that wait 300 ms
  after an exit never saw it, and the earlier simulator never acknowledged
  exits.
- Fix (`6124475`, live; cherry-picked here as `ea6c93f`):
  - wait 300 ms between the detection and fast sessions
  - both Bluetooth sessions skip up to four stray leading `06` bytes before
    the identity (which always starts with `T`, `0x54`)
  - the simulator queues the exit ACK ahead of its next reply; a regression
    test reproduces the exact hardware error without the fix and passes
    with it
  - a browser run of the real programmer against that simulator: fast read,
    15 requests, 4/4 cross-check blocks matching
- The fallback after the failure worked: 300 ms wait, then a clean standard
  read. That suggests the real ACK arrives within 300 ms.

## 2026-11-09 Hardware: programmer fast read works (after the late-ACK fix)

- Live programmer, H9 Toolbox Firmware radio: detection, fixed 15 ms link
  requested and restored, **fast read 12,608 bytes in 3.8 s, 4 of 4
  cross-check blocks match the normal read**, whole read 4.7 s (the standard
  read took 15.7 s, so 3.3x faster). About 1.5 s of the fast read is the
  deliberate wait for macOS to apply the faster link, worth more on large
  reads than on this 12 KiB image.

## 2026-11-09 First C firmware built (c-hello, offline)

- `firmware-research/c-hello/`: `entry.s` copies the hardware-proven stage-23
  startup unchanged (LED, stock pin registrations, controller setup, panel
  reset/init, backlight, LCD helpers) and then calls `h9_main()` from
  `main.c`, compiled with JieLi clang 4.0.1 in Docker
  (`-target pi32v2 -mcpu=r3 -Oz -ffreestanding -fno-builtin`).
- C API wrappers: window, write, key, delay, LED.
- JieLi clang ABI, from a probe: arguments `r0–r3` (more on the stack),
  result `r0`, `r4–r12` preserved across calls. Cross-object calls use
  `R_PI32V2_LONG_JUMP_23M2`, which the repository linker resolves.
- Program: full-screen gradient plus a key-driven 16×16 square, the LED
  toggling per press.
- Build: 1,062 bytes in the stage-23 cave, same hook and 4 sectors as
  stage 23. Package `dd24a8ad…`, flash `e12e2700…`; reproducible including
  the C compile; flasher inspection verified. Disassembly: startup →
  `h9_main`, C → wrappers → stage-23 helpers.
- Colima shares only `$HOME`, so `h9cc` must run from a folder under home.

## 2026-11-09 Takeover plan (hybrid of reclaimed flash and RAM-loaded programs)

- The user chose a hybrid: reclaim stock code that is dead once `my_app` is
  replaced (resident code), and use the RAM it frees for programs loaded from
  the resource chip (Doom-class goal).
- `takeover/reachability.py`, first estimate from the stock call index:
  - 682 of 1,560 functions are reached from reset (cutting `my_app`;
    48 address-taken functions kept as roots)
  - about 389 KB of 811 KB of flash code is not reached
  - largest candidate run: 75,748 B at app+`0x65EE0`
  - not proof: 1,477 indirect jumps are unresolved, and the ROM may call
    back into the app
- `takeover/DESIGN.md`: next is a hardware "trap test" per region, then RAM
  measurement in takeover mode, then the resident loader/runtime.

## 2026-11-09 C-hello gains hardware readouts (offline)

- `main.c` now measures, in ticks of the stock timer-3 counter at
  `0x1D8B0`:
  - full-screen fill
  - sixteen 16×16 squares
  - busy-loop iterations per tick
  - `h9_delay(100)`
  - a live TICK line, for converting ticks to seconds with a stopwatch
- A 3×5 font (digits plus the label letters) draws at 2× scale.
- `TICK STUCK` is shown if the counter never advances; the busy loop is
  capped.
- Size: the `switch`-based font overflowed the cave (1,758 > 1,704 B); a
  lookup table brings it to 1,680 B. Package `941f40c2…`, flash
  `941e2586…`; reproducible; flasher verified.
- `test/host_preview.c`: the same C compiled natively with stand-in hardware
  writes a 160×128 PPM, checking window bounds and write sizes. The preview
  showed the expected panel, gradient, square movement and TICK line. The
  `H9_HOST_TEST` hook leaves the radio build byte-identical.

## 2026-11-09 C-hello on hardware: first speed measurements

- Flashed via the passwordless sudo rule. Backup `h9-before-c-hello-20261109.bin`
  (`83f79fb3…`, i.e. H9 Toolbox Firmware); 8 sectors updated; readback
  verified `941e2586…`. The first C firmware boots and runs on the radio.
- Readings: FILL 2, SQ 0, LOOP 199,873, DLY 10. TICK advanced 3,000 in a
  30 s stopwatch run.
- Tick: the counter at `0x1D8B0` runs at 100 Hz (10 ms per tick).
- Display: a full 160×128 RGB565 write (40,960 B in 128-byte transfers plus
  windows) takes 2 ticks, i.e. 10–30 ms: about 33–100 full frames/s of raw
  throughput, 1.4–4 MB/s. The stage-18 assembly fill (about 9.5 s) was
  limited by its per-pixel path, not the LCD bus.
- Small writes: sixteen 16×16 squares (8 KiB, 16 windows) take under one tick.
- CPU: the busy loop is 5 instructions per iteration (taken branch, load from
  `0x1D8B0`, add, sub, compare-branch; checked with `objdump -d` on
  `build/main.o`). 199,873 iterations per 10 ms = about 20 M iterations/s,
  so about 100 M simple instructions/s, timer interrupts included. The clock
  is at least about 100 MHz, more if loads or taken branches cost extra
  cycles.
- Delay: `h9_delay(100)` took 10 ticks = 100 ms, so its unit is 1 ms.
- Doom: neither CPU (about 100 MIPS, fixed-point game) nor display (tens of
  frames/s) looks like the limit. RAM (about 192 KiB total, shared with
  stock) and code space remain the constraints.

## 2026-11-09 C-hello blocks hold-8 update mode; PTT+2 HID still works

- With C-hello running, holding 8 at power-on no longer enters the
  `4C4A:2342` BR23UBOOT update mode; the user reports hold-8 only works from
  the normal firmware. The hold-8 check therefore lives in app code that
  C-hello's stage-23 takeover never returns to (C-hello loops forever).
- PTT+2 at power-on still enumerates as `4C4A:4155` "USB Composite Device"
  (HID updater), so recovery remains possible over HID.
- The web flasher cannot be used: C-hello's sectors (`0x5000`, `0x8D000`,
  `0xAC000`, `0xD9000`) match neither stock, H9 Toolbox nor Brick Breaker,
  so `planUpdate` refuses ("Radio differs from the supplied backup").
- Route back: `resource-service/restore_hid.py restore` with the stock
  restore point and the H9 Toolbox package. Its pinned 8-sector set covers
  all four C-hello sectors, so it restores stock; then reinstall H9 Toolbox.
- `restore_hid.py` needs the `hid` module (PyPI `hidapi`); the `h9usb` venv
  did not have it.
- Rule for future takeover images: keep an exit to update mode (for example,
  a key that returns to the stock boot path), or document PTT+2 HID as the
  only recovery.

## 2026-11-09 C-hello removed over PTT+2 HID (stock restored)

- `hidapi==0.15.0` installed in `~/h9usb` (with the user's approval).
- The first `restore_hid.py inspect` failed with "Unexpected HID report
  descriptor". macOS hidapi rebuilds the descriptor from parsed elements and
  encodes Logical Minimum 0 as `16 00 00` instead of `15 00`; the meaning is
  identical. `probe_hid_recovery.py` now has `REPORT_DESCRIPTORS` accepting
  exactly the original and that one re-encoding;
  `restore_logo_over_hid.py` uses it.
- Inspect: `0x5000`, `0x8D000`, `0xAC000` and `0xD9000` were not stock (the
  C-hello sectors). The other 209 accessible sectors matched stock.
- Restore wrote those four sectors from the stock restore point
  (`74243eae…`); each was verified by CRC, and a full rescan showed all 213
  accessible sectors at stock. Next: reinstall H9 Toolbox Firmware via
  hold-8.

## 2026-11-09 H9 Toolbox Firmware reinstalled after C-hello

- Hold-8 worked again from stock. The pre-flash backup
  `h9-before-h9tb-20261109.bin` hashed to `74243eae…` (exact stock), which
  confirms the HID restore. 8 sectors were updated; readback verified
  `83f79fb3…`.

## 2026-11-09 Dead-code estimate corrected: about 96 KB, not 389 KB

- The first `reachability.py` pass put the `app_core` task entry
  (app+`0x066648`, passed as an immediate at app+`0x05FFC4`) inside its
  largest "dead" run (75.7 KB at `0x65EE0`). A trap test there would have
  fired at boot.
- Corrected roots: 32-bit values that point at a function prologue (673).
  About 96 KB not reached; largest run 8,174 B at app+`0x538E0`. With every
  pointer-like word as a root: 77 KB (floor). Details in
  `takeover/DESIGN.md`.
- Consequence: the other stock tasks keep most code live, so replacing
  `my_app` alone frees little flash. That fits the user's wish to keep
  Bluetooth (the `btstack` task) anyway.

## 2026-11-09 Main flash tail mapped (read-only 1 MiB dump)

- `h9_flasher.py dump` (new, read-only) read the whole 1 MiB SPI NOR in
  hold-8 mode. The dump is private (`~/h9-backups/h9-main-flash-full-20261109.bin`,
  `45276aeb…`) because the settings area holds the radio's Bluetooth name and
  settings; it is not committed. The first `0xDA000` bytes hash `83f79fb3…`
  (H9 Toolbox Firmware), as expected.
- Flash file table (decoded with key `0xFFFF`): `uboot.boot` at `0xA0`,
  `isd_config.ini` at `0x45B0` (0x71 bytes: SPI 2_3_0_0, RESET PB01_00_0,
  UPDATE_JUMP 0, PSRAM 0, VLVD 4, EOFFSET 0; no VM size), `app_dir_head` at
  `0x5000`, `key_mac` at `0xFF000` (4 KiB).
- Tail by 4 KiB sector:
  - `0xD8000–0xD9FFF`: end of the app and package metadata (known).
  - `0xDA000`: settings VM, magic `55 AA AA 55`, 2,369 bytes used; contains
    the Bluetooth name `TD-H9-0536`.
  - `0xDB000–0xFCFFF`: **34 sectors (139,264 B) erased**.
  - `0xFD000`: 10 bytes (`f7 66 60 00 b3 f3 70 0b cc ce`), role unknown
    (possibly Bluetooth pairing/address data).
  - `0xFE000–0xFFFFF`: erased; `0xFF000` is the reserved `key_mac` entry.
- Not yet known: how large the VM area is. JieLi's VM is log-structured and
  may grow into erased sectors after it, so "erased now" is not proof of
  "free". Searches for `0xDA000`/`0x24000`/`0xFE000` immediates hit only
  coefficient and bitmap tables. The VM init (code using the `55AAAA55`
  magic near app+`0x62378`) is the next place to read for its length.

## 2026-11-09 Settings VM: two banks described in RAM

- The VM code near app+`0x062376` (loads magic `0x55AAAA55`) keeps its
  state at RAM `0x11AD0+0x2398` = `0x13E68`: two 12-byte bank descriptors
  (bank A at `0x13E68`, bank B at `0x13E74`). The bank is selected by bit 1
  of byte `0x13ED5`; word 0 of a descriptor is the bank's flash address.
  App+`0x062408` copies the live bank into the other (defragmentation) and
  checks a second magic `0xDDEEBB77`.
- No static writer of the descriptors was found in app+`0x61000–0x64000`. The
  addresses are probably computed at run time (possibly from boot
  information the loader passes). Reading `0x13E68–0x13E7F` on a running
  radio with the RAMREAD research build (`s2-ram`) would give both banks'
  addresses and sizes directly.
- Why it matters even for a Doom-only build: `app_core` and Bluetooth keep
  running and own the VM; a VM format or defragmentation erases its whole
  allotted area. Code placed in erased sectors must sit outside both banks,
  or the VM length must be patched smaller.

## 2026-11-09 RAM probe on hardware: VM banks, boot info, idle heap

- Flashed `s2-ram` via hold-8 (backup `h9-before-s2-ram-20261109.bin` =
  `83f79fb3…`; readback `ba05f7c8…`). The user ran `prototypes/ram-probe`
  over Bluetooth on the running radio: heap readings and two full 192 KiB RAM
  dumps (about 33 s each), all consistency checks true. This is the first
  hardware run of the RAMREAD service; it worked. Dumps are private (in the
  user's Downloads) and not committed.
- **Settings VM banks** (`0x13E68`, `0x13E74`; identical in both dumps):
  bank A flash `0xDA000`, length `0x8000`; bank B flash `0xE2000`, length
  `0x8000`. The VM uses `0xDA000–0xE9FFF` (64 KiB).
- **Boot info** at RAM `0xE2A0` (passed in at boot): `0xDA000`, then length
  `0x22000` (so a reserved area `0xDA000–0xFBFFF`, 136 KiB), app start
  `0x5000`, map base `0x01E00000`, `key_mac` `0xFF000`, chip key `F181`. The
  VM uses only its first 64 KiB.
- RAM at `0x11980` holds XIP pointers `0x01EF8000`/`0x01EF8200`, i.e. flash
  `0xFD000`/`0xFD200` (app offset + `0x5100`): the 10-byte record at
  `0xFD000` is read at run time. Keep out.
- **Free flash, by evidence:**
  - `0xEA000–0xFBFFF` (72 KiB): erased, outside both VM banks, but inside
    the boot info's reserved length. Unknown whether a VM format erases the
    whole reserved length.
  - `0xFC000–0xFCFFF` (4 KiB): erased, outside everything seen.
- **Idle heap:** 276–288 of 527 blocks used; about 30–32 KiB free. The
  largest all-zero RAM runs at idle: 12,086 B at `0x19B7A`, 7,422 B at
  `0xE832`, 5,136 B at `0x145E0` (zero at idle, not proven unused).

## 2026-11-09 H9 Toolbox Firmware restored after the RAM probe

- Hold-8 flash. Backup `h9-before-h9tb-after-ram-20261109.bin` = `ba05f7c8…`
  (the RAM-read build, as expected); 8 sectors; readback `83f79fb3…`.

## 2026-11-09 VM init caps the settings area at 64 KiB

- VM init is at app+`0x08C9D4`:
  - It asks the flash driver (ioctl `0x40045602`) for the reserved area:
    start at `[sp+4]`, length at `[sp+12]`. On hardware these are `0xDA000`
    and `0x22000`, matching the boot info at RAM `0xE2A0`.
  - It then clamps the length: `if (len >= 0x10000) len = 0x10000`
    (app+`0x08C9F2`).
  - With the 4 KiB-aligned start, it halves that into the two bank
    descriptors at `0x13E68` (`[+0]` = start, `[+4]` = len/2, then bank B at
    start + len/2). This matches the hardware readout: 2 × `0x8000` at
    `0xDA000`/`0xE2000`.
  - The other path (mode 1) aligns to 512 bytes and caps at 8 KiB.
- All VM reads, writes, defragmentation and format work through those bank
  descriptors (code at app+`0x0621xx–0x062Axx` and `0x08CAxx–0x08CCxx`
  indexes them; the bank is chosen by bit 1 of `0x13ED5`). Nothing in the
  VM code addresses the remaining reserved length.
- Conclusion: `0xEA000–0xFBFFF` (72 KiB, 18 sectors) is outside the VM as
  stock computes it. `0xFC000` (4 KiB) is outside the reserved area entirely.
- Residual risks, not yet checked:
  - Other users of the boot-info record (9 more code sites load RAM
    `0xE2A0`) might use the reserved tail.
  - The official JieLi updater may erase the whole reserved area. Our
    flasher writes only up to `0xDA000`.
- Writing there needs a flasher mode for sectors beyond the app image.

## 2026-11-09 Reserved-area users checked: only the VM writes there

- RAM `0xE2A0` is a boot record (the stack canary `5A5A…` ends at `0xE290`,
  the startup stack top). App+`0x02D834` copies boot arguments into it (`+0`,
  `+16..+39`). A copy also sits in the flash driver's state at `0x125A8`,
  next to the NOR size (`0x100000`) and JEDEC ID (`0x856014`).
- The ten code sites that load `0xE2A0` read only these fields: `+0` byte
  (app+`0x2C8A4`), `+16/+20` app start and map base (`0x252C8`), `+28` chip
  key (`0x9702`, `0x903D4`, `0x90688`), `+30` calibration halfword used in
  float maths (`0x4AB80`, `0x60C30`), `+32..+38` six bytes plus a check
  (`0x2C440`), plus the writer. None reads `+4/+12` (reserved start and
  length).
- The reserved range reaches code only through flash-driver ioctl
  `0x40045602`, requested in two places:
  - VM init at app+`0x08C9E6`: clamps to 64 KiB.
  - App+`0x04F31C`: computes 128 KiB-aligned bounds (`+0x1FFFF`,
    `& ~0x1FFFF`, `0x20000 - …`) around the reserved range and the code
    range. This is arithmetic only, with no flash writes in this part; it is
    probably the NOR write-protection boundary setup.
- Conclusion: no stock code writes or erases `0xEA000–0xFBFFF`.
- Remaining unknown: what the official JieLi updater does there.

## 2026-11-09 Far-hello built (offline): C from flash 0xEA000

- `h9_flasher.py write-far`: sector-aligned writes inside `0xEA000–0xFBFFF`
  only, with a backup of the old bytes, skipped identical sectors and
  per-sector plus full readback. 3 offline tests (range refusals, untouched
  neighbours, no rewrite of identical sectors); 18/18 pass (8 skipped,
  needing stock files).
- The flash mapping is `CPU = 0x01E00000 + (flash - 0x5000)` (from boot
  record map base `0x01E00000`, app start `0x5000`, and XIP pointer
  `0x01EF8000` ↔ `0xFD000`), so `0xEA000` runs at `0x01EE5000`.
- App-area cipher confirmed against the dump: block key
  `0xF181 ^ ((flash - 0x5000) >> 2)` decodes flash `0x8D280` to app.bin's
  `my_app` bytes (base `0x5100` does not).
- Open question for hardware: whether the mapping decrypts beyond the app
  image too. RAM holds no copy of the `0xFD000` record (raw or decrypted), so
  the dumps cannot answer it.
- `far-hello/`:
  - Far blob: 1,696 bytes, the `H9X1` header plus C-hello from the
    post-controller-setup entry (`0x01EE5062`), encrypted per the hypothesis.
  - H9 Toolbox Firmware gets a 24-byte stub at app+`0xA7E80` (spare
    `.tables` tail), and the Back-hold launcher jump at app+`0xA9DD2`
    retargets from `0x01EA7942` to it. The stub falls back to Brick Breaker
    if the magic does not read back.
  - Package `e83f1140…`, flash `5af9bdfe…`; reproducible; flasher inspect
    verified. Changed sectors vs H9 Toolbox: `0x5000`, `0xAC000`,
    `0xAE000`, `0xD9000`.

## 2026-11-09 Far-hello flashed

- `write-far`: backup of `0xEA000–0xEAFFF` was all `0xFF` (`f47a8ec3…`); 1
  sector written; verified blob `fd38f226…`.
- Main: backup `h9-before-far-hello-20261109.bin` = `83f79fb3…` (H9 Toolbox);
  4 sectors updated (as predicted); readback `5af9bdfe…`. Behaviour on
  Back-hold: pending.

## 2026-11-09 Far-hello run 1: Brick Breaker (encrypted blob not visible)

- With the encrypted blob at `0xEA000`, holding Back started Brick Breaker:
  the stub's magic check failed, and the radio did not crash (fallback
  worked as designed).
- So the CPU mapping does not show `0xEA000` decrypted with the app-area
  stream. The likely reason: JieLi leaves the reserved area (`0xDA000`
  onward) unencrypted. The VM and the `0xFD000` record are stored in plain
  form, and `0xFD000` is read through the mapping (`0x01EF8000`).
- Next: write the same program unencrypted
  (`h9_far_hello_0xEA000_unencrypted.bin`, `504c3661…`). The main firmware is
  unchanged (package `e83f1140…`). If Brick Breaker appears again, the
  mapping does not cover `0xEA000` as assumed.

## 2026-11-09 Far-hello run 2: C runs from main flash 0xEA000

- `write-far` replaced the encrypted copy with the unencrypted blob
  (`504c3661…`; backup of the encrypted sector `dbced1ad…`). Holding Back
  on H9 Toolbox Firmware + stub now shows **C-hello**: the stub read `H9X1`
  through the mapping and jumped to `0x01EE5008`, and the program ran.
- Established:
  - Flash `0xEA000` is mapped at `0x01EE5000` (`0x01E00000 + flash - 0x5000`).
  - **The mapping does not decrypt there:** code and data in the reserved
    area are stored unencrypted, unlike the app area.
  - Code launched from the running radio (after the full stock boot) can
    live entirely in the far region; only the 24-byte stub is in the app.
- Pending: the C-hello readouts in this mode, and whether Bluetooth (the
  resource service) still answers while it runs.

- Far-hello readout: LOOP 172,022 per tick, against 199,873 for C-hello
  replacing `my_app` at boot: 14% fewer iterations. The 5-instruction loop
  fits in cache either way. The likely cause is that the whole stock system
  (`app_core`, `btstack`, audio and timer interrupts) now runs alongside, not
  the far-flash location. Not yet separated (an A/B run of the same build
  from the app cave would settle it).

- Second far-hello launch: FILL 2, SQ 0, LOOP 150,163, DLY 10. Display and
  delay match the takeover run. LOOP varies between launches of the same
  code at the same address (172,022 → 150,163, −13%). That supports
  variable background load from the running stock system over a cost of the
  far location. Planning figure: roughly 75–86% of the CPU is available
  while the radio runs underneath.

- **Bluetooth works while the far program runs:** with C-hello on screen
  (started from the far region), the user's Bluetooth read from h9toolbox.com
  completed. The resource service (and `btstack`) keep running alongside
  code launched this way, so assets can go to the 16 MiB chip while our
  program is active. Read duration was not recorded.

## 2026-11-09 Far logo written: C reads the resource flash (pending)

- `far-hello/logo.c` (`build.py --program logo`, 1,556 bytes): streams the
  boot logo from resource partition 5, offset 0, through `h9_res_read` (a
  wrapper for stock `0x01E52CB2(sel, offset, buf, len)`, `r0..r3`) straight
  to the LCD in 128-byte chunks, then times reads of the 40,960 bytes in
  128- and 512-byte calls and compares byte sums.
- The host preview with the user's logo (from the PNG, converted to RGB565
  big-endian) renders correctly.
- The main firmware is unchanged (package `e83f1140…`, already on the
  radio). `write-far` replaced the far sector (backup = run-2 hello blob,
  `e5e8929a…`); verified `4bb075d7…`. Behaviour: pending.

- **Far logo on hardware: C reads the resource flash correctly.** Holding
  Back drew the user's logo from the 16 MiB chip; readouts SHOW 4, RD128 2,
  RD512 2, SUM 5977, identical after a reboot. The byte sum of
  `0xA00000–0xA09FFF` in the user's 16 MiB dump
  (`td-h9-resource-chip-0x0-2026-11-09.bin`) is 3,872,601 = `5977` mod
  65536: an exact match, and both plain reads agreed with the streamed one.
- Speeds (10 ms ticks, ±1 tick):
  - read: 40,960 B in about 20 ms (10–30 ms), roughly 1.4–4 MB/s, and the
    same for 128- and 512-byte calls (per-call overhead is small)
  - read and draw a full screen: about 40 ms (30–50 ms), roughly 20–33
    full frames/s streamed straight from the resource flash

## 2026-11-09 Doom size check (offline): unmodified doomgeneric does not fit

- doomgeneric `e257a09` (GPL-2.0, downloaded 3.1 MB tarball, kept outside
  the repo) compiles cleanly with JieLi clang `-Oz` (72 files).
- Totals before linking: code 115,130 B, rodata 100,755 B (66,820 B trig
  tables), init data 60,030 B (40,236 B `info.o`), bss 243,693 B (134,152 B
  `r_plane`), plus a 6 MiB zone heap by default.
- Against 72 KiB far flash and about 31 KB free RAM (130 KB more with
  `my_app`'s RAM), it is about 4× over on flash and far over on RAM. A
  fitting port must render at 160×128, read the WAD in place from the
  resource flash, constify tables, and get more main flash. Details in
  `takeover/DOOM-SIZE.md`.

## 2026-11-09 Doom firmware route chosen; resource-chip rules

- The user chose Doom as its own firmware, with two requirements:
  everything written to the resource chip must be restorable, and stock
  must still boot with it modified.
- Erased runs in the full-chip dump (`b6d60e2e…`), each a single
  contiguous block:
  - `resource` `0xA1F000–0xCFFFFF` (2,948 KiB)
  - `font` `0x717000–0x7FFFFF` (932 KiB)
  - `voice` `0x3A7000–0x3FFFFF` (356 KiB)
  - Total 4,236 KiB. `record` (3 MiB, empty) is excluded: stock owns it.
- Rules, restore method and the stock-boot checklist:
  `doom-firmware/README.md`.

## 2026-11-09 General PTT+2 restore to stock (offline)

- PTT+2 is the recovery path for the Doom firmware. It survived C-hello's
  `my_app` replacement; hold-8 did not. Rule: Doom builds take over at the
  `my_app` entry (as C-hello did), and the first check after flashing any new
  build is that PTT+2 still enumerates.
- `hid-recovery/restore_stock.py` restores any main firmware to stock:
  - It scans all 213 app sectors by CRC against the stock restore point
    (`74243eae…`) and rewrites the ones that differ.
  - It always rewrites the sectors any `--known` package changed (against
    CRC collisions). `--all` rewrites everything.
  - It writes highest sector first, with the header sector and its first 32
    bytes last, then rescans and requires all sectors to be stock.
  - It is resumable, and never writes past `0xD9FFF`.
  - It uses the proven HID packets (C2/C3/C4) from the logo and C-hello
    restores.
- 9 offline tests against a simulated NOR radio pass: arbitrary and 384 KiB
  changes, header-last ordering, inspect-only, already-stock, known-sector
  rewrite, `--all`, resume after a simulated USB drop, untouched VM and far
  region, wrong restore point rejected.
- `--known` on the far-launcher package and on H9 Toolbox gives the same 8
  sectors (`0x5000`, `0x62000`, `0x8D000`, `0xAA000`, `0xAC000`, `0xAE000`,
  `0xAF000`, `0xD9000`).

## 2026-11-09 General PTT+2 restore works on hardware

- Radio on H9 Toolbox + far launcher, PTT+2 mode (`4C4A:4155`).
  `restore_stock.py inspect --known <far-launcher .fw>`: 205 stock, 8
  differ, exactly the predicted set.
- `restore`: 8 sectors written highest first (`0x5000` last); rescan: all
  213 app sectors match stock v1.0.38. Boot check: pending.

- Stock booted after the HID restore. The hold-8 backup before reinstalling
  H9 Toolbox hashed `74243eae…` (exact stock), an independent full-image
  confirmation of the HID restore. H9 Toolbox reinstalled (8 sectors,
  readback `83f79fb3…`).
- The full recovery loop is proven on hardware: custom firmware → PTT+2
  restore → stock → hold-8 → H9 Toolbox.

## 2026-11-09 Freedoom measured: one map is about 2.5–3.1 MB

- Freedoom 0.13.0 (`freedoom-0.13.0.zip`, 24,143,781 B, SHA-256
  `3f9b264f…`, matching the signed CHECKSUM file), kept outside the repo.
  `freedoom1.wad` is 28,795,076 B (3,163 lumps): levels 10.6 MB, wall
  patches 10.1 MB, sound and music 3.0 MB, sprites 2.5 MB, other graphics
  1.6 MB, flats 1.0 MB.
- `doom-firmware/wad_tool.py map` counts what a map needs to start and play:
  - its level lumps
  - textures (plus switch and animation partners and the sky) and their
    patches
  - flats (plus animation ranges)
  - sprites of its things, with projectiles, drops and weapon HUD sprites
  - in-game graphics only (status bar, menus, fonts, palette, colormap)
  - no title, help, intermission, ending, sound or music
- Results:
  - E1M1: 2,624,451 B
  - E1M2: 3,144,756 B
  - E1M3: 2,464,202 B
  - all of episode 1: 7,844,897 B
- Against 2,948 KiB contiguous in `resource` (4,236 KiB stock-safe in
  total): **one map fits** (E1M1 or E1M3 in `resource` alone); a full
  episode does not. Wall patches are the largest part (about 1.0–1.4 MB per
  map).

## 2026-11-09 DMC100-Doom converter: all of episode 1 in 3.71 MiB

- Downloaded DMC100-Doom (`0cdc78e`, 739 KB tarball, SHA-256
  `5328fbdb…`, GPL-2.0-or-later), outside the repo. Its `oracle/fetch.sh`
  cloned doomgeneric `dcb7a8d` (verified).
- Its converter on our Freedoom zip (same SHA-256 it pins):
  - E1M1 pack: 2,802,000 B
  - E1 pack: 3,890,048 B (`DOOM.DAT` 3,031,040 + `DOOMX.DAT` 859,072)
  - Generated tables are compressed (`finesine` 4,302 B against 40 KB raw);
    info tables are about 4 KB.
- On macOS, Mach-O's leading underscore hid the `A_*` action symbols from
  its `nm` regex. An `llvm-nm` wrapper that strips it fixed this with no
  source change. `test_maps` (E1M1, E1), `test_trig` and
  `test_determinism` pass.
- E1 (3,799 KiB) fits in the `resource` and `font` tails (3,880 KiB). The
  pack must be split across the two ranges.

## 2026-11-09 E1M1 with E1 graphics settings: 1.01 MiB

- Two extra pack entries in DMC100-Doom's `tools/conv/pack.py`, kept as
  `doom-firmware/patches/dmc100-e1m1h-pack.patch`:
  - `E1M1H`: the E1 settings (`base` 1 = half-resolution base textures,
    `mips` 1, `xshift` 1 = half-width UI, no HELP1/CREDIT, half-width
    TITLEPIC/WIMAP0) with `greedy` upgrades on.
  - `E1M1S`: the same with `greedy` off.
- Sizes:
  - E1M1H: 2,263,744 B. Greedy re-upgraded 113 textures to full resolution
    to fill DMC100's 3,031,040-byte drive, which is its capacity, not ours.
  - **E1M1S: 1,056,192 B** (textures 346,736, patches 505,280, map
    120,592).
- Both pass `test_maps`; `test_determinism` still passes.
- E1M1S is the first-upload candidate: it fits easily in the `resource` tail
  (2,948 KiB), at about 9–12 min estimated upload. Visual quality on the
  160×128 panel is still to be judged.

## 2026-11-09 DMC100 engine on Pi32v2: 112 KB core

- DMC100's size-gate set, built with JieLi clang and linked with
  `--gc-sections`: code 80,036 B, rodata 31,842 B, data 240 B, bss
  13,396 B.
- Before gc, Pi32v2 is 119,415 B against Cortex-M4's 126,403 B (5.5%
  smaller).
- A complete port (with front end and platform) is estimated at 140–170 KB.
- Free main flash outside the app today: 76 KiB (`0xEA000–0xFCFFF`). The
  Doom firmware therefore needs roughly 70–100 KB more. Options: DMC100-style
  code overlays loaded from the resource chip into RAM, reclaiming stock
  code and assets only `my_app` uses, or a from-scratch image.

## 2026-11-09 Data uploader (offline)

- `prototypes/data-upload/` (page, plus `upload.mjs`): Bluetooth upload of
  a data pack through the S2 write service, under the doom-firmware rules.
  - **Layout:** a directory sector at `0xA1F000`, the pack from `0xA20000`,
    spilling into the font tail and then the voice tail.
  - **Directory:** magic `H9DATA01`, length, SHA-256, name, segments,
    CRC-16. It is written first with its last 32-byte block erased (in
    progress, claiming the sectors). After the pack is read back and
    matches, that block is programmed with `H9DATA-COMPLETE` (no erase
    needed). Readers must require it.
  - **Safety rules:**
    - Only the gold-dump erased tails can be written; anything else is
      refused.
    - Non-erased data is overwritten only when a matching in-progress or
      complete directory owns it.
    - A different pack first restores (erases) the previous one.
    - Already-erased sectors are programmed without an erase.
  - **Reads** start at the 32 KiB bank boundary so bulk reads work.
  - **Restore** erases the ranges the directory lists (or, deep, every
    non-erased sector of the three tails) and reads them back as `0xFF`.
  - **Timing:** write seconds and bytes per second go into the upload record
    (the first real measurement of Bluetooth write speed).
- `test/data-upload.test.mjs`: 8 tests on the simulated radio:
  - layout, directory round-trip and corruption
  - refusals outside the areas and of foreign data
  - upload with stock data untouched and no erases on blank sectors
  - idempotent re-upload and replacement
  - interrupted upload resumes, rewriting only the failed sector and the
    one after it
  - deep restore clears stray data
  - restore returns the chip byte for byte
- Full suite: 93/94 pass. The failure is the known, pre-existing
  browser-readback seed test on `main`.
- Pack staged for the user (outside the repo):
  `~/Documents/ChatGPT/TID-H9/doom-packs/freedoom-E1M1S-DOOM.DAT`
  (1,056,192 B, SHA-256 `06442c61…`).

## 2026-11-09 Flash for Doom: a 62 KB my_app-only block

- `takeover/callgraph.py`: whole-image call graph (3,507 functions, 8,227
  direct calls, iterated call-target discovery). `my_app`-only code totals
  69,136 B, mostly the contiguous block app+`0x79CC6–0x890E4` (~62 KB).
- **No real references in from elsewhere:** the only outside word pointing
  into the block that is a real reference is the task table's `my_app`
  entry. The rest are clock constants (`0x01E84800` = 32 MHz), odd values
  and table/instruction bytes.
- **Needed helpers sit outside it:** every stock helper the C programs need
  is outside the block.
- **Budget:** 76 KiB far region + ~62 KB block = ~138 KB against a 112 KB
  engine core plus ~10 KB platform. The front end goes to overlays.
- Next proof: a trap test of the block with `my_app` replaced (the Doom
  firmware condition).

## 2026-11-09 First real Bluetooth write speed (E1M1S upload)

- The data-upload page, E1M1S pack (1,056,192 B, 258 data sectors plus the
  directory). The live rate first showed 0.02 KiB/s: a display bug (bytes
  were added only when a region finished), fixed in `6e12a9e`.
- Measured by the sector counter: 61 → 74 of 259 in 60 s = 13 sectors/min
  = **about 0.87 KiB/s** (~4.6 s per 4 KiB sector: 128 acknowledged
  32-byte block writes plus 8 status reads on the 15 ms link). That is about
  half the earlier 1.5–2 KiB/s estimate.
- Upload times at this rate: E1M1S about 20 min of writing; E1 (3.71 MiB)
  about 73 min.

## 2026-11-09 E1M1S uploaded and verified on hardware

- Upload record:
  - `freedoom E1M1S`, 1,056,192 B, SHA-256 `06442c61…` (matches the pack)
  - one segment, `resource` at `0xA20000`; directory at `0xA1F000`
  - 259 sectors (directory plus 258 data sectors; no erases needed),
    1,060,864 B programmed in 1,352 s = **785 B/s (0.77 KiB/s)**
  - 1,708 s (28.5 min) in all
  - verified, marked complete
- Phases:
  - pre-check read of 1,060,864 B: 10:59:50 → 11:03:06 (about 3 min 15 s
    including connect and link changes)
  - writing: 11:03:06 → 11:25:39
  - read-back: 11:25:39 → 11:28:30 (about 2 min 50 s, about 6.2 KiB/s)
  - COMPLETE block: 11:28:31–11:28:39
- Stock-firmware check with the data present: pending.

- **Stock check with the pack present:** after a power cycle, H9 Toolbox
  booted with the KD9CRQ logo, the voice prompts played, the menus were
  normal, and a Bluetooth programmer read worked. The resource-chip rule
  "stock still boots and works" holds on hardware.

## 2026-11-09 Free block refined; trap-test safety question

- `callgraph.py --needed` subtracts everything the replacement startup
  calls. C-hello's stock calls are taken from `entry.s`, `logo.c` and
  `far-hello/build.py`.
  - 39 functions (18,574 B) are needed. Four of them were `my_app`-only:
    `0x789EA`, `0x79BE4` (pin helper), `0x79C6E`, `0x79CC6` (backlight PWM
    setup, called at app+`0x8841E` by stock and by C-hello).
- **Free block now two runs:** app+`0x79E32–0x7D64E` (14,364 B) and
  `0x7D69E–0x890E4` (47,686 B), about 62 KB. The 80 B between them is used
  elsewhere.
  - `my_app`'s entry `0x88180` lies in the second run. The Doom firmware
    should redirect the task-table entry (immediate `0x01E88280` at
    app+`0x5FFD0`) instead of hooking there.
- **Safety question before trapping the block:**
  - With `my_app` replaced, hold-8 is gone, so PTT+2 is the only recovery.
  - PTT+2 worked under C-hello, so its code is not `my_app`-only on
    hardware. A static misclassification (code reached only through a jump
    table) could still put part of it inside the block, and a trap there
    would leave no USB recovery.
- **Hold-8 entry:** `my_app` startup (app+`0x88180–0x88900`) shows no
  direct key-decoder call. It calls the delay, pin helper, controller
  setup, backlight and four partition reads, then reaches the key handler
  (`0x524A0`) only in the main loop (`0x8890A` …). The hold-8 path is not
  located yet.

## 2026-11-09 Hold-8 path found (static): ROM 0x02000D68

- The user asked about PTT+3. Earlier notes show PTT+1 (serial), PTT+2
  (HID) and PTT+3 (USB serial) all give the same green-LED, black-screen
  updater, so PTT+3 is not independent of PTT+2. Hold-8 (mask-ROM USB
  loader `BR23UBOOT1.00`) is the independent path, so we locate it.
- Of the 125 ROM functions called by the app, only `0x02000D68` is called
  exclusively from the `my_app` block (sites app+`0x7CF24`, `0x7D590`).
  Both sites draw green text (colour `0x07E0`), wait 1000 ms, call
  `0x02000D68`, then `goto -2`.
- The boot-key handler at app+`0x7D418`:
  - It reads settings item 1 (`call -89110`, 8 bytes) and checks the
    marker `TIDE9` (84 73 68 69 57).
  - If the marker is missing, it draws a red string and polls the key byte
    `0xC0F0+703` = **`0xC3AF`** (the byte `h9_key()` returns) and
    `0xC0F0+685` = `0xC39D`.
  - Key byte **5** leads to app+`0x7D54E`: it clears the screen, draws two
    green lines (`USB Update` / `Connect USB` at `0x95C2F`/`0x95C3A` are
    the likely strings), waits 1000 ms, calls ROM `0x02000D68`, and never
    returns.
  - Key 8 with `0xC39D` = 14 writes `TIDE9` to the settings and continues.
  - At app+`0x7D3FA`, `0xC39D` = 14 with key byte 5 calls `0x7CE14` (the
    other ROM `0x02000D68` site).
- Hypothesis: key byte 5 is the "8" key, and ROM `0x02000D68` enters the
  mask-ROM USB loader (`4C4A:2342`). The next test checks both from a far
  program launched on the running radio, where stock hold-8 is unaffected.

## 2026-11-09 Our code enters the USB loader: ROM 0x02000D68 proven

- Far-keys program (`84be685d…` at `0xEA000`) with H9 Toolbox and the far
  launcher (`5af9bdfe…`; 4 sectors from H9 Toolbox).
- **Key byte 5 is the 3 button, not 8.** With 3 held for 2 s, the program
  called ROM `0x02000D68`. The Mac then enumerated `4C4A:2342`
  "BR23UBOOT1.00", and `h9_flasher.py probe` answered
  `BR23 UBOOT1.00 1.00`: the same mask-ROM USB loader hold-8 gives.
- So any firmware of ours can enter the USB loader by itself (one ROM call,
  no stock `my_app`). Every Doom build gets an independent recovery path
  besides PTT+2.
- Open: stock's handler fires on key byte 5 (button 3) but the user enters
  this mode by holding 8. Either the boot-time key byte differs from the
  runtime mapping, or hold-8 reaches `0x7CE14` through the `0xC39D` = 14
  branch. It does not matter for our own trigger, which we choose.

## 2026-11-09 Trap test built (offline)

- `trap-test/build.py` builds the Doom-firmware condition on stock
  v1.0.38:
  - The `my_app` task entry immediate at app+`0x5FFD0` (`0x01E88280`) is
    pointed at `0x01EE5008`, the far header's jump to the full C-hello
    startup.
  - The two `my_app`-only runs are filled with traps:
    - Every 1 KiB, a 14-byte stub: `r0 = own address; r3 = far_trap; goto
      r3`.
    - Every other halfword is a 2-byte `goto` to the next stub.
    - 62 stubs in all. Every halfword was decode-checked to reach a stub.
  - `far_trap` writes `TRAP` and the stub address to RAM `0x1E4F0` and calls
    ROM `0x02000D68`, so a trap puts the radio in the USB loader.
- `monitor.c` (1,542 B at `0xEA000`):
  - The 3 key held at power-on (checked for 0.3 s) enters the USB loader:
    recovery without PTT+2.
  - It shows the previous trap record if RAM kept one, then uptime and a
    heartbeat. Holding 3 for 2 s also enters the USB loader.
- Package `a5541dc1…`, flash `f70b4400…`; reproducible; flasher inspect
  verified.
- 20 sectors changed: `0x5000`, `0x65000`, `0x7E000–0x8E000`, `0xD9000`.

## 2026-11-09 Trap test flashed

- The monitor was written with `write-far` through the USB loader that our
  own ROM call had entered (`eda71511…`; backup = the keys program). The
  ROM-call path gives a full flashing session (RAM helper, read, erase,
  write), not only enumeration.
- Main: hold-8 from H9 Toolbox + far launcher (backup `5af9bdfe…`); 25
  sectors (undoing H9 Toolbox and applying the trap image); readback
  `f70b4400…`. Run results: pending.

## 2026-11-09 Trap test run 1: black screen, no LED; recovered by PTT+2

- Power-on gave a black screen with no LED. The monitor turns the LED on
  first, so it never ran (or something trapped before it started). No
  `4C4A:2342` appeared, and holding 3 at power-on did nothing (that check
  is in the monitor).
- **PTT+2 still worked with this build:** `restore_stock.py inspect` found
  exactly the 20 trap-build sectors, and `restore` returned all 213 to
  stock.
- Next diagnostic: `trap-test/read_record.py` reads RAM `0x1E4F0` through
  the mask-ROM loader (hold-8 from stock) to see whether a trap stub
  recorded itself.
- Hypotheses:
  1. the task launcher rejects or mishandles an entry outside the app image
     (`0x01EE5008`)
  2. code before the `my_app` task (`app_core` or initializers) reaches the
     block, so a trap fired where the ROM USB-loader call could not run
- Discriminating builds: the redirect without traps, and traps with a
  trampoline at `my_app`'s own entry (`0x88180`) instead of the redirect.

## 2026-11-09 Trap test run 2 (trampoline + traps): also black

- `my_app`'s own entry jumped to the far monitor (`r3 = 0x01EE5008; goto
  r3` at app+`0x88180`, the task table untouched), with the rest of the
  block trapped. Package `70f054a2…`, readback `9ab18126…`, 24 sectors.
- Power-on: black screen, no LED. The Mac then showed `4C4A:4155`;
  whether the user had entered PTT+2 is unconfirmed. HID inspect found
  exactly 19 trampoline-build sectors; restore returned all 213 to stock.
- **The cause is not the task-table redirect.** What both runs share,
  unlike C-hello (which ran): the startup code runs from the far region at
  boot, and the block is trapped.
- **Diagnostics were ambiguous:**
  - The trap handler did not light anything.
  - The monitor drew only after its key scan, and C-hello's LED setup
    leaves the LED off.
  - So "black, no LED" could not separate "never ran" from "trapped".
- **Fixed:**
  - The monitor (`6660f504…`) turns the LED on and paints the screen blue
    first.
  - `far_trap` lights the LED solid before calling the USB loader.
  - New mode `trampoline-only` (no traps, package `110bfe1f…`) tests
    whether the far monitor starts at boot at all.

## 2026-11-09 Run 3 (trampoline-only, no traps): black — far code cannot start at boot

- New monitor `6660f504…` (LED on and blue screen first). Firmware
  `trampoline-only` (`110bfe1f…`, readback `131f0f78…`, 3 sectors from
  stock): `my_app`'s entry jumps to `0x01EE5008`, with no traps anywhere.
- Power-on: black, no LED. **The traps were not the cause.** Code in the
  far region does not run when started at `my_app` entry, although it ran
  when launched later from the running radio (far-hello).
- Combined evidence:
  - plain far code fails at boot
  - plain far code works late
  - encrypted far code failed late (far-hello run 1)
- Hypothesis: the mapping decrypts `0xEA000` like the app area until
  something during normal operation (possibly `my_app`) switches the
  reserved range to plain. App+`0x4F31C` is only an argument wrapper used
  by the VM write path (callers in `0x62xxx`, `0x8CBxx`), not that switch.
- Test: the same firmware with the monitor stored encrypted
  (`h9_trap_monitor_0xEA000_encrypted.bin`, `1738454f…`).
  - If it shows the blue screen, encrypted far code is the right form for
    the Doom firmware, where `my_app` never runs.
  - If it starts and then freezes, the switch happens elsewhere at run
    time.

## 2026-11-09 Probe: data reads of the far region are plain at boot

- `c-hello --source probe.c` (cave at `my_app` entry, package `a6fe83ae…`,
  readback `660c7e26…`) reads through the mapping, every second, with the
  encrypted monitor in the far region.
- Readings: F0 `5A7D3BC9`, F1 `9158ACD6`, F2 `ED5ED8C0` (exactly the
  **raw stored** bytes), A1 `6C15EA81` (correct app word), stable from
  UP 9 to 21 s (photos).
- So data reads of `0xEA000` are plain from boot, and nothing switches
  them during that time.
- Yet the plain monitor did not start at boot (run 3). The remaining
  hypothesis: **instruction fetches** from the far region are decrypted at
  boot while data reads are plain.
  - Plain code then decrypts to garbage.
  - Encrypted code runs, but its tables (font, panel init) read as garbage
    data; a garbage init table can hang startup before the LED code.
  - Late in boot (far-hello), plain code executed, so the instruction path
    must change at some point.
- Next: probe 2 calls a constant-returning far function stored plain
  (`0xEA100`, returns `11111111`) or encrypted (`0xEA200`, returns
  `22222222`), chosen by key, one per boot.

## 2026-11-09 Probe 2: plain far code executes at boot; encrypted hangs

- Probe 2 (`a2187336…`, startup in the app cave at `my_app` entry), far
  blob `c6f86059…`.
- **Up** (plain function at `0x01EE5100`): `RT 11111111`.
- **Down** (encrypted function at `0x01EE5200`): froze at "CALL ENC".
- The "I" missing from "PLAIN" on screen is a font omission in probe2.c,
  cosmetic only.
- **Conclusion:** the far region is plain for instruction fetches too, and
  code there runs at boot when called after a startup that ran in the app
  image.
- What failed in runs 1–3 is **starting** in the far region: `my_app`'s
  entry jumping straight there, with the C-hello startup itself executing
  from `0xEA000`. Cause unidentified.
- **Architecture for the Doom firmware and the next trap test:**
  - Startup (C-hello stage-23 code, the USB-recovery key check) in the app
    cave at `0xA77F0`, entered by the proven `call` hook at `my_app`'s
    entry.
  - Everything else in the far region, called after startup.

## 2026-11-09 Trap test in the cave architecture (built)

- `trap-test/build.py --mode cave+traps`:
  - The 4-byte `call` hook at `my_app` entry (as C-hello) leads to the
    startup (C-hello `entry.s`, section renamed `.cave`) in the app cave:
    716 B at `0x01EA78F0`.
  - The startup calls the far monitor `h9_main` (`0x01EE51CA`).
  - The monitor (868 B, `243cc128…`) and `far_trap` (`0x01EE501A`) are in
    the far region, stored plain.
  - The block is trapped except the hook bytes (15 + 43 + 4 stubs).
  - Package `07225154…`.
- `--mode cave-only` (`b9638439…`) is the same without traps, for
  separating causes if needed.
- Checked by decode: hook → cave, cave → far `h9_main`, stubs →
  `far_trap`.

## 2026-11-09 Cave trap test boots: my_app replaced, 62 KB block trapped

- Far monitor `243cc128…`, firmware `cave+traps` (readback `d8afb645…`,
  20 sectors from stock).
- Power-on: "TRAP TEST" and "NO TRAP SEEN" on black (the "N"s are missing:
  the monitor's font has no N; cosmetic), with the LED blinking each second.
- The startup in the cave, the far-region monitor, and stock's remaining
  tasks booted with the whole `my_app`-only block trapped: no code reached
  it during boot. The run-time soak is in progress.

- **Soak passed:** at 713 s of uptime, with keys pressed and USB plugged
  and unplugged during the run, it showed "NO TRAP SEEN". The LED never went
  solid, there was no freeze or reset, and the USB loader never appeared on
  its own.
- **Proven on hardware:** with `my_app` replaced, nothing executes in
  app+`0x79E32–0x7D64E` (14,364 B) or `0x7D69E–0x890E4` minus the 4-byte
  hook (47,682 B).
- **Flash available to the Doom firmware:** ~62 KB in place of the radio UI
  code, plus the far region `0xEA000–0xFCFFF` (76 KiB), plus the app cave
  for the startup.
- Caveats:
  - Coverage is what ran in 12 minutes (boot, idle, keys, USB plug). Rare
    stock paths (low battery, charging edge cases) were not exercised.
  - The trap stubs stay in the Doom build's safety design only until Doom
    code replaces them.

## 2026-11-09 Back to H9 Toolbox through our own USB trigger

- Holding 3 on the cave trap monitor entered the USB loader (`4C4A:2342`
  "BR23UBOOT1.00"). From there `h9_flasher.py flash` reinstalled H9 Toolbox
  with the far launcher. The backup was the cave trap image (`d8afb645…`),
  24 sectors were written, and the readback is `5af9bdfe…`.
- The cave-hosted recovery trigger works for a full reflash with
  `my_app` replaced. No PTT+2 and no stock hold-8 were needed. The far
  region still holds the trap monitor (harmless on H9 Toolbox).

## 2026-11-09 E1M1S pack renders in DMC100's host engine

- DMC100's host engine (`build/host/dmcdoom`) needs GNU ld `--wrap`, which
  Apple's linker lacks. It is built in a new local image `h9-jieli-dev`
  (`h9-jieli` plus Debian `build-essential`, GCC 12.2; Dockerfile at
  `~/Documents/ChatGPT/TID-H9/docker-h9-dev`). The macOS LLVM wrappers
  (`llvm-nm` without the leading `_`, `llvm-dwarfdump`, `llvm-size`,
  `llvm-objdump`) are now kept in `~/Documents/ChatGPT/TID-H9/mac-llvm-tools`.
- `tests/host/mknor.py build/packs/E1M1S` gives a meter NOR image; the
  image check is ok. `dmcdoom --nor … --low-detail --verify -warp 1 1 --tics
  140 --frame-out …` rendered E1M1 at tics 35/70/105: the start room with
  textures, sprites and the status bar.
- The exact pack on the radio's resource chip (1,056,192 B, `06442c61…`)
  is valid game data for this engine. The port work is now the TD-H9
  platform layer.

## 2026-11-09 First-pass Doom firmware builds (offline)

- `doom-firmware/build.py` with `port/` (TD-H9 platform layer: I_* hooks,
  LCD pump with on-the-fly 320x240 → 160x128 downsampling, NOR/FAT shim
  onto the uploaded pack, entry with hold-3 recovery).
- It compiles DMC100's size-gate set plus `port/` with JieLi clang and
  links with JieLi `ld --gc-sections` in two passes (sizing, then
  object-level packing).
- JieLi `ld` accepts objects from the repo's Python assembler; cross-region
  calls and absolute relocations resolve.
- No undefined symbols.
- Result:
  - cave 738 B
  - far region 69,634 B code plus 264 B `.data` image
  - R2A 39,186 B, R1 4,790 B
  - RAM core to `0x255C0`, frame 15,208 B, arena 11,736 B
  - 19 sectors changed; flasher inspect verified
  - package `74edf3c4…`, far blob `49bfa937…`
- Checked by decode:
  - hook → cave
  - cave → `td_main` (`0x01E82E22`)
  - heap end `r5 = 0x22000`
  - `.data` loads at the end of the far blob
- `test/test_pack_find.c`: the directory reader accepts the uploader's
  E1M1S directory (pack `0xA20000`, 1,056,192 B) and rejects erased,
  in-progress and corrupted ones.
- Not flashed. Open runtime questions:
  - stock heap pressure with the heap 40 KB smaller
  - `my_app` task stack depth
  - frame rate
  - key codes other than 3

## 2026-11-09 First-pass Doom firmware flashed

- `write-far` of `h9_doom_far_0xEA000.bin`: 18 sectors `0xEA000–0xFBFFF`
  (backup `99db53dc…`: the trap monitor plus erased sectors), verified
  `49bfa937…`.
- Main: hold-8 from H9 Toolbox + far launcher (backup `5af9bdfe…`), 23
  sectors, readback `702d8eb6…`. Run result: pending.

## 2026-11-09 Doom run 1: "NO PACK" — resource flash not set up; fixed

- The first-pass firmware booted to its status screen and stopped at "NO
  PACK": the directory at `0xA1F000` did not read back.
- Cause: the stock partition read (`0x01E52CB2`) requires the ready marker
  `0x5AA5` at RAM `0x12234`, and only stock `my_app`'s start-up sets up the
  chip:
  - GPIO 23 as a high output (app+`0x08849C`)
  - SPI2 via `0x01E4C2DA(2)` (`0x0884BC`)
  - the JEDEC ID through helper app+`0x7A116`, which is `my_app`-only and
    now overwritten by Doom code in R1
  - the marker
- Fix:
  - `td_nor_setup()` sets GPIO 23 (bank 1 bit 7: input enable, output,
    high) and calls `0x01E4C2DA(2)`.
  - `nor_read` now calls the lower-level stock SPI2 read
    `0x01E4FF22(buf, chip_addr, len)` (outside every overwritten region),
    which skips the marker check.
- Rebuilt: package `b67227c6…` (flash `b964ab70…`), far blob `4cbf79bb…`.
  Directory test passes.
- Lesson: the `--needed` set came from C-hello's calls only. The Doom
  build's own stock calls must be added to it (here `0x4FE22`, all
  outside), and stock state that `my_app` initialises needs an explicit
  replacement.

- Run 2 flash. Both sessions were entered through our own recovery: hold 3
  for 2 s on the "NO PACK" screen, then hold 3 at power-on (`td_main`'s
  check). Both gave `4C4A:2342`. Far blob `4cbf79bb…` (13 of 18 sectors
  changed). Main: backup `702d8eb6…` (run-1 image), 17 sectors, readback
  `b964ab70…`.

- **Run 2: the pack reads and passes the engine's checks on hardware.**
  - Status screen: HEAP 167/209 blocks, PACK 1,056,192 B, "PACK OK".
  - `td_nor_setup` plus the stock SPI2 read work without stock `my_app`.
  - `img_init` (DMC100's header CRC and section checks) passed on the real
    chip.
- **Heap:** with the end lowered to `0x22000` the stock heap has 209 blocks
  (26,752 B), 167 used (21,376 B), 42 free (5,376 B), with `my_app` not
  running. The stock tasks fit, with little margin.

- **Run 2 then stayed on "PACK OK":** no frame, no red error, no reset, so
  it hangs or stops inside `D_DoomMain` before the first strip.
- Trace build (`port/td_trace.c`): GNU ld `--wrap` on `V_Init`, `R_Init`,
  `P_Init`, `S_Init`, `D_CheckNetGame`, `HU_Init`, `ST_Init`, `G_InitNew`,
  `P_SetupLevel`, `TryRunTics` and `R_RenderPlayerView` (`PAR_Init` was not
  wrapped).
  - Each stage shows its name and stack address, with a green dot when
    done.
  - Counters (NOR reads R, I_GetTime calls T, I_StartTic calls S) refresh
    every 0.5 s until the first strip.
  - JieLi clang 4.0.1 segfaults on `__builtin_frame_address`, so the stack
    address comes from a local's address.
  - Package `1b69ecc1…`, far blob `5bca4550…`; wraps confirmed in
    `D_DoomMain`'s disassembly.

## 2026-11-09 Doom hang found and fixed; E1M1 runs in the host test

- Trace run on hardware: bottom line R1 T0 S0 and no stage names, so it
  stopped in `D_DoomMain` before `V_Init`. The only call there is
  `Z_Init()`.
- **Cause:** `I_ZoneBase` (copied from DMC100's size-only stub) returned
  NULL. `Z_Init` writes the zone header at the returned base, so it wrote
  over the RAM-resident code at address 0. (DMC100's device firmware
  replaces `z_zone.c`; its host build, whose path this build follows
  because `DMC_FW` is not defined, uses a real zone.) Fixed with a 1 KiB
  static zone; only the sound channels are allocated.
- **Host test** (`test/host_test.py`, `test/td_host.c`): the exact firmware
  source set compiled with gcc in `h9-jieli-dev` (`-DTD_HOST`).
  - Stand-ins: LCD as a 160x128 framebuffer with window checks, the
    resource flash as a 16 MiB image holding the uploader's directory plus
    the E1M1S pack, scripted keys.
  - RAM symbols are set on host arrays with `.set`.
  - Harness traps: the engine's stub headers hid `fopen`, so the harness
    compiles separately; DMC100's libc stubs replace glibc's `free` on the
    host, so they are left out there.
- Under AddressSanitizer it ran 175 tics with no errors and rendered E1M1
  at 160x128 with the status bar; holding Up walks forward
  (`host-evidence/e1m1-host-final.png`).
- Two further fixes found by it:
  - `M_Responder` forwarded keys to DMC100's menu, which opens on any
    button.
  - `M_Drawer` always drew the menu.
  - With no menu in this pass, both are now no-ops.
- `PAR_Init` exists only in DMC100's host build, so its wrap was removed.
- Rebuilt: package `1789f4a3…` (flash `cb01527b…`), far blob `8fbb46ed…`.

- Run 4 flashed through hold-3 recovery: far blob `8fbb46ed…` (12 of 17
  sectors; `0xFB000` keeps unused bytes from the previous blob), main 15
  sectors, readback `cb01527b…`. Result: pending.

## 2026-11-09 DOOM RUNS ON THE TD-H9 (run 4)

- The user confirmed "that's working!": the first-pass firmware boots
  through the cave startup, the status screen and PACK OK, then plays
  Freedoom E1M1 on the radio's panel, read from the resource chip, with key
  control.
- User feedback: no sound (expected: sound is stubbed in this pass), and the
  backlight is dim.

## 2026-11-09 Doom build 5: full backlight, key test (offline)

- **Backlight:** the cave startup set the PWM duty to stock's formula,
  saved level (RAM `0x12BAA`) × 2000 + 2000. That level is loaded only by
  stock `my_app`, so the panel came up at the minimum. Fixed at the top
  step, 10000.
- **Key test:** holding Menu at power-on shows the stock key byte of each
  button (live, plus the last eight) instead of starting the game. It is for
  mapping 2/4/5/6 to WASD-style movement as the user asked; only button 3
  = 5 is known. Hold 3 for 2 s still enters the USB loader.
- Host test still runs 140 tics. Package `a0e36c1e…` (flash `8347117c…`),
  far blob `6dae58ac…`.

## 2026-11-09 Keypad codes measured; WASD-style controls (build 6)

- Key test on hardware (build 5, hold Menu at power-on): the digits report
  the digit + 2 (1=3, 2=4, 3=5, 4=6, 5=7, 6=8); Menu=12, Up=13, Down=14,
  Back=15 as known.
- Controls, as the user asked: 2 forward, 5 back, 4 / 6 turn, 1 fire,
  3 use; the arrow keys still move and turn; other keys do nothing.
- The host test walks forward with key 2. Package `91514524…` (flash
  `e9e2e4d2…`), far blob `1bdf7edd…`.

- Build 6 flashed through hold-3: far blob `1bdf7edd…` (12 of 17 sectors),
  main 16 sectors, readback `e9e2e4d2…`.

## 2026-11-09 Brightness: backlight already at stock's maximum; gamma lift (build 7)

- The user found build 6 brighter but asked for more.
- Stock reads the saved level (settings item 3229) and clamps it: `if (level
  >= 5) level = 4` (app+`0x72022`). Its top duty is therefore 4 × 2000 +
  2000 = 10000, which the Doom firmware already uses.
- Build 7 lifts the image instead: a gamma curve (exponent 0.6) applied to
  the pump palette after each `pal_read` (`I_SetPalette`).
  - Host test still runs 175 tics, visibly brighter
    (`host-evidence/e1m1-host-gamma.png`).
  - Package `6db162a6…`.

## 2026-11-09 Sound research: stock plays tone *files*, no beep generator

- The `key_tone` name (app+`0xB19CE`) is used at app+`0x61016`, inside a
  large initialiser that also reads settings items 88 and 92. It allocates
  a 44-byte request block (pointer at `0x11AD0`+236) and registers task
  callback app+`0x4A5DA`.
- The callback waits on a signal, then takes a file name from the block (it
  scans for the `.` extension) and starts the stock decoder on it. Key
  beeps and prompts are files from the voice container's JLFS `tone`
  (11,025 Hz PCM WAVs and MP3s), decoded through the audio pipeline to the
  speaker.
- Request routine near app+`0x52330` ("play tone N"):
  - It looks the name up in a pointer table at `0x01EB5CA0`, indexed by
    tone number minus 1 (8 entries).
  - It fills the block: `[+4]` = name, `[+8]`, `b[+12]`, `b[+13]`,
    `h[+14]`, `[+16]`, `[+20]`.
  - It sets `b[+2]` = 1 (pending) and signals the task (event at
    `0x01EB1385`+1865).
- Speaker amplifier enable: GPIO pin 3 via app+`0x052228` (earlier
  evidence, `main-speaker-gate.md`).
- **Options for Doom sound:**
  - (a) call "play tone N": prompt-style sounds only, from existing files
  - (b) find the PCM output the decoder feeds (DAC write) and stream
    Doom's 11 kHz samples, or DMC100's PC-speaker effects as synthesized
    square waves
  - (b) is the real route. It needs the DAC output API and must check that
    the audio pipeline is initialised without `my_app`.

## 2026-11-09 One-session install: `flash --far`

- The user asked for a single flash.
- `h9_flasher.py flash … --far <blob> --far-backup <file>` writes and
  verifies the main image, then backs up, writes and verifies the far region
  (`0xEA000`) in the same RAM-helper session. If the main image already
  matches, only the far region is written.
- `write_far_with_loader` is shared with `write-far`. Far writes are
  refused with resume and restore.
- 2 new tests (one session; refusal with recovery). All flasher tests pass.
- Bluetooth cannot replace USB for code: the CPU executes from main flash,
  and the S2 service writes only the resource chip. A main-flash writer over
  Bluetooth would be possible but risky. Game data already goes over
  Bluetooth once per pack.

## 2026-11-09 Build 8: Bluetooth upload inside Doom (offline)

- The Doom image is now built on H9 Toolbox Firmware (app `1334a32c…`)
  instead of stock, so it carries the S2 upload service and its
  clone-protocol hooks.
- Byte comparison with H9 Toolbox: every service byte is unchanged. That
  covers the hooks at app+`0x5D018`/`0x5D122`, `.s2` at `0xA51A8`, the
  tables at `0xA7E50`, and the reader at `0xA9D9E–0xAA178`. Only Brick
  Breaker's game in the cave is replaced, by the startup (748 B, below the
  tables).
- The service's stock calls are all outside the Doom regions (`0x4FF06`,
  `0x4FF22`, `0x50000`, `0x5006E`, `0x50286`, `0x50316`, `0x52CB2`,
  `0x5D2E0`, `0x16652`). Its readiness check (and the partition reader's)
  wants RAM `0x12234` = `0x5AA5`; `td_main` sets it after `td_nor_setup`.
- No complete pack now shows "WAITING FOR BLUETOOTH UPLOAD" and re-checks
  the directory every 3 s; once the pack is complete it starts the game.
  Hold 3 for 2 s still enters the USB loader.
- Includes the gamma lift and the WASD controls. Host test ok. Package
  `add5a849…` (flash `0fff5d8c…`), far blob `173587a0…`.
- Open: whether the clone protocol session answers with `my_app` replaced.

- Build 8 flashed in **one session** (`flash --far`, first hardware use):
  main 22 sectors, readback `0fff5d8c…`; then far 12 of 17 sectors,
  verified `173587a0…`. Bluetooth check: pending.

## 2026-11-09 Build 8 on hardware: Bluetooth does not advertise inside Doom

- Doom runs (with the pack present), but the radio does not appear in the
  Web Bluetooth picker: Bluetooth itself is off, not just the upload
  service.
- The five code sites that name the `btstack` task (`0x164FA`, `0x1838A`,
  `0x18806`, `0x1FF02`, `0x23EDC`) are not `my_app`-only. `my_app`'s body
  launches one task, `my_ui` (task launcher `0x2CF22` at app+`0x88752`,
  entry app+`0x89164`), the radio UI.
- The `my_app` block calls into the Bluetooth range 57 times, mostly
  `0x1682C` (a getter), `0x17832`/`0x17D66` (state at `0xCE9C`) and
  `0x17BC8`.
- Hypothesis: the radio app or UI requests the Bluetooth mode or start (in
  JieLi's SDK, a UI-requested app-mode switch), which the Doom firmware
  never does. Next: find that request and replicate it in `td_main`.
- Fallback that works today: upload with H9 Toolbox Firmware, then flash
  Doom (the pack survives).

## 2026-11-09 Bluetooth start-up path in stock (static + stock RAM)

- `app_core` (app+`0x66648`) runs a mode state machine: current mode at
  RAM `0x11AD0+101`, requested mode at `+100` (copied and cleared at
  app+`0x6336C`). Jump table at app+`0x667F2`: mode 1 → `0x63390`,
  2 → `0x633BE`, **3 → `0x63C9A`**, 4 → `0x65700`, 5 → `0x6616A`,
  10 → `0x6261E`.
- Stock RAM (idle dump): mode 3, request 0, `+360` = `0x01EB2C90`,
  `0xC11A` = 98, `0x12234` = `0x5AA5`.
- Mode 3 (`0x63C9A`) runs the Bluetooth stack start-up (`0x634EC`,
  `0x15598`, `0x181B4…0x1835C`, which name the `btstack` task) when flag
  bit 2 at `0xC118` is set and `0x630EE` (`[0x11AD0+764]` and `+108`
  bit 2) agrees. Bit 2 is set at app+`0x8A1D6` and app+`0x8DC6C`.
- The request writer was not found statically (likely through a pointer).
- Build 9 (diagnostic) shows mode, request, `+360`, `0xC11A`, `+108` and
  `+120` inside Doom for 6 s. Package `ea2cdadf…`, flash `97c32338…`, far
  `e8fe7394…`.

- Build 9 on hardware (inside Doom): MODE 10, REQ 0, P360 0, C11A 98,
  B108 0, B120 0. `app_core` stays in its start-up mode 10 (set at
  app+`0x667CA`); stock's radio app requests mode 3. Doom never does, so
  the Bluetooth start-up never runs.
- Build 10: `td_main` writes request 3 (`0x11AD0+100`) after the
  diagnostic and shows the mode and `+360` 2 s later ("AFTER").

- Build 10 on hardware: AFTER 10 0. The request alone is not applied.
- Cause: mode 10's loop (app+`0x665B2`) blocks in the message wait
  `0x2D750` and runs the request-apply function `0x63354` only after a
  message arrives. Stock's `0x2D77A(code, value)` posts a 24-byte event
  (type 1, magic `0x4B450000`, code at +10, value at +12) to task
  `app_core` (name at `0x01EB1C36`). Code `0xFFFF` is ignored by the
  event handler (`0x63110`, check at app+`0x6315C`).
- Build 11: request 3, then `h9_post_core_event(0xFFFF, 0)` (wrapper
  calling `0x01E2D87A`). Package `95d61c9f…`, flash `77bcfaa7…`, far
  `77bfb6a0…`.

- Build 11 on hardware: boot loop (hold 3 at power-on still reached USB).
  Mode 3's call tree (841 functions) never enters the Doom regions.
  Likely cause: heap exhaustion. Stock's allocation-failure path
  `0x4A574` jumps to ROM `0x0200000A` (reset). Stock runs Bluetooth with
  288/527 heap blocks used; Doom's heap is 209 blocks with 167 used, so
  the Bluetooth start-up's buffers do not fit beside Doom's 40 KB RAM block.
- Build 12 (recovery): build 11 without the mode request. Package
  `2d3f03ce…`, flash `58b134bd…` (built, not flashed; superseded by 13).

## 2026-11-09 Build 13: boot-key Bluetooth upload mode

- The user's idea: open Bluetooth only when a boot key is held.
- Stock heap init (app+`0x5FA7C`): 128-byte blocks; free-bit bitmap at
  `0x1D7D0` (a set bit is free), summary bits at `0x1D7D0+192`, header at
  `0x1D898` (block total at `0x1D8AA`, used at `0x1D8A8`). It frees blocks
  `0x1E4D8`–end (stock end `0x2BF00`, Doom's patched end `0x22000`) and
  `0x2D27F`–`0x2FF80`.
- Hold 2 at power-on (or no complete pack): Doom does not start.
  `upload_mode` hands blocks `0x22000–0x2BF00` (318) back with interrupts
  off (ROM `0x02001634`/`0x02001650`), adds them to the total, writes
  request 3 and posts the `0xFFFF` event. After that nothing in the Doom
  RAM block is used. The screen refreshes the mode, `+360` and heap counts
  every second; hold 3 enters USB.
- Normal power-on: Doom, with no mode request; the build 9–11 diagnostic
  is removed.
- Host test ok. Package `8da0f5c9…`, flash `60245a85…`, far `94a64b21…`.

- Build 13 on hardware (user: "thats working"): normal power-on runs Doom.
  Hold 2 at power-on enters upload mode, Bluetooth comes up, and the
  radio is reachable from the data-upload page. Uploading inside the Doom
  firmware works via the boot key.

## 2026-11-09 Sound step 2: the DAC (static) and the build 14 audio probe

- Audio MMIO: `0x1E2F00` (DAC) and `0x1E19C0`. The DAC driver is at
  app+`0x43100–0x43880`.
- Stock `audio_dac_start` (app+`0x43434`, handle at `0x11AD0`+6636)
  writes:
  - `+4` the ring buffer address
  - `+8` its length in samples per channel (bytes / 2 / channels)
  - `+16`/`+20` set to 0 (hardware read / software write pointers)
  - `+0` |= `0x100`, then `+12` = `0x3C`, then `+0` |= `0x110`
  - `+40` the volume (L | R << 16) and `+44`
- Speaker amplifier: `0x5224C(source, on)`; source 1 passes the request
  straight to the GPIO writer (state byte `0x11B4D` bit 0).
- Build 14: hold 4 at power-on gives an audio probe instead of Doom. It
  shows the live DAC registers and the amplifier byte. Key 1: amplifier
  on. Key 2: square wave into the existing ring buffer. Key 5: start the
  DAC on our own 1024-sample buffer (DMA bit `0x100` only).
- Package `d7d54200…`, flash `f3581298…`, far `f1e70228…`.

- Build 14 on hardware (photos): before any key, CON ends `…19F`, ADR is
  about `0xE5Ex`, LEN is `0x100`, HRP/SWP are small and do not move, AMP
  is 0. Key 1 sets AMP to 1 (the gate works). Keys 2 and 5 gave no sound;
  after 5, ADR is our buffer, LEN `0x200`, HRP/SWP 0 and still not moving.
  The DAC is stopped inside Doom, and its registers alone do not start it.
- Stock's start sequence (key-tone set-up, app+`0x611C8`), on the handle at
  `0x11AD0+6636`:
  1. `0x433C8(dac, 8000)` sets the rate. It looks the rate up in the table
     at `0x01EB0BB0`: 96000, 88200, 64000, 48000, 44100, 32000, 24000,
     22050, 16000, 12000, 11025, 8000.
  2. `0x43434(dac)` starts the DAC.
  3. `0x43598(pcm)` writes 128 bytes through the handle at `[0x12B48]`;
     it only writes when the state byte (`+97`) is 2 or 3.
  4. `0x43656` runs last.
- Build 15 (hold 4 probe): 1 amplifier on; 2 rate 11025 and start; 5 while
  held, stream a 690 Hz square wave; 6 all of these plus a 1 s tone. The
  screen also shows the state byte and the write count.
- Package `821f2270…`, flash `822e7c45…`, far `26c5ae81…`.

- Build 15 on hardware: no sound for any key. After key 6: STATE 3,
  WRITES about `0x9506C` in 1 s (every write refused: the ring buffer is
  full), HRP and SWP 0, LEN `0x100`, AMP 1. The driver state is running,
  but the DAC hardware is not consuming (clock or analog power off).
- Stock play-tone `0x5231A(n)`: gate (source 3, on), name from the
  language table (`b[0x11AD0+4326]`, tables at `0x01EAD2B0`/`0x01EB5CA0`),
  then fills the key-tone block `[0x11AD0+236]` and signals the task.
- Build 16 probe (hold 4): gives the heap back on entry. Keys 7/8/9 play
  stock tones 0/1/2. Key 2 toggles page 2, showing `0x1E19C0…+0x14`.
  Keys 1, 5 and 6 as in build 15.
- Package `03c3219e…`, flash `558fe241…`, far `8527dc00…`.

- Build 16 on hardware: stock tones 7/8/9 move HRP (HRP = SWP afterwards:
  about `0x0CE`, `0x6C8`, `0x5E6`; state 4 between tones). So the stock
  path runs the DAC hardware. With key 1 first, the amplifier byte goes
  1 → 3 → 1 during each tone, but nothing is heard. Key beeps are off in
  the radio's settings (the stock tones are likely at volume 0), and the
  volume knob is up. Page 2 (`0x1E19C0`): about `06A3?903`, `55000000`,
  `83?00000`, `00030000`, then 0.
- Build 17 probe: key 5 writes the DAC volume register `+40` =
  `0x40004000`. Key 6: amplifier on, stock tone 0 to wake the hardware,
  then our 1 s square wave through the stock write. Package `54499096…`,
  flash `0de721ed…`, far `1e116744…`.

- Build 17 on hardware: key 5 alone does not crash and gives no sound.
  Key 6 alone crashes: our writes collided with the playing stock tone.
- Build 18: key 6 plays the stock tone, waits 1.5 s, re-gates the
  amplifier, sets rate 11025 and starts (stock), then streams 1 s.

- Build 18 on hardware: no sound. After key 6: STATE 2, WRITES about
  `0x315xx`, HRP = SWP (`…26`, static), VOL still `FBAD022C`.
- Reference check: H9 Toolbox Firmware reflashed (flash `5af9bdfe…`;
  backup of build 18 saved) so the user can turn key beeps on. Then Doom
  build 18 goes back, and the probe tests stock tones with beeps enabled.

- Build 18 again, with key beeps turned on in H9 Toolbox: still silent
  (keys 1, 7, 6).
- Radio-app-only audio calls (the `my_app` block): `0x5224C` ×24,
  `0x523C6` ×21, `0x5231A` ×3, `0x520CE` ×2, `0x52228` ×2, `0x60DDA` ×1.
- `0x60DDA(state, level)` is app_audio_set_volume: state 0–4, level
  clamped to 30, stored at `0x11AD0+4404…`. Stock start-up calls (3, 15)
  and the key-tone set-up calls (1, beep setting); `my_app` calls
  (2, `b[+70]`) at app+`0x82140`. Doom never sets the volume.
- Build 19 probe: key 5 sets volume 30 for states 0–4. Key 7 plays stock
  tone 0. Keys 8/9 call `0x520CE(1)`/`(0)`. Key 6 sets the volume, then
  as build 18.

- Confirmed by the user: key beeps play in H9 Toolbox Firmware (speaker
  path works with the radio app). Build 19 flashed for the volume test.

- Build 19 on hardware: silent for every key (1/5/7, 6, 8 then 7).
- Changed approach: measure the working state. `far-hello/audio.c` (far
  program, 1,768 B, `2902e2f7…`) runs inside H9 Toolbox Firmware
  (Back-hold), where key beeps play. It shows the DAC block, the
  `0x1E19C0` block and GPIO ports A–D OUT/DIR, live and as a snapshot
  taken 60 ms into stock tone 0 (key 7). Install: H9 Toolbox
  (`5af9bdfe…`) + `--far` this program, one session.

## 2026-11-09 Audio reference: registers in working H9 Toolbox

- `far-hello/audio.c` inside H9 Toolbox (Back-hold). Key 7 plays stock
  tone 0, which is the voice prompt "zero", audible. Photos (snapshot 60 ms
  into the prompt / live):
  - DAC: CON `0x0F41E135` / `0x0F41E195`, ADR `0xE524`, LEN `0x1000`,
    HRP `0x9CC` / `0xC9C`, SWP `0x19C` / `0xC9C` (moving), VOL
    `FBAD022C`, `+2C` 0.
  - `0x1E19C0`: `06A41403`; `+4` `2550001C` / `25500000`; `+8`
    `33100000` / `B3100000`; `+C` `00010000`; `+10`, `+14` 0.
  - GPIO (same in snapshot and live): PA OUT `2209`, DIR `DC56`; PB OUT
    `08C4`, DIR `F100`; PC OUT 0, DIR `FFF2`; PD OUT `0018`, DIR `FFEC`.
- Against Doom's probe photos: **DAC CON bit 24** is set in the working
  radio (`0x0F…`) and clear in Doom (`0x0E…`). The only setter is
  app+`0x4B8DA`, at the end of an analog power-up sequence (block
  `0x1F1700` with stepwise enables and delays). It is reached through a
  pointer, not a direct call.
- Build 20 probe: pages D/A/G as in the reference (key 2). Key 8 sets
  CON bit 24. Key 9 calls `0x520CE(1)`. Key 7 plays the voice prompt.
  Package `cf5c7b22…`, flash `e62da654…`, far `698374d2…`.

- Build 20 on hardware: silent. Doom vs the reference:
  - DAC CON `0x0E41E19x` (bit 24 still clear after key 8)
  - `0x1E19C0` `06A41903` vs `06A41403`; `+C` `00030000` vs `00010000`
  - GPIO PA OUT `2208` vs `2209` (PA0 low); PA DIR `06BE` vs `DC56`;
    PB OUT `0BC?` vs `08C4`; PC OUT `6?0?`; PC DIR `FFFF` vs `FFF2`
- Build 21: key 8 drives PA0 high (output) and PC0/PC2/PC3 low (outputs),
  the reference receive state, then retries CON bit 24.

- Build 21 on hardware: with keys 1 and 8, PA OUT = `2209` and PC DIR =
  `FFF2` (the reference), but still silent.
- `0x42E24` (called by DAC start) writes CON bit 24 itself (with bit 26,
  from a config flag at `[[dac+4]+8]` bit 20), yet Doom reads bit 24 as 0.
  It behaves like an analog-ready status bit.
- Build 22: key 9 runs stock's analog power-up tail app+`0x4B82E..0x4B914`
  (block `0x1F1700`, table `0x01EB0D14`, ROM `0x02003DD8`, CON bit 24)
  through a wrapper that builds the function's frame and jumps in.
  Package `bd5fa92d…`, flash `b0b1feb9…`, far `aa8a9d3b…`.

- Build 22 on hardware: key 9 freezes the radio; no audio.
- The function holding the bit-24 code starts at app+`0x4AEAC` (frame
  `{rets, r15-r4}`, sp -16). It allocates 6040 + 36 + 4096 heap bytes
  (`[0x11AD0+800/812/804]`) and is entry 0 of an ops table at app+`0xA14`
  (`0x4AEAC`, `0x4BA5C`, `0x4BB4A`, `0x4BB7E`, `0x4BB88`, then the name
  `fm_inside`). It is the built-in FM receiver's init; the table is used at
  app+`0x661A0` (app_core). CON bit 24 is FM's analog link, not the
  speaker path.
- Build 23: probe key 9 requests app_core mode 3 (stock's running mode)
  with the `0xFFFF` event, as upload mode does; the heap is already given
  back. The screen shows the mode. Package `8e991a03…`, flash `c3d54677…`,
  far `766f3f33…`.

- Build 23 on hardware: mode 3 reached (M = 03), still silent.
- The user recalled the speaker GPIO (`main-speaker-gate.md`: PA3, written
  by app+`0x52228`). Rechecked direction as well as output: the reference
  PA DIR is `DC56` (PA3 output); Doom's is `06BE` (PA3 input; JieLi DIR 1 =
  input). Stock's writer only sets OUT, so in Doom the amplifier enable was
  never driven.
- Build 24: key 8 makes PA0 and PA3 outputs and drives them high (plus
  PC0/2/3 as before).

## 2026-11-09 SOUND WORKS inside the Doom firmware (build 24)

- Build 24 on hardware (far `ca94236b…`): key 7 plays the stock voice
  prompt "zero"; key 6 plays our square wave (through stock's DAC write)
  and the prompt. First audio from our own code in the Doom firmware.
- Cause of builds 14–23's silence: the speaker amplifier enable PA3 was
  configured as an input (PA DIR bit 3 = 1). The radio app sets it as an
  output at start-up; stock's speaker writer (app+`0x52228`) only writes
  OUT. The user pointed back to the pin-3 speaker finding.
- Working recipe so far: PA3 output and high; DAC rate and start through
  stock (`0x433C8`, `0x43434`); PCM through stock's write (`0x43598`,
  128 bytes per call). Still to establish:
  - which other steps are needed (key 9 mode 3, PA0, PC0/2/3, volume,
    a stock tone first)
  - how to feed Doom's mixer

## 2026-11-09 Build 25: Doom sound effects (PC speaker through the DAC)

- DMC100's `port/game/s_pcspk.c` (Chocolate Doom PC-speaker semantics)
  replaces the sound stubs. The pack's SOUNDS section (section 8, DP lumps)
  holds the tone bytes.
- `port/td_sound.c` is the TD-H9 stepper behind `snd_play/stop/busy/volume`:
  - 140 steps/s; each byte is a PIT divisor (Chocolate Doom table), turned
    into a square wave in 11025 Hz 16-bit mono PCM.
  - Output goes through stock's DAC write (128 B per call).
  - It keeps about 200 ms ahead of the 100 Hz tick, topped up from every
    renderer strip, every input poll and `I_WaitVBL`.
  - Init (after the pack checks): PA3 output and high, speaker gate,
    volume 30, rate 11025, start.
  - The status screen shows SOUND ON / NO SOUNDS.
- Host test ok. Package `271ca1bc…`, flash `1f20d50f…`, far `77d51a1b…`.

- Build 25 on hardware (user: "i hear it!"): Doom's PC-speaker sound
  effects play through the TD-H9 speaker during the game.
- Next request: music.

## 2026-11-09 Music (route A: chiptune player), build 26

- The user chose route A (on-radio chiptune) over pre-rendered audio.
- `doom-firmware/music.py` converts MIDI to H9MU (header + events in
  64-sample chunks): 7 square voices allocated offline (oldest stolen), 1
  drum voice; volume = velocity × channel volume, 0–15. Freedoom
  `D_E1M1` (MIDI, 50,124 B, 18 tracks) → `E1M1.h9mu` 28,652 B, 130.6 s,
  3,746 notes (403 stolen), 1,737 drum hits. Output is kept outside the
  repo (`doom-packs/music/`).
- `port/td_music.c`: the player. Square voices; drums are noise
  (hats/snare/cymbals with decays) or a low square (kick/toms). Events are
  read from the chip 256 B at a time, and the song loops. `td_sound.c`
  mixes one chunk under the effects, clipped.
- `test/music_wav.c` builds the same player on the host: preview
  `E1M1-radio-preview.wav`, peak 11,196, no clipping.
- Music slot: its own upload directory at `0xCE0000`, data
  `0xCE1000–0xD00000` (124 KiB). The game slot's resource area now ends at
  `0xCE0000`; the E1M1S pack (1 MB at `0xA20000`) is unaffected.
  `td_dir_find()` generalises `td_pack_find()`. The status screen shows
  MUSIC ON / NO MUSIC.
- Uploader: `SLOTS` (game, music), with the slot passed to planLayout,
  buildDirectory, uploadPack and restore; a slot selector on the page. A
  new test checks that a music upload and restore leave the game pack
  intact. 10/10 pass.
- Build 26: package `5080287d…`, flash `610f1b4d…`, far `fd471ac0…`.

- Build 26 flashed (flash `610f1b4d…`, far `fd471ac0…`). The music was
  uploaded in Doom's hold-2 upload mode to the music slot: 28,652 B
  (SHA-256 `a7ac9a93…`) at `0xCE1000`, directory `0xCE0000`, 8 sectors,
  verified and complete. The write rate was 123 B/s (266.8 s), versus
  785 B/s in H9 Toolbox; to investigate later. The record's name is
  "freedoom E1M1S" (the page's default; cosmetic).

- Build 26 on hardware: music plays and the audio is right ("perfect").
  The picture stutters a little when moving and when far things are in
  view; close to a wall it is smoother.
- Build 27 (diagnostic):
  - Frame-rate readout top-left (frames in the last second, slowest frame
    in ms), drawn in `I_FinishUpdate`.
  - Key 7 toggles the music, to compare its cost in the same view.
  - The sound pump writes at most 2 chunks a call (11.6 ms) unless the lead
    is below half, so top-ups inside the renderer do not lengthen one frame
    in a lump.
  - Package `788d4bc4…`, flash `cc06a679…`, far `22881e87…`.

- Build 27 on hardware: about 10–12 fps; slowest frame 90–160 ms. Music
  on/off (key 7) changes nothing, so the synth is not the cost.
- Build 28: key 8 skips the panel output (renderer and sound still run),
  to split the panel transfer from Doom's drawing and the chip reads.

- Build 28 on hardware (far view): panel on about 10–12 fps, slowest
  90–160 ms; panel off (key 8) about 10–20 fps, slowest 80–110 ms. The
  per-column panel output is a real share; Doom's drawing and chip reads
  are the larger part.
- Build 29: `td_pump.c` batches up to 16 consecutive panel columns (4 KiB,
  row-major) per window: about 10 windows a frame instead of 160, and long
  transfers. Flushed on `pump_end`/`pump_sync`/a column break; the frame
  readout flushes first. Host render intact.

- Build 29 on hardware: up close to a wall about 24 fps, slowest 60 ms (was
  about 10–12 fps); a far view is still 8–14 fps, up to 140 ms. The panel
  batching works; far views are bound by drawing and chip reads.
- Build 30: the readout adds chip KB per frame (`td_nor_bytes` counted in
  `nor_read`), to tell synchronous chip reads (about 2 MB/s) from
  computation. Package `a0ef2f0e…`, flash `ad38a922…`, far `e21a7b9a…`.

- Build 30 on hardware: wall 24 fps / 60 ms / 5 KB a frame; far view
  8 fps / 140 ms / 75 KB a frame.
- Stock's chip read (app+`0x4FE22`) takes its SPI DMA path (`0x4C25A`)
  only for n ≥ 16 into a 4-byte-aligned buffer. Otherwise it moves one byte
  at a time (register write plus wait call `0x4BC0E`), with interrupts
  off for the whole read.
- Build 31: `nor_read` sends short or unaligned reads through a 512-byte
  aligned bounce buffer (at least 16 bytes read), so all reads take the DMA
  path; the chip's last bytes keep the byte path. Package `3df78498…`,
  flash `a3da97eb…`, far `6f12d13c…`.

- Build 31 on hardware: far 8 fps / 140 ms / 70 KB; wall 22 fps / 60 ms /
  7 KB. No change, so DMC100's reads were already on the DMA path. The far
  view is bound by drawing on this CPU. The user's call: "as good as we
  can be". Performance work stops here.

## 2026-11-09 Doom firmware status (end of this round)

- Runs Freedoom E1M1 at about 22–24 fps up close and about 8–14 fps in
  far views. The panel output is batched (build 29).
- Brighter picture (backlight at stock's maximum plus gamma 0.6) and
  WASD-style keys.
- PC-speaker sound effects and E1M1 chiptune music through the speaker
  (PA3 must be an output). Key 7 toggles the music.
- Hold 2 at power-on: Bluetooth upload mode (game and music slots).
  Hold 4: audio probe. Hold 3: USB loader. Menu: key test.
- Installed in one USB session (`h9_flasher.py flash … --far …`).
- Diagnostics still in the image: the frame readout, key 8 (panel off),
  the hold-4 audio probe and the start-up trace. A release build would
  remove them.

## 2026-11-09 Release 32 (clean) and the slow-upload test

- Removed:
  - the frame readout and key 8 (panel off)
  - the hold-4 audio probe and the Menu key test
  - the heap and pack lines on the status screen
  - the start-up stage wrappers (`WRAPS = ()`; `td_trace.c` keeps only the
    counters)
  - the probe-only stock calls (`h9_play_tone`, `h9_audio_route` and
    `h9_dac_analog_on`, the FM power-up that froze the radio)
- Status screen: GAME DATA OK, SOUND ON, MUSIC ON. Keys: 2/5 move, 4/6
  turn, 1 fire, 3 use, 7 music on/off. At power-on: hold 3 for USB, hold 2
  for upload mode.
- Slow upload: build 26's upload mode wrote at 123 B/s, and its 32 KB check
  read took about 30 s (about 1 KB/s against 6.2 KiB/s in H9 Toolbox), so
  the whole Bluetooth link is about 6x slower. The clone-protocol handler
  (`0x5CF18`, hooked at `0x5D018`) is reached from `0x5DBA8`, which is
  registered at `0x24004` (BT stack context), so no `my_app` task is
  involved. Release 32's upload mode lets key 5 cycle the wait style of
  our loop (Y yield 10 ms, B never yield, L yield 200 ms) to test
  low-power idle or CPU contention.
- Multi-key input: requested. The stock key byte (`0xC3AF`) holds one key,
  and no direct writer was found; the keypad hardware (matrix or AD
  ladder) is still to be identified.
- Package `a68c4dcd…`, flash `10a741b9…`, far `c1e8c688…`.

## 2026-11-09 Doom installer page (/doom/) and firmware page section

- `dist/doom/`:
  - `index.html`, `doom.css`: a five-step guide (files, USB install,
    game data, music, play).
  - `doom.mjs`: the page.
  - `install.mjs` (pure): file checks, `doomTarget`, `planInstall` (Doom
    code first, then the firmware high to low), and `DoomHid` (the web
    flasher's HID client, also allowed at `0xEA000–0xFBFFF`; never the
    boot region or the settings VM).
  - `upload.mjs`: the canonical Bluetooth uploader; the prototype now
    re-exports it.
- Files come from a manifest on raw.githubusercontent.com (`GITHUB.base`,
  branch `doom-files`; every file is checked against its SHA-256), or from
  the user's own `.fw`/`.bin`/pack/song (structure checked).
- `doom-firmware/stage_release.py` stages the folder and
  `doom-manifest.json` (git-ignored `build/doom-release`). Release r32:
  - fw `a68c4dcd…` (image `10a741b9…`)
  - code `c1e8c688…`
  - pack `06442c61…`
  - music `a7ac9a93…`
  Nothing is published.
- `firmware-update/firmware.mjs` exports `decodePackage` (no hash pin); the
  pinned loaders are unchanged. `firmware-update.test.mjs` has 1 failure,
  the same before and after this change (the `verified-official-image-seed`
  string).
- Browser check: the page renders, and the manifest flow (served
  same-origin) loads all four files with matching hashes and no console
  errors. The firmware page has a Doom section with steps and a link.
- **Untested on hardware:** HID erase and write at `0xEA000+`. CRC reads
  there are proven (`scanRadio` covers 1 MiB). The HID updater's
  acceptance of the far region needs a test before the page is relied on.
- Tests: `doom-install` 4/4, `data-upload` 10/10.

- Installer and firmware page reworked:
  - Three steps (Install over USB-C, Upload the game data over Bluetooth,
    Play).
  - A "Load Doom" card loads the files hosted in `dist/doom/files/`, as
    firmware-update hosts H9 Toolbox, and checks them against
    `doom-manifest.json` with nothing downloaded; "Your own build" is the
    alternative.
  - The GitHub fetch and raw.githubusercontent.com in the CSP are removed.
- The user approved committing the release files (an exception to the
  no-binaries rule for this site release): `h9-doom-r32.fw`,
  `h9-doom-r32-code-0xEA000.bin`, `freedoom-e1m1-pack.dat`,
  `freedoom-e1m1.h9mu` and the manifest.

## 2026-11-09 JieLi toolchain bundled

- The user decided to include the toolchain so people need not hunt for it
  (they will remove it if JieLi asks).
- `firmware-research/toolchain/jieli/` holds
  `jieli-linux-toolchains-20260730.1.tar.xz`, unchanged from
  `jl-update.oss-cn-shenzhen.aliyuncs.com` (SHA-256 `a1545cdd…`,
  26,027,508 B), plus `setup.sh` (checks the hash, extracts next to it;
  the extracted folder is git-ignored) and a README.
- `toolchain/h9cc` and `doom-firmware/build.py` default to the bundled copy
  when `JIELI_TOOLCHAIN` is unset.
- Check: Doom built with no `JIELI_TOOLCHAIN` reproduces release 32
  byte for byte (package `a68c4dcd…`, flash `10a741b9…`, far
  `c1e8c688…`).

## 2026-11-09 HID far-region probe, part 1: 0xFB000 holds a leftover

- `hid-recovery/hid_far_probe.py` (one spare sector, `0xFB000`) found CRC
  `0xEFA9` there, not erased, and refused to write.
- USB dump (`h9-main-flash-full-20261109-doom32.bin`, SHA-256
  `e9818226…`) against the morning dump (08:40, H9 Toolbox): only
  `0xEA000–0xFB109` changed. `0xEA000–0xFA4ED` is the current Doom code;
  `0xFB000` has 208 bytes to `0xFB109`, data that also appears in the
  current blob at `0x103DE`.
- Cause: Doom builds 1–3 had 18-sector far blobs (their far backups are
  73,728 B, `0xEA000–0xFBFFF`); later builds use 17. Nothing reads it.
  No system writes to the far region; stock's data at `0xFD000` is
  unchanged.
- The probe now also accepts that known leftover.

- HID far-region probe, part 2 (PTT+2, `0xFB000`):
  - the leftover (CRC `0xEFA9`) erased to `0xE03E`
  - a 4 KiB pattern written in 40-byte C3 packets read back CRC `0xD0BA`
    (stored exactly as sent, no encryption)
  - erased again to `0xE03E`
  **The vendor HID updater erases and writes the far region.** The /doom/
  installer's USB step is viable.

## 2026-11-09 Web install of Doom works end to end (/doom/)

- The radio was restored to stock v1.0.38 over PTT+2 (`restore_stock.py
  --known h9-doom-r32.fw`: 23 sectors, all 213 stock). In Chrome, `/doom/`:
  Load Doom (4 files hash-checked) → Connect → Back up (5 s, `.h9hid`) →
  Install: 0 code sectors (r32 code was still at `0xEA000`), 23 firmware
  sectors, all verified. Doom runs from a browser install.
- Bluetooth step: the uploader read the whole 1 MB target (1,060,864 B) to
  check it before noticing the data was already complete, about 17 minutes
  at upload mode's about 1 KB/s.
- Fixes:
  - `uploadPack` reads the slot's directory sector first and returns
    (`alreadyComplete`) when it is COMPLETE with the same length and
    SHA-256 (COMPLETE is written only after a full read-back verify).
  - The post-install message no longer claims there is no game data.
  - New test (only the directory is read); data-upload 11/11.
- Added "Remove Doom data" (`/doom/` step 02, collapsed): `restore()` for
  the music then the game slot, read back as erased. It reads each sector
  first (the erase command carries the sector's CRC as a guard), so it is
  slow until upload mode's Bluetooth rate is fixed.

- The "already on the radio" check took exactly 60 s in upload mode: fast
  reads (`readFast`) start at a 32 KiB bank, so the 4 KiB game directory
  at `0xA1F000` cost a 32 KiB read from `0xA18000` (about 0.5 KiB/s). The
  same padding explains "Reading 1,060,864 bytes".
- `readDirectory()` now reads only what `parseDirectory` needs (the
  128-byte record and the 32-byte COMPLETE block) with 32-byte reads: 7
  reads (5 data + 2 bank) instead of 32 KiB.

## 2026-11-09 Build 33: system clock diagnostic (slow upload, game speed)

- Stock `clk_set` is app+`0xC538E(name, hz)`, name `0x01EB1C7B` = "sys".
  - 240 MHz: `0x633E8`, called first by mode 3's entry `0x63C9A`, and by
    `0x65714` and `0x66182`. Also `0x61EB8`, reached through `0x61FC4`
    from the radio app (`0x7B1A4`, `0x818F0`) and app_core.
  - 24 MHz: `0x5E3E2`, registered by pointer at `0x5E21E` (a
    power/idle callback).
- Hypothesis: the system drops to 24 MHz when idle and the radio app's
  activity restores 240 MHz. Without the radio app, upload mode (and maybe
  the game) runs at 24 MHz.
- Build 33:
  - `h9_clk_set_sys(hz)` calls `0x01EC548E` with "sys".
  - `td_cpu_loops()` is C-hello's LOOP (about 200,000 per tick in H9
    Toolbox).
  - The status screen shows CPU, then sets 240 MHz and shows CPU again,
    then plays at that clock.
  - Upload mode shows CPU every 3 s; key 6 sets 240 MHz.
- Package `2cb3e002…`, flash `074ca49f…`, far `9be34f1a…`.

- Build 33 on hardware (upload mode):
  - CPU about 134,900 loops per tick (fluctuating), unchanged after key 6
    (`clk_set` sys 240 MHz, "SET"). Not a 24 MHz clock; the clock
    hypothesis is ruled out.
  - Speed: a 4 KiB read about 9 s, a 32 KiB read about 60 s, writes
    0.09 KiB/s (music removal took 156.6 s). Each exchange takes about
    1 s.
- The link request reaches stock: `link_request` calls `0x01E16652` with
  the connection handle (`0x11AD0+138`), and "ok" means it returned 0.
- Next hypothesis: Bluetooth replies wait on app_core's message loop, which
  nothing wakes in upload mode (the radio app keeps it busy in H9 Toolbox).
- Build 34: wait style E (now the default) posts the ignored `0xFFFF` event
  to app_core every 10 ms; key 5 cycles E/Y/B/L. The CPU is measured once,
  not every 3 s (that busy loop took 250 ms of every 3 s). Package
  `a58ff9e8…`, flash `6c443af7…`, far `060e2d0b…`.

- Build 34 on hardware, WAIT E: the 32 KiB check read took about 24 s
  (about 1.3 KiB/s, up from about 0.5); the verify read about 24 s; the
  music upload (resumed) took 1.8 min in all. Waking app_core helps about
  2.5x; H9 Toolbox reads at about 6.5 KiB/s.
- Build 35: wait style F posts an event about every 2 ms (spin of cpu/5
  loops between, one tick yield per 5 events); styles E/F/Y/B/L.

- Build 35 on hardware, WAIT F: the check read took about 25 s (no better
  than E). Writing was worse: 8 sectors in 4.5 min (about 260 ms per 32-byte
  W), and the upload took 5.7 min in all. Music removal took 74.1 s
  (156.6 s on build 33).
- Reads and writes use the same link parameters (`link_params_fixed`
  12/12/0/600), so peripheral latency is not the write delay.
- Status:
  - Reads waited on app_core's loop; E helps (about 1.3 KiB/s).
  - Write ACKs (about 260 ms each) are a separate, unexplained
    bottleneck; the stock clone 'W' path is the next place to look.
  - A first 1 MB game upload in upload mode would take about 2.5 h,
    against about 23 min through H9 Toolbox.
- The user asked to flash back to H9 Toolbox Firmware (the hosted
  `h9-toolbox-firmware-v1.0.38.fw`).

- Flashed back to H9 Toolbox Firmware (the hosted package `b9d24cb4…`):
  19 sectors, read back `83f79fb3…` (the pinned H9 Toolbox image). The
  Doom code is still at `0xEA000` (inert), and the game data and music
  remain on the 16 MB chip.

## 2026-11-09 Release 36 (pushed to charlie/loader-toolchain)

- Release 32 plus upload mode waking app_core every 10 ms (build 34's E,
  the measured read improvement); without the build 33–35 diagnostics (CPU
  readout, `clk_set`, wait styles).
- Package `9792a590…`, flash `6bf9dc4f…`, far `bf51313f…`. Staged into
  `dist/doom/files/` (r36) for the /doom/ installer; pack and music
  unchanged.
- Not yet flashed as this exact build: each change was tested on hardware
  in builds 32–35, but not this combination.

## 2026-11-09 Merged into main and deployed

- At the user's request: origin/main (4 new commits including PR #1) was
  merged into `charlie/loader-toolchain`. Only conflict:
  `test/boot-logo-radio.test.mjs`, both sides kept.
- All test files pass except `firmware-update.test.mjs` (1 failure, the
  same on origin/main before the merge).
- `main` fast-forwarded to `cb59f23`. Pages run 42745124919 succeeded.
- Live: h9toolbox.com/doom/ (manifest r36) and the /firmware/ Doom section.
- r36 itself has not been flashed as an exact build.

## 2026-11-09 ACK path traced (static); per-write timing added

- W path:
  - `0x5DBA8`, the BT data callback registered at `0x24004`, calls
    `0x5D170`. It builds the clone ops (send byte `0x4FD82`, send buffer
    `0x4FD9E`, `0x5017C`; state `0x1202C`) and runs the clone handler
    `0x5CF18`. Our hook runs at `0x5D124`, then ACK `0x06` is appended to
    the reply buffer (`0x127B0`, 80 B).
  - Back in `0x5DBA8`: `0x5D1CA(len)` checks TX space with `0x50186` =
    `0x16552(9)` (answered directly); if it fits, `0x5D1E0` sends with
    `0x16552(10)`, queued to the `btstack` task and woken at once by
    `0x164F8` (`0x202B9C` task post, "btstack"). Otherwise a later case
    (`0x5DCC8`, code 17) resends.
- No wait of about 260 ms on that path. The clock stays at full speed: the
  24 MHz timer `0x5E208` → `0x4F4EC` → `0x5E3E2` is an inactivity
  timeout, not active in our runs.
- Remaining suspects: link timing (connection events, macOS side) or
  btstack scheduling.
- Diagnostic: `ResourceRadio.sendWrite` records each GATT write duration
  and ACK wait; `uploadPack` logs the median and max per sector. Baseline
  first on H9 Toolbox, then Doom upload mode.

Hardware results for S1 and S2, and U2 onward, will be recorded here.

## 2026-11-10 Module loader: code from the resource chip runs in RAM

- New `module-loader/` (branch `charlie/ram-loader`, local): H9 Toolbox
  Firmware with Brick Breaker removed from main flash. A 608-byte stub in
  its cave (app+`0xA77F0`) is called from the same hold-Back hook
  (app+`0x088906`). The heap end is moved from `0x2BF00` to `0x29F00`
  (app+`0x05FA9E`), giving modules an 8 KiB RAM slot. Modules live in a new
  "modules" upload slot (directory `0xCC0000`, `H9MODS01` bundle at
  `0xCC1000`). The game slot's resource area now ends at `0xCC0000`.
- Bundle build 1 (`feedc6b1…`, 3,468 B): module 0 is the menu (C, JieLi
  clang, 1,760 B); module 1 is Brick Breaker (`game.s` unchanged and
  checked against `19ab1b31…`, relinked for `0x29F00`, 1,628 B).
- Package `efb32b8e…`, flash `79858310…`. Hold-8 flash on Windows:
  - Backup `h9-before-modules-20261110.bin` = `83f79fb3…` (H9 Toolbox, as
    expected). 6 sectors were written and each one verified.
  - The final full readback failed at offset 655,360 (`0xA0000`) with
    `WinError 31`.
  - A later fresh `flash` session (new backup path) failed at the same
    offset; a `resume` straight after an error failed at offset 0.
  - Reading H9 Toolbox's image in full worked in the first session. The
    cause is unknown. It matters because the flasher's `restore`/`resume`
    start with a full read, so PTT+2 HID is the recovery path until this is
    understood.
- Hardware (user): the radio boots and works normally. With no bundle,
  holding Back does nothing. After uploading `h9_modules.bin` over
  Bluetooth (data-upload prototype, slot Modules), holding Back shows the
  module menu: **first code loaded from the resource chip into RAM and
  run** [HW]. The Bluetooth upload ran with the 8 KiB smaller heap.
- Not yet reported: Brick Breaker from the menu, RADIO (ROM `0x0200000A`
  as a deliberate restart), USB UPDATE, a long Bluetooth programming
  session with the smaller heap.

## 2026-11-10 Module loader: RADIO freeze fixed; APRS ring fills while a module runs

- **RADIO freeze:** the menu's RADIO item jumped to ROM `0x0200000A`,
  which froze the radio. Stock `cpu_reset` (app+`0xC6EC8` / `0xC6EF4`,
  CPU `0x01EC6FC8` / `0x01EC6FF4`) turns interrupts off through ROM
  `0x02001634`, sets bit 4 of `[0x1E0000]` and spins until the reset.
  The menu module now does the same (`h9_reset` in
  `module-loader/modules/common/h9lib.s`). This is a bundle-only change; the
  firmware image is unchanged (`79858310…`). The stub's own `ml_reset` still
  uses `0x0200000A` and needs the same fix at the next firmware flash.
- **APRS ring while a module runs (hardware, user report):** the APRS
  MONITOR module (`module-loader/modules/aprs_monitor/`) reads the
  main-side APRS serial ring (`0x16634`, write index `h[0x16E34]`, read
  index `h[0x16E36]`, mask 2047) and advances the read index itself. On the
  radio, WRITE and READ advanced every second and the GPS-sentence count
  rose once a second. So the ring producer (app+`0x078EFE`) keeps running
  while a module holds the main loop, and GPS NMEA (command `0x10`) arrives
  on the same link. Decoding of received AX.25 packets (command `0x00`) on
  hardware is not yet confirmed.

## 2026-11-10 Stock APRS Beacon List and Inbox live in `sys_data` (static trace)

- Received packet path: dispatcher app+`0x0870BC` (command `0x00`) decodes
  with app+`0x05D9F6`, then calls the handler app+`0x086138`
  (`0x01E86238`). The handler's tail is the digipeater (DIGI1/DIGI2 alias
  and `WIDE` compares, forward at `0x01E8616E`); `0x01E85ACC` handles `:`
  messages addressed to the radio's own callsign and compares the text with
  fixed strings (looks like remote commands; not traced further).
- The parser at `0x01E6B81C` copies the 324-byte decoded packet into RAM
  `0x13D10` (`0x11AD0 + 0x2240`, memcpy at `0x01E6BA68`). Its layout matches
  `replacement-source/main/h9_main_ax25.h` `H9MainAx25Decoded`: destination
  `+0`, source `+7` (6 ASCII bytes, spaces trimmed to 0, then the SSID
  byte), digipeaters `+14` (6 × 7), digi count `+56`, control `+57`, PID
  `+58`, information `+59` (256 bytes), information length u16 `+316`.
  Bytes `+318..+323` are not identified.
- Beacon save, `0x01E85880`: header u16 `328`, two zero bytes, then the 324
  bytes from `0x13D10`; written to storage area 6 at `index × 328 + 6`
  (limit 32,800 bytes, 100 entries). The write index is u16 at area offset
  2 (`0x01E6ADB6`, wraps at 100); the count is u16 at offset 0
  (`0x01E6AD04`, capped at 100).
- Storage areas: table of eight `{name, offset, size}` at `0x01EAA408`, all
  on partition device 3 (`sys_data`, `0x800000`, 1 MiB; partition table
  `0x01EB53F0`, read `0x01E52CB2` / write `0x01E50128`, both gated on
  `0x12234 == 0x5AA5`):
  `hw_version` 0x0 (6), `board_type` 0x100 (8), `bt_paired` 0x1000 (501),
  `sms_inbox` 0x2000 (12,800), `sms_draft` 0x5200 (12,800), `sms_sent`
  0x8400 (12,800), `aprs_beacon` 0xB600 (35,000), `spectrum_bla` 0x13EB8
  (402).
- So the Beacon List is at chip `0x80B600` (count, index, records from
  `0x80B606`) and the APRS message inbox at `0x802000`. A module can read
  both with the stock NOR read (`api->read`). These are stock-owned areas:
  read only, never written by H9 Toolbox. The inbox record format is not
  traced. Not yet checked against a real chip readback.

## 2026-11-10 Own APRS callsign in RAM; module switching (static, not yet on hardware)

- Stock's received-packet handler (`0x01E86238`) copies 6 bytes from RAM
  `0xC0F0 + 945` = `0xC4A1`, trims trailing spaces, and formats them with
  the byte at `0xC4A7` as the SSID; `0x01E85ACC` compares message
  addressees against the same bytes. So the radio's APRS callsign is at
  `0xC4A1` (6 chars) and its SSID at `0xC4A7`. The APRS modules read it to
  mark messages addressed to you. Not yet checked on hardware.
- Module platform: a "run this module next" mailbox in the last 32 bytes of
  the RAM slot (`0x2BEE0`, magic `HRUN`, 16-byte name). A module writes it
  and returns; the loader runs the menu, which clears it and returns that
  module's index without drawing. No firmware change: the stub already
  returns to the menu after a module. Module names starting with `.` are
  hidden from the menu. State kept across switches lives below `0x2BEE0`;
  the APRS screens share 0x680 bytes at `0x2B860`. `build.py` now rejects
  any `.data`/`.bss` in a module, since the loader copies `.text` only.

## 2026-11-10 Module key timing: use the 100 Hz timer, not the hold counter

- Hardware report: tapping Menu in APRS opened the map (the hold action).
  `h9_delay` takes milliseconds (h9_delay(100) = 10 ticks of the 100 Hz
  timer, 2026-11-09), not 10 ms units as `h9lib.s` said, so the first
  35-poll hold threshold was about 70 ms. The second attempt timed holds
  with `h[0xC3B2]` (the counter the hold-Back launcher compares in stock
  context) and still fired on a tap: either it does not advance while a
  module runs or not at 100 Hz there. Not investigated further.
- The APRS core now times holds (0.6 s) and the RX flash (1 s) with the
  timer-3 counter at `0x1D8B0`, measured at 100 Hz and running in takeover
  mode. Comment in `h9lib.s` corrected. Awaiting a hardware check.

## 2026-11-10 Stock APRS transmit paths (static trace)

- `0x01E5B4BE` (cmd 0x00 sender): frames a payload as serial command `0x00`
  (5 × `C0`, command, escaped payload, 3 × `C0`, framer `0x01E5B42E`) and
  writes it through the serial object's callback (`0x01E5B4AA`,
  `[[0x11AD0+556]+20]`). This hands an AX.25 frame to the APRS modem.
- `0x01E5B4EA` send_packet(p): AX.25-encodes a 324-byte decoded-packet
  structure (`0x01E5B398`; same layout as the Beacon List records) and
  sends it with `0x01E5B4BE`. `0x01E5B506` builds such a structure from a
  `CALL-SSID` string (beacon builder chain).
- Beacon sender `0x01E5C3D4` (synchronous): choose side, RF setup
  `0x01E58E3E(0)`, set TX flag `0x11D28 = 1`, key on the TX frequency
  `0x01E5AD68({tx, rx, shift, 0, cb 0x01E5A82E})`, ROM delay
  `0x02000A00(50)`, build+send `0x01E5BF2C`, delay airtime
  (`0x01E5C36C`, else 500 ms), restore side. It does not unkey; its caller
  `0x01E5CDEC` (end of TX: `0x01E58F58(0)`, `0x01E5CCCA`, flag cleared,
  RX restore `0x01E595B6`) does.
- **`0x01E5DBCC(buf, len)`: stock KISS transmit.** Deframes KISS
  (`C0`, `DB DC`/`DB DD`) into a 512-byte buffer, decodes it with the AX.25
  decoder `0x01E5DAF6`, sets the logical side `0x11B1D = 0x11B50 >> 7`,
  calls `0x01E5B144` (the PTT-press handler: mode checks, UI state, key
  up), sends with `0x01E5B4EA`, waits the airtime (`0x01E5C36C`, else
  500 ms) and ends with `0x01E5CDEC(0)`. One synchronous key/send/unkey
  sequence using stock's own checks. Reached indirectly (no direct caller;
  likely the Bluetooth KISS data handler). Never exercised by us.
- RX while a module runs: the APRS modem decodes the receiver's audio on the
  working frequency (`0x11C8C`, 10 Hz units). Dual Watch alternation
  appears to be driven by the stock loop, which a module pauses, so the
  receiver likely stays on the frequency active at entry. The APRS list
  footer now shows that frequency. Not yet confirmed on hardware.

## 2026-11-10 APRS SEND: first transmit proof of concept (offline, not yet run)

- User confirmed a 2 m licence and that the callsign the MSG screen reads
  (RAM `0xC4A1`/SSID `0xC4A7`) is theirs; asked for canned messages first,
  keypad entry after the proof of concept.
- New hidden module `.APRS SEND` (4,133 B), reached by holding Menu on
  APRS MSG (hold cycle LIST → MAP → MSG → SEND → LIST). Items: status
  `>H9 Toolbox test`, and three canned messages to the station selected in
  the list (`:ADDRESSEE:text{id}`, id 1–99 counting per send).
- Frame: `CALL-SSID>APZH9T,WIDE1-1` UI/`F0`, built by `aprs_tx.c` as one
  KISS data frame, then handed to stock `0x01E5DBCC(buf, len)` (stock does
  the PTT handler, modem send, airtime wait and end of TX). The module never
  writes RF or PTT state.
- Guards before transmit: callsign present and valid AX.25, working TX
  frequency `0x11C9C` within 144.000–148.000 MHz, and a review screen
  needing a second Menu press.
- The SEND build leaves out receive parsing (`APRS_NO_RX`, via
  `aprs_core_tx.c`); packets heard meanwhile stay in the 2 KiB modem ring.
- Host test: KISS bytes match the `test/kiss-tnc.test.mjs` vector
  (`N0CALL-1>APRS:>Hi`); path, SSID and rejection cases; a frame fed back
  through the receive parser yields the sending station.
- Unknowns for the first run: whether `0x01E5DBCC` behaves when called
  from a module (stock loop paused, my_app task stack: about 0.9 KiB of
  frames under DBCC plus the PTT handler's own); whether the PTT handler
  draws stock UI; whether the modem accepts the frame while the
  radio is in this state. Catalog: APRS 0.3.
- Follow-up: user reports holding Menu on MSG does not open SEND (v0.3 bundle
  was fetched by the App Manager at 13:07; cause unknown). SEND is now the
  visible module "APRS SEND" (listed in the module menu, which also shows
  whether it is installed). MSG hold and the new Menu-on-details entry show
  "SEND NOT INSTALLED" instead of falling back silently. INFO Menu opens
  SEND with the first message to that station selected.

## 2026-11-10 First H9 Toolbox APRS transmission; channel settings; sent list

- Hardware: APRS SEND transmitted. aprs.fi 2026-11-10 13:16:44 CST:
  `KD9CRQ>APZH9T,WIDE1-1,qAR,N6NKO-1::N9LKH-1  :Test, please ack{1`
  (digipeated and iGated). Stock's TX screen flashed over the module while
  transmitting, then the module redrew. So stock `0x01E5DBCC` called from a
  module keys, sends and unkeys correctly.
- It transmitted (and the module listened) on CH-A although the user's APRS
  settings are CH-B: a module pauses stock and nothing applied the APRS
  channel. Now, as stock's beacon sender `0x01E5C3D4` does: set `0x11B50`
  bit 7 and `0x11AEC` to the side, call RF setup `0x01E58E3E(0)` (only
  when the side changes).
  - Opening APRS fresh: save the side, tune to APRS RX CH (`0xC532`:
    0 Off = keep, 1 A, 2 B). Back from the list to the menu restores it.
  - SEND review: tune to APRS TX CH (`0xC533`: 0/2 A, 1/3 B, 4 A+B =
    keep; Lock is treated like Auto) before the 2 m check reads `0x11C9C`;
    leaving review returns to the RX side.
- Sent messages are added to the MSG list (from own call, not "new"); the
  digipeated copy is deduplicated by sender and text.
- Size: `aprs_wrap` moved to `aprs_text.c` (not linked into list/map).
  Now LIST 6,487, MAP 6,467, MSG 6,495 of 6,496 B: the next feature on those
  screens needs the shared state moved or the screens split further.
- Host tests: side selection for all RX/TX CH values, RF setup only on a
  change.

## 2026-11-10 APRS: open squelch and mute the speaker while the app runs

- User: reception is much better with squelch open. Cause (static):
  stock's receive/squelch handler `0x01E7B2A4` runs from the main loop
  (`0x01E88E36`), choosing AF mode 0/1 (`0x01E593E4`, RF reg `0x47`) and
  speaker gate source 7. A module pauses that loop, so receive audio to
  the APRS chip stays as it was when the app opened.
- `aprs_quiet()`: AF selector mode 1 (stock receive-setup value) and stock
  speaker writer `0x01E52328(0)` (GPIO 3 low; bench-proven 2026-11-03 to
  silence the built-in speaker, Bluetooth audio unaffected). Applied on a
  fresh open, after each side change (RF setup) and after each transmit
  (stock's end of TX turns the speaker back on). Back from the list
  restores the speaker state saved at open (`0x11B4D` bit 0).
- Not yet known: whether the RF chip's own squelch also gates its AF output
  (then the SQL setting would still matter). Hardware check pending.
- Space: receive frame buffer 330 → 200 B (longer packets keep their first
  200 bytes), shared slot 0x680 → 0x600, magic "APR2" for the new layout.
  Screen limit 6,624 B; LIST 6,539, MAP 6,507, MSG 6,535, INFO 6,353,
  SEND 4,493.

## 2026-11-10: module-loader release prep (local, not flashed)

- Stub `ml_reset` (menu returns 0, or no valid bundle) now uses the stock
  cpu_reset sequence that the menu's RADIO item proved on hardware: ROM
  0x02001634 (interrupts off), `[0x1E0000] |= 0x10`, spin. The old jump to ROM
  0x0200000A froze. New build: package c1a6f78b…, flash 08d2409a….
  Hardware check pending; `build/test-ml-reset-bundle.bin` (a "menu" that
  returns 0 at once) exercises this path from a Back hold.
- Web installer now ships this build. Seeds rebuild and recognise stock, the
  Brick Breaker build of H9 Toolbox (83f79fb3…), the original Brick Breaker
  (741bbeb8…) and the module-loader preview (79858310…; 3 sectors: 0x5000,
  0xac000, 0xd9000). Pinned write set 0x5000, 0x62000, 0x64000, 0x8d000,
  0xaa000, 0xac000, 0xae000, 0xaf000, 0xd9000. Step 04 and the result screen
  send the user to the App Manager.
- Hardware, new build (08d2409a…) installed through the local installer:
  Back hold opens the menu; RADIO restarts into the radio (expected: the
  menu's h9_reset). The `ml_reset` test bundle is still to run.

## 2026-11-10 APRS: squelch open needs RF register 0x30 (audio DAC)

- Hardware (speaker-on test build): no hiss with AF mode 1 + speaker on.
  With SQL set to 0 in stock the hiss was there on opening APRS, and went
  away after an APRS SEND transmit.
- Cause (static): RF register 0x30 bit 9 is the RF chip's audio DAC enable.
  Receive setup (`0x01E5942E`, run after every TX and RF setup) writes
  0x30 = 0xBDF1 (off); the squelch handler writes 0xBFF5 on open
  (`0x01E7B7C6`, right after AF mode 1) and 0xBDF5 on close (`0x01E74104`).
  RF register write routine: `0x01E5070A(reg, value)`.
- `aprs_quiet()` now also writes 0x30 = 0xBFF5 (APRS 0.4). Hardware check
  pending with `build/test-aprs-speaker-bundle.bin` (speaker left on).
- Hardware (0x30 fix, speaker-on test build): hiss after an APRS SEND, but
  not on opening APRS; and opening APRS jumped straight to INFO of the first
  station (a Menu tap) without a key press.
- Keypad (static): `h9_key` = stock `0x01E515AC`, one ADC sample of two
  resistor-ladder channels (13 and 10) matched against voltage windows, no
  debounce. Likely cause: speaker amp + hiss noise gives a one-sample Menu
  reading = a tap. Fix: `aprs_event` takes a key only when two readings in a
  row agree (APRS state "APR3", new `raw_key`).
- Audio on open: something after open turns the DAC off again (cause not
  traced; receive setup writes 0x30 = 0xBDF1). Fix: `aprs_event` reads 0x30
  (`0x01E505FE`, RF register read) about every 0.32 s and calls
  `aprs_quiet()` if bit 9 is clear. Screen sizes now: LIST 6,599 (limit
  6,624), MSG 6,595, MAP 6,567, INFO 6,413, SEND 4,545.
- Hardware (debounce + keep-open build): still no hiss on open, still opened
  INFO. Cause of no audio (static): battery save. `0x01E7BAF8` sets
  0x11B01 = 1 and powers the RF chip down (0x30 = 0, 0x37 = 0x1D00, RF GPIO
  0x33 bits); `0x01E5969C` wakes it (clears 0x11B01, 0x37 = 0x1D0F, then
  receive setup). The squelch handler wakes it on activity; paused under a
  module, so APRS opened during a sleep phase stays deaf (likely also the
  long "RX 0"). A transmit re-initialises the chip, hence hiss after SEND.
  Fix: `aprs_quiet()` wakes first when 0x11B01 is set; the periodic check
  catches it via 0x30 bit 9.
- Keys at open: now ignored until every key reads released (KEY_WAIT),
  instead of trusting one sample. Footer RX frequency no longer turns yellow
  out of band (space). Sizes: LIST 6,615 / 6,624, MSG 6,623, MAP 6,595.
- Hardware (battery-save wake build, speaker on): hiss on opening APRS. Wake
  fix confirmed. Still opened INFO: LOADING, then WI9HRO-1's details, no list
  seen.
- Keypad scan `0x01E515AC`: ADC channel 13 reading < 21 = key 12 (Menu).
  So a near-0 glitch (likely when the speaker amp starts) is Menu, and the
  two-sample debounce did not filter it. Now: a Menu tap counts only after
  3 ticks (30 ms) down; the two-sample check and `raw_key` are removed
  (state layout back to "APR2"). Sizes: LIST 6,611, MSG 6,619 (limit 6,624).
- Hardware (30 ms tap build): tapping APRS in the menu still opens INFO.
  Holding Menu on APRS: LOADING + hiss, no list until Up/Down. Reading:
  right after the battery-save wake the keypad reads Menu for > 30 ms
  (absorbed by KEY_WAIT when Menu is still held), and the first list draw
  does not reach the screen. Fix: 300 ms `h9_delay` after waking on a fresh
  open, before the first draw and key reads. MSG "SEND NOT INSTALLED" →
  "NO SEND APP" for space. Sizes: LIST 6,619, MSG 6,619 (limit 6,624).
- Hardware (300 ms settle build): a tap on APRS now stays on LOADING (no
  INFO jump, but no list drawn). User suggestion: draw before waking.
  Change: a fresh open only marks `switching = TUNE`; `aprs_event` returns
  the first redraw, then on its next call switches to the APRS RX channel,
  wakes/opens audio (`aprs_quiet`), settles 300 ms and waits for all keys
  released. The periodic 0x30 check is removed (battery-save sleep was the
  cause and is handled in `aprs_quiet`; it also turned interrupts off ~3x/s).
  Sizes: LIST 6,623, MSG 6,623 (limit 6,624): APRS screens are full.
- Hardware (draw-first build): tap or hold on APRS → LOADING stays, hiss
  starts at once. So the first draw ran but nothing was drawn: not the wake.
- ROOT CAUSE (bug, mine): `aprs_load_stored` read each 324-byte Beacon List
  record into `A->frame`, cut from 330 to 200 B earlier on 2026-11-10. Each
  stored station overran 124 B into `length/escape/nmea_len/nmea/rx_flash/
  last_key/held/press_tick/dirty/list_sel…`: the redraw flag cleared (no
  list) and key state faked a Menu tap (INFO of the first station). Showed
  up once stock had stored stations (WI9HRO-1). Not caught by UBSan (inside
  one struct). Fix: read the 2-byte length, then only addresses + info cut
  to fit (info ≤ 141 B); host test checks the following fields survive.
- Reverted the draw-first tuning and 300 ms settle (not needed); kept
  KEY_WAIT and the 30 ms Menu tap minimum. The keypad-ADC/wake theory is
  unconfirmed. Sizes: LIST 6,607, MSG 6,583, MAP 6,563, INFO 6,409.
- Hardware CONFIRMED (overrun fix, speaker-on test build): APRS opens on
  the list with stored stations and hiss; navigation normal.
- Hardware (normal bundle, APRS 0.4): APRS SEND transmits on CH-B (APRS TX
  CH setting followed); menu USB UPDATE works; boot logo write from the App
  Manager works.

## 2026-11-10 APRS SEND: typed recipient and message (APRS 0.5)

- Keypad codes (handbook): digit d = key d + 2. `aprs_event` now returns
  `EV_DIGIT + d` on a digit press; other screens ignore it.
- SEND items: TO (typed; starts as the selected station), MSG (typed), SEND
  MESSAGE, STATUS test, two QUICK texts that fill MSG. Multi-tap entry in
  `aprs_edit.c` (SEND only): 2 = ABC2 … 9 = WXYZ9, 0 = space 0,
  1 = .,?!-/@1; same digit within 1 s cycles, Back deletes, Menu or Up/Down
  finish. TO takes A-Z, 0-9, '-', max 9; MSG max 39 (the MSG list's width).
  Draft on SEND's stack (lost when leaving SEND); MSG cleared after a send.
- Host tests: digit event, multi-tap cycling/timeout/delete/full/callsign
  filter. Sizes: SEND 5,335; LIST 6,619 (limit 6,624). Hardware: pending.
- Hardware (APRS 0.5): typed send transmitted; aprs.fi 2026-11-10 16:59:57
  CST `KD9CRQ>APZH9T,WIDE1-1,qAR,N6NKO-1::QRX      :R{1` (addressee "QRX",
  text "R"). Whether that matches what was typed: asked.

## 2026-11-10 SAT PASS: FM satellite pass predictor (module, 0.1)

- New app `module-loader/modules/satpass/`: the next five passes of SO-50, ISS,
  AO-91, AO-27, PO-101, AO-123 and LilacSat-2 over the GPS position, in UTC.
  Time and position come from `$xxRMC` in the APRS ring (KISS cmd 0x10); the
  module consumes the ring itself (advances `0x16E36`). Until the GPS reports,
  it starts from APRS's saved fix (`struct aprs` `my_lat`/`my_lon`/`my_fix`,
  magic checked).
- Orbit model (`orbit.c`): two-body Kepler from TLE mean elements (SGP4
  un-Kozai mean motion and semi-major axis), secular J2 rates for RAAN,
  perigee and mean anomaly, the ndot/2 term, and IAU 1982 GMST. Integer only:
  binary angles, Q30 CORDIC sin/cos/atan2 (24 steps), km Q8, no 64-bit
  division (only `__divdi3` would be a library call). Search: 30 s steps over
  36 h, bisection of AOS and LOS to 1 s, ternary search for the peak.
- Embedded elements: `satpack.py <tle>` writes `sats.h` (elements of
  2026-11-10 from CelesTrak's amateur group).
- Accuracy (`test_satpass.py`, against Python sgp4, 5 places, 36 h): worst
  AOS/LOS error 19 s at 0.5 days after epoch, 18 s at 7 days, 25 s at 14 days;
  peak elevation within 2.5 degrees; no missed passes.
- Size: 6,593 B, under the 6,624 B limit so APRS's saved state (and its last
  fix) survives. A version using the whole slot was 6,537 B; staying under the
  APRS state cost the "(GO OUTSIDE)" hint.
- Hardware: not yet flashed. Unknown: how long the 36 h search takes on the
  radio (about 30,000 position evaluations at most).

## 2026-11-10 Stock tuning path (static) and VFO PROBE (for SAT PASS tuning)

- Goal: let SAT PASS put satellite uplink/downlink frequencies into the
  VFOs. Static analysis of stock 1.0.38 (`td-h9-open-firmware`
  `artifacts/main/disassembly/instructions.jsonl`, same app.bin `11a07bc0…`):
  - 282 calls to the RF register write `0x01E5070A`; registers `0x38`/`0x39`
    (frequency low/high, 10 Hz units; the register map matches the Beken
    BK4819 family) are written only at `0x01E590D2`/`0x01E590DA`, inside
    **`rf_set_freq` `0x01E59094`** (r0 = 0 receive / 1 transmit,
    r1 = frequency, r2/r3 = tone, stack arg). It adds a per-band calibration
    from the halfwords at `0x11AD0 + 0x334*2` / `0x333*2` (÷10), chosen by
    the band byte `0x11AD0+0x10CD` (set elsewhere to 0 at ≤ 299.99999 MHz).
  - Callers: **receive tune `0x01E5942E(ch)`**: `ch` = pointer to a
    channel-like struct (+0 u32 frequency, +4 byte, +6 u16 tone, +8 byte,
    +0xC word, +0x10 byte), or 0 for the live state: frequency
    `0x11AD0+0x1BC`, tone `+0x98`, bytes `+0x17`, `+0x18`, `+0x77A`.
    `0x01E595F8` tunes from a table at `0x01EB3214` indexed by `+0x2A`
    (probably a fixed channel list). **Transmit tune `0x01E5AD68(ch)`**:
    frequency from `ch` or from `+0x1BC` / `+0x1CC` (chosen by `+0x10EA`),
    passed through `0x01E56AE2(freq, dir)`, which adds/subtracts the offset at
    `+0x1C8` by direction byte `+0x789` (1 = minus, 2 = plus) unless the flag
    at `+0x1B` is set.
  - `0x01E56B08` swaps `+0x1BC` and `+0x1CC` (through `+0x1C4`) when
    `+0x1D` is set: probably the reverse function.
  - [HYP] `+0x1BC` is the active receive frequency and `+0x1CC` the transmit
    base; where VFO A and B keep their own frequencies is not yet known.
- `modules/vfo_probe/` (VFO PROBE 0.1, catalog Experimental, 2,177 B): read-only.
  Page 1 shows those fields; Menu scans RAM `0x100–0x29EFF` for aligned
  words in 136–174 / 400–520 MHz and lists addresses. Hardware: pending
  (set A = 145.800, B = 435.000, open the probe; then switch A/B and repeat).

## 2026-11-10 Background GPS recorder in the loader stub; stock GPS state (static)

- Stock's NMEA parser (CPU `0x01E86780…`, uses the name table at
  `0x01EB16A0`: GPGGA/GNGGA/GPRMC/GNRMC/GPGSV/BDGSV) keeps the fix in the
  `0x11AD0` block: `+0xA80` (`0x12550`) valid byte (GGA quality > 0, RMC
  status 'A'), `+0xA81` quality, `+0xA83` satellites, `+0xA88` (`0x12558`)
  latitude and `+0xA90` (`0x12560`) longitude as **signed microdegrees**
  (converter `0x01E54450`: deg×1e6 + min×1e6/60, negative for S/W),
  hemisphere letters at `+0xA8C`/`+0xA94`, date at `+0xAA4` (u16 year, month,
  day) and time at `+0xAA8` (hour shifted by the timezone setting). [STATIC]
- Neither the main app nor `APRS-1.0.15.bin` contains GPS receiver commands
  ($PCAS/$PMTK/UBX): no known way to aid the receiver itself.
- Stub (`src/stub.s`, 608 → 960 B): `rec_tick` runs on every my_app loop
  before the stock loop call. With a valid fix more than 100,000 µdeg from
  the last saved one (either axis), it appends a 16-byte record ("GFX1", lat,
  lon µdeg, u16 day 0, u16 check) to resource-chip sector `0x3FF000` through
  stock erase `0x01E50000` / program `0x01E5006E` (the S2 upload routines),
  erasing only when all 256 records are used. RAM: the run-next mailbox's
  spare words `0x2BEF4` (magic "GFCR", last saved lat/lon), rebuilt from the
  chip when the magic is missing. Disassembly of the built stub checked
  (compares unsigned, shifts zero-fill).
- Game slot's voice area now ends at `0x3FF000` (`dist/doom/upload.mjs`).
- Modules: `common/fix_store.c` reads the newest valid record (torn records
  skipped); host test `common/test_fix_store.c` builds records as the stub
  does. SAT PASS reads it at start (6,457 B; its own save code was dropped,
  the stub is the only writer); VFO PROBE 0.1 shows stock's fix (valid, lat,
  lon) on page 1.
- SAT PASS size work: midpoint peak (was a ternary search), one shared epoch
  in the table (satpack.py moves each satellite to it), elements date in the
  header. SGP4 check after: AOS/LOS ≤ 25 s, peak ≤ 5.3° at 14 days.
- Build: package `dceac70c…`, flash `2f5e428d…` (differs from the hosted
  first release `08d2409a…` in `0x5000`, `0xAC000`, `0xD9000`). Web
  installer moved to it (`release_update.mjs`; first release kept as
  PREVIOUS `release1`). Not flashed; hardware pending.
- Installer bug (user report, first install attempt of `dceac70c…`): after a
  backup of a radio running the first release, step 02 said the firmware was
  not recognised. The backup scan stores the matched PREVIOUS id
  (`release1`), but `validateSelectedBackup` only accepted the literal
  `'previous'`, so no earlier build ever got the rebuilt-stock restore point.
  Fixed (`previousInfo(createdBackup.source)`), `update.js?v=20261110d`.
  Nothing was written to the radio.

## 2026-11-10 GPS app (GPS 0.1) and SAT PASS 0.2 diagnostics

- User report after the single-flash build: GPS lock in stock, then SAT PASS
  showed no fix. Cause not found yet (candidates: framing of the forwarded
  NMEA, stock's own reader sharing the ring, stock's periodic ring reset in
  the dispatcher at `0x01E87318`: resets `h[0x16E34]`/`+2`/`b[+4]` through
  `0x01E85766` when a timer compare at `0x11AD0+0x228` expires). SAT PASS
  0.2 shows `L<lines> R<rmc> S<status>` on its bottom row to tell them apart.
  Hardware: pending.
- New app `module-loader/modules/gps/` (GPS 0.1, 6,247 B, catalog Radio):
  four pages from the ring's NMEA (GGA, RMC, GSV): POSITION (lat/lon in
  degrees, altitude m/ft, speed km/h and mph, course with 16-point compass,
  satellites used/in view, HDOP, quality, Maidenhead grid), SKY (azimuth/
  elevation plot, colour by SNR), SIGNALS (SNR bars by constellation), INFO
  (UTC date/time, the chip's saved fix, sentence counts, last RMC age).
  Redraws a page's text each second and the plots when the satellite table
  changes (at most once a second).
- Host test `modules/gps/test_gps.c` (ASan + UBSan; sample GN/GP/GL
  sentences framed as the modem does): found a unit slip (NMEA minutes to
  microdegrees: x 5/3, not x 50/3); now checks Chicago (EN61EV) and Sydney
  (QF56OD) grids, hemispheres, status V keeping the last position, GSV
  replacement per constellation, and torn input.
- `fix_store`: `fix_load_micro` (microdegrees) replaces `fix_load`;
  `fix_bam` converts for SAT PASS (6,613 B, still under 6,624).
- Not yet known on hardware: whether the ring carries GGA and GSV as well as
  RMC (the monitor only counted lines), so the SKY / SIGNALS pages may stay
  empty. The INFO page's RMC / GGA / GSV counts answer that.

## 2026-11-10 GPS ring: RMC 0, GGA 120, GSV 0 (hardware, user)

- GPS 0.1 on the radio: GGA counted up (altitude correct), RMC and GSV
  stayed 0, so no position / satellites. Stock's cmd `0x10` handler
  (`0x01E86FF4`) assembles lines from the payload byte by byte (`$` start,
  `\n` end, 0x3FF limit) and feeds each to the NMEA parser (GGA, RMC, GSV
  all handled), so the sentences exist in stock's stream.
- Likely cause (not yet confirmed): our modules buffered each KISS frame in
  100 bytes (SAT PASS) / 200 bytes (APRS) and parsed it afterwards. If the
  modem batches the second's sentences into one frame, only the first (GGA)
  survived the cap. GPS 0.2 and SAT PASS 0.3 now stream the payload through
  the line assembler byte by byte (no frame buffer; host test feeds GGA + GSA
  + RMC + 2 GSV in one frame, 300+ bytes). APRS (`aprs_core.c`, 200-byte
  frame) still buffers and may drop position the same way.
- GPS 0.2 also takes position, time and fix from GGA when RMC is absent, and
  its INFO page lists the distinct sentence names seen and the longest frame,
  so the next hardware run shows what the modem really sends.

## 2026-11-10 SAT PASS 0.4: horizon plot of a pass; local time from stock's Time zone

- User: SAT PASS works after the streaming fix. New hidden module
  `.SAT PLOT` (`modules/satpass/satplot.c`, 4,405 B): from SAT PASS's detail
  view, Menu again draws the pass on a polar plot (north up, rim = horizon,
  centre = zenith; rings at 30/60 degrees; path sampled 48 times with
  orbit.c; rise green, set red, highest point yellow; rise/set local time
  and azimuth, peak elevation). Any key returns to SAT PASS, which restores
  time, selection and detail view from `satshare.h` (32 bytes at
  `0x2BEE0 - 0x600 - 0x20`, just below APRS's state; both SAT screens now
  stay under 6,592 B, checked by build.py). SAT PASS 6,513 B (dropped the
  GPS diagnostic row, "AT time" and "IN ..." from the detail view).
- Host render (`modules/satpass/render_plot.c` to PPM): PO-101 over Chicago
  rises az 176, sets 343, peak 53 degrees, matching the pass list and SGP4.
- Local time: stock's Time zone setting (settings map `0x3070`, options
  UTC-12..+12) is applied by stock's NMEA code to the GPS hour from the byte
  at `[0xC0F0 + 0xD6]` = RAM `0xC1C6`, minus 12 (CPU `0x01E86A76`). SAT PASS,
  the plot and GPS 0.3's INFO page add it to UTC (whole hours; `L` after a
  local clock, `Z` for UTC+0). That RAM address comes from the disassembly
  only: [STATIC], hardware check pending (a wrong value shows as UTC).
- Sizes: GPS 6,621 B (limit 6,624). Firmware package unchanged (`dceac70c…`).

## 2026-11-10 SAT PASS 0.5: page tabs

- User: the plot is "money"; make the second page visible. Every SAT PASS
  screen now draws a tab row at the bottom (`sat_tabs` in `satshare.h`):
  `LIST DETAIL PLOT` with the current page in yellow; Menu goes to the next
  page, Back to the previous; the plot shows `KEY<` (any key returns).
  Size: SAT PASS 6,573 B (limit 6,592; dropped the unused "NO GPS" state,
  the position restore and a hint to fit), `.SAT PLOT` 4,501 B. The plot moved
  up 4 px (centre y 64) to clear the tab row. Hardware: pending.

## 2026-11-10 SAT PASS 0.6: Up/Down steps through passes in the plot

- User: "allow up/down buttons in plot to cycle through next passes".
  `satshare.h` now carries the whole list (rise times and a packed
  duration + satellite index per pass, 64 bytes at `0x2BEE0 - 0x600 - 0x40`);
  `.SAT PLOT` redraws the plot for the next / previous pass on Up / Down
  (shows `n/5`) and Menu or Back returns, leaving SAT PASS on the pass last
  viewed. To make room the DETAIL page is gone (the plot shows the same rise,
  set, azimuth and peak data): tabs are now `LIST PLOT`. Sizes: SAT PASS
  6,233 B, `.SAT PLOT` 4,701 B (limit 6,560, set in build.py).
- Host render (`modules/satpass/render_plot.c`, scripted keys d,d,u,menu over
  five Chicago passes): shows pass 2/5 (SO-50, 4 degrees, low in the north) and
  returns to SAT PASS with that pass selected. Hardware: pending.

## 2026-11-11 Web installer: stock-compatible radios (user report)

- Report (a user who had just updated 1.0.33 -> 1.0.38): the backup step
  stopped with "Radio is neither exact stock v1.0.38, H9 Toolbox Firmware,
  nor an earlier H9 Toolbox build. No backup seed accepted." The radio is
  probably stock apart from sectors the official update left as they were
  (not confirmed: we have no scan from that radio).
- The error now names the closest known image and the sectors that differ
  (`describeMismatch`). The installer is also more permissive:
  `stockCompatible` accepts a radio (or a backup file) that is exact stock
  v1.0.38 in every sector outside `BUILD.sectors`, the only sectors an
  install writes; whatever those hold is replaced, so the result is still the
  exact package. Such a radio is backed up in full (no known-image shortcut,
  so slower) as source `stock-like`, and its own backup is the restore point.
  A user-selected backup file only has to be stock-compatible too (was exact
  stock SHA-256). A difference anywhere else still stops with the sector list.
- Tests: pinned-only differences pass, a stray sector (0x30000) fails, a
  leftover in the last pinned sector is fine. Hardware: untried (no radio in
  that state here).
- Follow-up (user): the backup step no longer refuses a radio that is not
  stock-compatible: it takes the full recovery anyway (read only; source
  `other`, file `TD-H9-Unrecognised-HID-recovery-<date>.h9hid`) and keeps
  Install locked (the pinned sectors are patches of stock 1.0.38). Note: for a
  radio with no known-image shortcut the recovery returns the whole accessible
  flash including the settings area `0xDA000-0xFFFFF` (v1 archive), so that
  file holds the user's channels and settings: not for sharing as-is. The
  shortcut backups of known images stay firmware-only.
- Follow-up (user: "instead of all 213, rewrite the ones that differ"): a
  radio that is not stock-compatible but was just backed up in full (source
  `other`, the user's own recovery, boot region checked against the target's
  as before) now unlocks a **full-image install**: `planUpdate` already lists
  exactly the sectors where the backup differs from the package, so only
  those are written (the pinned-sector limit is lifted for this case only),
  each verified, then a final CRC check of every sector; the interrupted-sector
  resume and Restore (the backup) work as before. The page states the sector
  count before writing. Guard added because the stock-compatibility check no
  longer implies the model: the recovered firmware must contain the name
  "TD-H9" (`looksLikeH9`: the flash is decrypted block by block with the
  CRC stream `0xF181 ^ ((flash - 0x5000) >> 2)`, found at app+0xB162A in
  v1.0.38; the same keystream as the handbook's flash encryption, proven here
  by decrypting the string). A user-selected backup file still has to be
  stock-compatible. Tests: plan lists the two non-pinned sectors a changed
  backup differs in; `looksLikeH9` true for stock, false for zeros.
  Hardware: untried (no unrecognised radio here); the commenter's radio is
  the first candidate.
- Follow-up (user: what if he refreshes and then cannot restore?): the
  freshly taken backup lived only in page memory, and a selected file had to be
  stock-compatible, so after a refresh an unrecognised radio's own `.h9hid`
  was refused. Now any backup file that is a TD-H9 application is accepted in
  full-install mode, and what makes it safe is the check right before writing:
  every sector of the radio must equal the backup or the new firmware (one
  interrupted sector allowed), otherwise nothing is written (the message names
  the mismatching sectors). So the saved `.h9hid` restores and resumes after a
  refresh, an old or foreign file is refused, and a half-installed radio
  resumes (its sectors are backup-or-package). The file stays the only copy:
  keep it (the page does not store the backup itself).

## 2026-11-11 SAT PASS 0.7: TRACK page with Doppler-corrected frequencies

- Next step towards "SAT PASS sets VFO A/B": the satellite frequencies and
  the Doppler maths, shown on a third page so the radio can be tuned by hand
  now. `satpack.py` embeds each satellite's downlink, uplink and access tone
  (`struct satfreq` in `sats.h`): SO-50 145.850 up / 436.795 down, 67.0 Hz
  (arm with 74.4 Hz first); ISS 145.990 / 437.800, 67.0; AO-91 435.250 up /
  145.960 down, 67.0 (AMSAT's April 2025 page says no tone needed); AO-27
  145.850 / 436.795; PO-101 437.500 up / 145.900 down, 141.3; AO-123 145.850
  up / 435.400 down, 67.0; LILAC2 144.350 / 437.200 from memory, not in
  those lists. Sources: amsat.org (status pages, ?p=5012, ?p=62395),
  work-sat.com FM-VOICE-SATS-2026a, REF QRG list 2025-06-26. Several of
  these satellites are intermittent or silent: check amsat.org/status.
- `orbit_look` also returns the slant range (CORDIC magnitude); `doppler.h`
  turns the range change over one second into a shift (f x rate / c as one
  multiply and a shift). `.SAT TRACK` (4,877 B): once a second, time to rise
  or set, azimuth, elevation, range, LISTEN = downlink - shift, SEND = uplink
  + shift, tone. Before the rise it shows the values at the rise. Navigation:
  Menu LIST -> PLOT -> TRACK -> LIST; Back to the list; Up/Down change pass.
- Checks: `test_satpass.py` compares range and Doppler (at 437.8 MHz)
  against SGP4 over a day at three places: worst 17 km / 64 Hz at 0.5 days,
  29 km / 123 Hz at 7, 49 km / 545 Hz at 14 (gated within a week). Host
  render of a PO-101 pass 400 s after the rise: receding, listen 145.8985
  (-1.4 kHz), send 437.5044 (+4.4 kHz), as expected.
- Still needed to set the VFOs themselves: the VFO PROBE hardware run
  (where A and B keep their frequencies), then a tune-only test.

## 2026-11-11 VFO PROBE on hardware: where VFO A and B live

- Photos (A = 145.800, B = 435.000): with A selected `+0x1BC` = `+0x1CC` =
  145.80000, `+0x1C8` offset 0, direction `+0x789` 0, band byte `+0x10CD` 0;
  scan hits at `0x11C8C`, `0x11C9C` (working copy), `0x11CA0` / `0x11CA4`
  (145.800) and `0x11CA8` / `0x11CAC` (435.000). With B selected `+0x1BC` =
  `+0x1CC` = 435.00000, `+0x1D0` still 145.80000, `+0x10CD` = 1. So VFO A =
  `0x11CA0` (`+0x1D0`), VFO B = `0x11CA8` (`+0x1D8`); the next word of each is
  probably that side's TX frequency; the working copy follows the selected
  side. Matches the static trace: stock's VFO stepper `0x01E67B94` steps
  `+0x1D0` (side A) or `+0x1D8` (side B), copies it to `+0x1BC` and calls
  `0x01E595B6` (receive tune from `+0x1BC`). [HW + STATIC]
- Stock GPS row read `0 0.000000 0.000000` while GPS 0.2 and SAT PASS had a
  fix at the same place. A module runs with stock's main loop dropped, so
  stock's parser state is whatever it held when the module started; either
  stock had no fix then, or `0x12550/58/60` are wrong. Not settled. Because
  the stub's background recorder depends on those, SAT PASS 0.8 now saves the
  first good fix of a session itself (`fix_save`, same record format, erase
  only when the sector is full; `fix_store_save.c` so GPS keeps its size).
- Design note for setting VFO A/B: a module cannot do it (stock is not
  running: no PTT, no squelch, and RAM reverts at RADIO's reset). Plan: a
  resident tracker called from the stub's per-loop hook that writes the
  Doppler-corrected downlink to `+0x1D0` and uplink to `+0x1D8`, copies the
  active side to `+0x1BC` and calls `0x01E595B6`. Needs a stub change.

## 2026-11-11 Resident VFO tracker (SAT PASS 0.9, firmware build 357289b9, not flashed)

- Goal (user): SAT PASS sets VFO A / B with the satellite's downlink / uplink.
  A module cannot (stock's loop is stopped while it runs: no PTT, no squelch,
  and the RAM words revert at RADIO's reset), so the module stays armed while
  the stock radio runs.
- Stub (`src/stub.s`, 960 -> 1,120 B): `res_tick` runs next to `rec_tick` on
  every my_app loop. While `satshare.res_magic` ("CSRT", RAM `0x2B8B4`) is
  set it calls the armed module as `entry(&ml_api, 0xFFFF)` twice a second,
  after checking its entry is in the RAM slot and the slot CRC once (length
  and CRC come from the bundle table); any failure disarms. `ml_main` (hold
  Back) disarms it before the menu replaces the slot. Disassembly of the built
  hook matches the source (branch targets checked). Nothing changes while
  disarmed. Assumption: RAM survives `cpu_reset`; if it does not the magic
  is gone and nothing happens.
- `.SAT TRACK` (6,069 B): key 5 opens a confirmation (A = downlink; B = uplink
  on/off, default off; key 1 = test now, listen only, any time), arms the
  tracker and resets the radio. `tracker()` (host test `test_sattrack.c`, 9
  cases): rebases the clock when the tick restarted (+4 s boot guess), picks
  the first unfinished pass, waits until 60 s before the rise, computes the
  Doppler-corrected frequencies, and when one moved by 200 Hz or more writes
  VFO A (`0x11CA0`) / VFO B (`0x11CA8`); for the selected side (`[0x11B50]`
  negative = B) also the band byte `+0x10CD`, the working copy `+0x1BC`, and
  calls `0x01E595B6` (tune receive). It skips everything while RF register
  `0x30` bit 14 is set (stock writes `0xC1FE` for TX, receive values have it
  clear), disarms after the last pass, never writes flash, and sets the UI
  flag `0xC136` = 1 after a change (that flag's meaning is a guess from the
  stepper's tail: check it does not cause settings writes).
- Shared state moved/extended: `satshare` is 96 bytes at `0x2B880` (build.py
  limit 6,528 B for the SAT screens). VFO PROBE 0.2 page 3 shows magic, entry,
  CRC, calls, flags, the last A/B written, `+0x80` (side), RF register `0x30`
  and `+0x24C`.
- Installer: new build package `357289b9…`, flash `6663c809…` (3 sectors differ
  from release2: `0x5000`, `0xAC000`, `0xD9000`); release2 (`dceac70c…`,
  image `2f5e428d…`) kept as PREVIOUS.
- Hardware plan: flash; update SAT PASS and VFO PROBE; VFO PROBE page 3 with
  A then B selected (side byte, REG30 in receive); SAT PASS > pass > TRACK >
  key 1 (test now): VFO A should jump to the rise downlink within seconds;
  hold Back, VFO PROBE page 3 CALLS > 0 shows the RAM survived the reset;
  then a real pass, B off; B on only after a dummy-load check.

## 2026-11-11 VFO PROBE page 3 on hardware (new firmware flashed, tracker not armed)

- User photos (two power-ups, "A then B VFO"; which photo is which side is
  not certain from the pictures): both show MAGIC 0 (not armed) and
  garbage in the status words (CALLS 4071623012, FLAGS 91, ...): power-up SRAM
  is random and never zeroed there. The garbage differs by 2-3 bits per word
  between the photos (about 8 percent), the signature of SRAM start-up noise
  across a power cycle, so these photos say nothing about whether RAM survives
  `cpu_reset` (still the key open assumption).
- `+0x80` read `0x80` in one photo and `0x00` in the other. Whether negative
  means B (as the stepper's branch reads) or A is unconfirmed, so the tracker
  no longer trusts it: the selected side is the one whose word equals the
  working copy `+0x1BC` (the side byte only breaks a tie).
- RF register 0x30 read `0000` in receive: either that register reads back 0
  through `0x01E505FE` from a module or the read is not valid there, so the
  transmit guard (bit 14) cannot be trusted. `+0x24C` was `01` while idle, so
  it is not a transmit flag either.
- Changes (modules only, firmware `357289b9` unchanged): the tracker retunes
  only VFO A (the receive side); for B it only updates the word and, if B is
  selected, the working copy `+0x1BC` and band byte, which stock turns into the
  transmit frequency at the next PTT (`0x01E87BF6` copies `+0x1BC` to `+0x1CC`);
  VFO PROBE 0.3 page 3 says "never armed" instead of showing leftover RAM
  (`st_valid`). SAT PASS 0.10.
- Still open for a safe B (uplink) write: a reliable "transmitting" signal.

## 2026-11-11 Tracker armed on hardware: it ran, but VFO A did not change on screen

- User: key 1 (test now) restarted the radio; VFO PROBE page 3 then showed
  CALLS 33 and A filled in. **So RAM survives `cpu_reset`, the stub's hook
  (CRC check, entry call) works and the tracker computes and writes** [HW].
  VFO A's displayed frequency did not change.
- Likely cause: the stepper's tail calls `0x01E53306(0)` before copying the
  word to the working copy; it loops `0x01E50556(0..2)`, which calls the UI's
  row-draw callbacks (table at `[0x11AD0+0x188]`), i.e. it redraws the three
  rows. The tracker had only set a flag (`0xC136`). SAT PASS 0.11 calls it in
  the stepper's order (word, redraw, working copy, flag, tune). Which side was
  selected during the test is not known; if B was, A only gets its word.

## 2026-11-11 Tracker: VFO A changed for about a second, then went back (hardware)

- User: with SAT PASS 0.11 (stepper-style redraw) VFO A showed the tracked
  frequency "for a second before going back to the previous frequency".
  So the write, the redraw and the retune work [HW].
- Why it reverts (static): stock keeps each VFO's frequency in the `rw_freq`
  settings partition (partition 4, records at `0x1950` for A and `0x1960` for B,
  4 bytes of BCD) and the RAM words `+0x1D0/+0x1D4` (A) and `+0x1D8/+0x1DC` (B)
  are a cache of them: `0x01E5663E` / `0x01E566A8` re-read the records and
  overwrite the words, and `0x01E58E3E` (the RF refresh, 30 callers) runs them
  first. The stepper avoids the revert by saving the new value to the record
  before it refreshes: `0x01E56038(freq, buf)` encodes it,
  `0x01E50128(4, 0x1950, buf, 4)` writes it (sector read-modify-write with
  interrupts off; the partition is named rw_freq: stock writes it on every dial
  step), then `0x01E58E3E(0)`. In the tracker's case the flag `0xC136` that I set
  (copied from the stepper) probably made stock run that refresh about a second
  later, loading the old record.
- SAT PASS 0.12: no longer sets that flag; if a word is put back it is written
  again on the next call and the count is kept (`st_reverts`, shown by VFO
  PROBE 0.4 page 3 as REVERTS). If it still reverts, the next step is the
  stepper's own path (write the record, then refresh), which would change the
  saved VFO (original to be restored when tracking stops).

## 2026-11-11 Tracker holds VFO A; the Doppler number jittered (SAT PASS 0.13)

- User (hardware, 0.12): VFO A is tracking; the display showed 145.89662,
  then 663, 664, then back to 662. Doppler is the right sign and size.
- Cause: the shift used a one-second forward difference of the slant range,
  which is only known to 1/256 km (3.9 m): noise about 20 Hz at 145.9 MHz
  (host: second difference stdev 22 Hz, 210 direction reversals in a 595 s
  pass) and a lag of half a second (up to about 25 Hz at 2 m on an overhead
  pass). `doppler.h` now centres an 8-second difference (range at t-4 s and
  t+4 s): noise 3 Hz (the 10 Hz display step), 0 reversals; the centred
  bias is far under a hertz. SGP4 check (now against SGP4's centred range
  rate): worst 76 Hz at 437.8 MHz with fresh elements, 124 Hz at 7 days.
- When stock puts a VFO word back, the tracker now writes the same value
  again (it used to compute a fresh one each time, which also made the number
  flicker); REVERTS in VFO PROBE page 3 still counts these.

## 2026-11-11 Tracker: swap the sides (SAT PASS 0.14)

- User: "can we also switch to VFO B for listening and VFO A for transmitting?"
  Flag 16 in `st_flags`: the listening side is VFO B (it gets the Doppler-
  corrected downlink, and is the one retuned), the sending side is VFO A (uplink,
  only if send is on; its word and, when selected, the working copy follow, but the
  receiver is never retuned onto it). The confirmation screen: Up = send on/off,
  Down = swap, Menu = start, 1 = test now (listen only). Host test: swapped, with
  the listening side selected or not, send side on and off. Hardware: pending.

## 2026-11-11 B listens / A sends: PTT put the listening VFO back (hardware) -> saved records (SAT PASS 0.15)

- User (0.14, swapped): VFO B shows the right frequency, but pressing PTT on VFO A
  flashes B back to the old frequency. That is the stock refresh at PTT
  re-reading both VFO records (`0x01E5663E` / `0x01E566A8`) over the RAM words.
  It also means a RAM-only uplink on the sending VFO could never work: PTT would
  reload the old saved A and transmit on that.
- Fix: every update is now saved the way the stock stepper saves one
  (`vfo_persist.h`: `0x01E56038` encode, `0x01E50128(4, 0x1950 or 0x1960, buf,
  4)`; the same literals as the stepper's branch at `0x01E67D16`), so a refresh
  loads our value. To keep the user's own frequencies: `vfo_backup.h` logs them
  in chip sector `0x3FE000` when tracking is armed (kept if an earlier session
  was never stopped) and `vfo_restore_pending` writes them back when the last
  pass is over (tracker) or when the menu opens (hold Back, `MENU` runs it
  before drawing; also after a power cycle in the middle of a pass). Only the
  sides the tracker touched are restored. The game slot's voice area now ends at
  `0x3FE000` (`dist/doom/upload.mjs`). Update threshold 400 Hz (fewer writes).
- Split: the tracker and its arming moved to the hidden module `.SAT TRACKER`
  (`satauto.c`, 4,597 B); `.SAT TRACK` (5,713 B) is the live page and the start
  screen and asks the loader for it. MENU 2,585 B (new). Host test
  `test_sattrack.c` (satauto.c with fake stock calls and chip): saved record
  offsets and values, swap, restore at the end, a second restore does nothing.
  Firmware package unchanged (`357289b9`).
- Risks to watch on hardware: partition writes during a transmission (every
  400 Hz of change, interrupts off for the sector write); the transmit-in-progress
  check is still unreliable; the saved VFO A/B are the Doppler values until
  restored.

## 2026-11-11 B stayed at PTT; squelch open during the pass (SAT PASS 0.16)

- User (0.15, swapped): VFO B stayed on its tracked frequency when PTT was pressed
  on A. Saving each update as the stepper does fixed the PTT revert [HW].
- Squelch (user: "open squelch when the pass begins"): the receive state machine
  `0x01E840BC` calls `0x01E83B42` (REG 0x30 = 0xBFF5, speaker gate 0xB on) when its
  measure (`0x01E833A6`) reaches the halfword at `0x11AD0 + 0x83C` and
  `0x01E83B7C` (0xBDF5, gate off) otherwise. The tracker sets that halfword to 0
  from the rise to the set (flag 32; start screen key 2, on by default; test
  now acts at once), keeps the original (`orig_sq`, flag 64) and puts it back
  after the pass; stock refresh recomputes it, so the tracker writes 0 again each
  call. Unverified polarity: if the measure is a noise level rather than signal
  strength, 0 would hold the squelch closed. VFO PROBE 0.6 page 3 shows the
  halfword (SQUELCH 83C) and the saved original.
- "Test now" now uses the start screen's choices (send, swap, squelch) instead of
  forcing listen only.

## 2026-11-11 Countdown in the stock status bar (SAT PASS 0.17)

- User asked for a countdown beside the VFO and sent a photo of the stock main
  screen (A/B rows: signal bar and RSSI, B 436.78695 and A 145.85268 with "VFO H
  FM" captions). From the photo (screen about 160 x 128): the status bar at the
  top has a gap between the "S" icon (x about 49-57) and the Bluetooth icon
  (x about 115-123), y about 2-16: room for 8 characters.
- `.SAT TRACKER` draws there with the module LCD routines from the loader's
  hook (the stock my_app loop, so it does not interleave with stock's own
  drawing): a small black box with `AOS m:ss` (white, until the rise, only when
  under 100 minutes away), `LOS m:ss` (green, rise to set) or `TEST` (test now),
  right-aligned to x = 112, y = 4, every call (stock may redraw the bar). When
  nothing is to be shown it blanks the box and calls `0x01E53306(0)` (the
  stepper's redraw of the three rows). Flag 128 = drawn. Whether stock's bar
  uses the same black as our fill (the panel shows black as blue-ish) and whether
  drawing from the hook is stable are to be seen on the radio.

## 2026-11-11 Real pass: only the chosen pass by default (SAT PASS 0.18)

- The start screen gets key 3, "only this pass" (on by default): the tracker's pass
  list is then just the chosen pass, so it restores the user's VFOs and stops when
  that pass is over; off keeps the chosen pass and every later one in the list
  (up to five). Test now respects it too.

## 2026-11-11 VFO A (send side) not showing the uplink; new defaults (SAT PASS 0.19)

- User: VFO A "isn't setting even though it was enabled"; asked for A sends (on)
  and B listens as the defaults.
- Probable cause (not yet confirmed on the radio): with B selected, `put_vfo` for the
  send side saved the record and the word but returned before `STOCK_REDRAW`, which
  was only called for the selected side, so A's row on the screen was not redrawn
  (the saved value would show after switching sides). Now `STOCK_REDRAW` follows
  every update. The start screen defaults are swap on and send on (B listens, A
  sends), squelch open on, only this pass on.

## 2026-11-11 Squelch gone after every VFO B change; start screen order (SAT PASS 0.20)

- User: "every time the VFO B frequency changed, the squelch went away".
  Cause (static, matches the symptom): the receive tune `0x01E5942E` writes
  REG 0x30 = 0xBDF1 (audio off); stock's state machine (`0x01E840BC`) only calls
  its open routine `0x01E83B42` (REG 0x30 = 0xBFF5, speaker gate on) when the
  squelch goes from closed to open, and with the threshold held at 0 it is
  already "open", so the audio stayed off after each retune. The tracker now calls
  `0x01E83B42` right after each retune while it holds the squelch open (flag 64).
- Start screen: VFO A is listed above VFO B, each with its role (LISTEN (DOWN) or
  SEND (UP) ON/OFF), whatever the swap.

## 2026-11-11 PF1 short press = PTT2 while tracking (SAT PASS 0.21)

- User: PF1 must be reassigned to PTT2 ("that forces the main PTT button to TX on A").
  The button settings live in partition 4 at `0xC90..0xC95` (top short, PF1 short,
  PF2 short, top long, PF1 long, PF2 long; the settings map's image offsets
  `0x0C98..0x0C9D` minus the eight-byte identity). Stock's loader
  (`0x01E71E30`, the block at `0x01E726CA`) reads them into RAM at `0x11AD0 +
  0x10EC` (+0 PF1 short, +1 PF1 long, +2 PF2 short, +3 PF2 long, +4 top short,
  +5 top long) and defaults an erased PF1 short to 7, which is PTT2.
- The tracker sets the RAM byte `0x12BBC` to 7 from its first call after the
  restart (RAM only: stock reads the saved setting again at the next restart),
  keeps the original (`orig_pf1`, flag 256) and puts it back after the last
  pass; start screen key 4 (on by default; flag 512). VFO PROBE 0.7 page 3
  shows the byte and the saved original. `orig_sq` is now a halfword so the
  shared state stays within 96 bytes.
- Unverified: that stock reads the byte at key time (not cached elsewhere), and
  what PTT2 does when the main PTT is pressed (the user's description: it
  forces the main PTT to transmit on A).

## 2026-11-11 TRACK page: SEND above LISTEN, Menu opens the start screen; TX tone (SAT PASS 0.23)

- TRACK page: SEND (with its tone) on top, LISTEN below; Menu opens the VFO
  tracking start screen (it was key 5; Menu used to return to the list, Back
  does that now); the start screen's own Menu still starts.
- TX tone: a VFO record is the channel layout (`dist/channel.mjs`): bytes 8-9
  receive tone, 10-11 transmit tone, two BCD bytes of tenths of a hertz, low
  byte first (67.0 Hz = bytes `70 06`; `FF FF` = none). The working copies are
  halfwords at `0x11AD0 + 0x98` (receive) and `+ 0x9A` (transmit; the transmit
  tune `0x01E5AD68` passes `+0x9A` unless `+0x10EA` is set). The tracker
  writes the satellite's tone (`SATFREQ.tone10`; none -> `FF FF`) into the
  SENDING side's record at partition 4 offset `0x1950 + 10` (A) or `0x1960 +
  10` (B) with `0x01E50128` (2 bytes), once per satellite, and into the
  working halfword when that side is selected; the original tones are read with
  `0x01E52CB2` at arming, kept in the backup log (now `VFB2`, 32-byte records
  with both tones; the old 16-byte records are ignored) and written back with
  the frequencies. After the last pass the tracker also calls the RF refresh
  `0x01E58E3E(0)` so stock reloads the restored VFOs at once.
- Unverified on the radio: that the VFO record's tone bytes are at 10-11 as in
  channel records, and the tone halfword format (BCD, as in the record).

## 2026-11-11 App Manager: info dialogs, satellite list, data in the browser (SAT PASS 0.24)

Offline build and browser-tested against the local site; **not yet tested on the radio** (a new `.SAT DATA`
module and the refactored SAT screens have not been flashed).

- The satellite table is no longer code. It is the hidden module `.SAT DATA` (`satdata.h`: 32-byte header,
  64 bytes per satellite: orbit elements, then downlink/uplink/tone). SAT PASS, PLOT, TRACK and the resident
  tracker read it from the bundle; without it SAT PASS shows NO SATELLITE DATA. `satellites.json` lists the
  compatible FM satellites (label up to 7 characters); `build.py` packs `default.tle` into the default data.
- `satpack.py` was rewritten as a library (`pack`). A bug found on the way: the mean-motion rate must include
  the decay term moved to the shared epoch (`m_rate + 2·m_ddot·shift`); without it the SGP4 check failed
  (range 34.9 km vs 17.1 km). `test_satpass.py` is back at 0.5 d 17 km / 76 Hz, 7 d 29 km / 124 Hz.
- ASan found a 28-byte buffer written with 28 characters in `sattrack.c` `row()` (now 32). Host tests moved to
  `host_satdata.h` (a fake bundle with `.SAT DATA`).
- Browser: `dist/apps/satdata.mjs` ports the packer (byte-for-byte against the Python output, test fixtures in
  `test/fixtures/satpass-*`), `info.mjs` is the Info / Info & settings dialog (description, controls, notes from
  `catalog.json`: `summary`, `controls`, `notes`, `config`, `data`). SAT PASS's dialog lists the satellites with
  on/off boxes (saved in the browser, all by default). **Add** or **Update** on the card fetches fresh elements
  from CelesTrak for the ticked satellites (all if the dialog was never opened) and stages them in place of the
  built-in data; if CelesTrak cannot be reached it falls back to the built-in data (first install) or keeps the
  data already on the radio. The dialog's "Update data and stage" also adds the app when it is not added.
  Refreshed data is never treated as an outdated app and survives app updates. CSP now allows `celestrak.org`.
- Cards show one-line summaries (`summary`, at most 90 characters) and a size line only.

## 2026-11-11 MAZE CHASE 0.1 (new game, offline only)

An original maze game (not a recreation of any existing title): `modules/maze/maze.c`, 5,269 B, 25 x 19 maze of
6-pixel tiles, four prowlers with different targeting (direct, ahead of you, hunt-and-retreat, random wander),
scatter/chase phases, corner power pellets (frightened ghosts, 200/400/800/1600), three lives, faster each
level, wrap-around tunnel, pause. Keys 2/4/6/8 or Up/Down steer, Menu pauses, Back leaves. The maze was
generated (seeded DFS, mirrored, braided, no dead ends, all tiles reachable) and embedded as text. State is on
the stack; time is the 100 Hz tick at 0x1D8B0. Host harness `modules/maze/render_maze.c` (fake LCD, clock and
random-walker keys) ran 120 s games for three seeds under ASan/UBSan without faults and renders frames.
Published in the App Manager catalog (Games). **Not yet run on the radio**: key mapping (digit key codes),
step speed (14 ticks per player step, 17 - level for prowlers) and screen fit are unverified.

## 2026-11-11 MAZE CHASE 0.2: sound (offline only)

- New `modules/common/snd.c/.h`: square-wave effects through the stock DAC, using the Doom firmware's recipe
  (builds 24-26): PA3 (speaker amplifier enable) output and high, speaker gate source 1, DAC rate 11025 Hz and
  start through stock (`0x01E434C8`, `0x01E43534`; the DAC handle is `0x11AD0 + 6636`), 64-sample (128 B)
  writes through `0x01E43698`, kept about 200 ms ahead of the 100 Hz tick. Closing writes about 70 ms of silence
  and releases gate source 1. Called by function pointer, so `h9lib.s` (shared with SAT PASS, 31 B from its
  limit) is unchanged. Amplitude 9000 of 32767, and the stock volume setting is left alone.
- Effects (priorities so a jingle is not cut by a munch): dot (alternating up/down sweep), power pellet,
  eaten prowler, start jingle, level clear, death sweep, game over. Title screen key 5 toggles sound.
- Host harness captures the DAC writes: 392 Hz first note measured about 385 Hz, peak 9000, amplifier released
  at exit, two seeds under ASan/UBSan without faults. MAZE CHASE is 6,533 B.
- **Unproven on the radio.** Doom ran with stock's app loop stopped; here stock's loop and audio tasks are
  alive around the module, so the DAC, the amplifier gate or a stock mute may interfere (silence, or hiss from
  the receiver path). Volume state was not set (Doom set all five states to 30). To check: sound on, listen for
  the start jingle; if silent, try raising volume or the gate source; if the radio misbehaves, sound off (key 5).

- MAZE CHASE 0.3 (hardware: "just a buzz-hum noise, no music/sfx"): cause was my call to the stock DAC write.
  Stock `0x01E43698` takes only the sample buffer (the Doom shim `h9_dac_write128(pcm)`); I passed the DAC handle
  first, so it played 128 bytes of RAM at the handle, a hum. Now `DAC_WRITE(chunk)`. The host harness fake
  also took a handle, which is why it missed this; it now has the same one-argument shape.

## 2026-11-11 APRS BEACON (smart beaconing), APRS 0.6 (offline only)

Rolled in the scheduler of https://github.com/tomaszkuehn/TD-H9-aprs-smartbeacon (MIT, commit 4857fb6e): vendored
unchanged in `modules/aprs/third_party/smartbeacon/` with its licence, noted in `THIRD_PARTY_NOTICES.md` and the
app's catalog notes. That project patches the radio's own APRS module; H9 Toolbox instead keeps the stock firmware
and adds a screen.

- New module `APRS BEACON` (`aprs_beacon.c`, 5,945 B, in the menu as its own entry; part of the APRS app):
  reads the modem ring's NMEA frames (cmd 0x10) itself (`beacon_core.c`: RMC parse with position, speed and course,
  KISS unescape, AX.25 frames skipped), feeds `sb_on_rmc`/`sb_tick` once a second, and when the scheduler fires
  sends `!DDMM.hhN/DDDMM.hhW<symbol>ccc/sss H9 Toolbox` through stock's KISS transmit `0x01E5DBCC`, exactly as
  APRS SEND does (TX CH side, 2 m only, callsign from stock; side and speaker restored after). It starts OFF; Menu
  (a tap of at least 3 ticks, so the keypad glitch cannot start it) toggles; 1 symbol (person/bike/car); 5 sends one now;
  Back stops and leaves. First position about 15 s after starting (the upstream 15 s guard).
- Not shown while beaconing: heard stations (the screen drains the ring but only parses GPS).
- Shared APRS state is unchanged (`struct aprs` is 1528 of 1536 bytes, so no room was needed or used).
- Vendored file note: its `(sb_state_t){0}` trips `-Wmissing-braces` under our `-Werror`; `build.py` adds
  `-Wno-missing-braces` for files under `third_party/` instead of editing it.
- `test_beacon.c` (host, ASan/UBSan): RMC parsing incl. void fix, bad latitude, missing course; KISS feed with
  junk, AX.25 skip, split frames, escapes; packet text; the scheduler (fix appears, 60 km/h interval, corner, stop,
  parked 10 min). Passes. **Not run on the radio.**

## 2026-11-11 APRS 0.7: a once-over (offline only), and section GC for every module

- **Linker GC (all modules).** `build.py` links with `--gc-sections` (the modules were already compiled with
  `-ffunction-sections`, but the Python linker kept everything), and `h9lib.s` has `h9_led`, `h9_usb_loader` and
  `h9_reset` in their own sections. Unused functions no longer ride along in every module. Savings (bytes):
  SAT TRACKER -716, SAT PLOT/TRACK -664, APRS MSG -181 (before this change's additions), SAT PASS -64, GPS -108,
  MAZE CHASE -96, MENU -36. **Every module's binary changed**, including the hardware-tested ones (MENU, SAT PASS,
  GPS...): the code is the same, so behaviour should not, but they have not been re-run on a radio.
- **APRS code limit** is now 6,768 B (state slot 0x570 instead of 0x600): the shared state was repacked (station
  struct had 4 bytes of padding per entry, counters and screen indices are bytes) and the frame buffer is 128 B
  (was 200; an AX.25 frame longer than that loses only its tail, stored Beacon List records keep 69 info bytes).
  `APR3` magic. SAT PASS's limit stays 6,528 (its state at 0x2B880 is hard-coded in the stub).
- **Menu "APRS" is now a 3 KB loader** that loads stock's Beacon List and hands over to the hidden `.APRS LIST`,
  so every screen's Back-to-list no longer carries the loader (it was 258 B of each list build).
- **UX**: a bottom line names the screens (1 LIST 2 MAP 3 MSG 4 SEND 5 BCN) and the digits jump there (Send and
  Beacon use digits themselves, so they have no digit navigation; hold Menu still cycles). The list shows how long
  ago each live station was heard (NOW/5M/2H) and a context line; the details page has the heard age and no longer
  shows the raw symbol characters; the map is 10 px shorter with a name/range line and no axes; the message list
  loses its "ME" row (callsign moved to the bottom line) and shows ACK on messages I sent once the recipient's
  `ackNN` arrives (the id I send is stored with the message, `struct message` is unchanged in size). BEACON's
  Back now returns to the list. Station names and ranges are unchanged.
- **Tech debt**: TICK, RING, KISS-transmit and 2 m limit constants live in one place each (`aprs_core.h`,
  `aprs_tx.h`) instead of per file; a dead branch in the details screen went; `FRAME_MAX`/slot sizes derive from one
  definition.
- Host: `test_aprs.c` extended (acks incl. wrong sender/id/addressee, heard ages, tab digits against the bundle),
  all green; `render_aprs.c` renders the list, details, map and messages screens for a visual check.
- Not done / ideas: auto-acknowledging received messages (an unattended transmit), message retries, wider station
  comments (the 16-station table is the memory limit), a heard-age on the map.

## 2026-11-11 Screenshots in the App Manager's info dialogs

`make_screenshots.py` renders each app's screens with the host harnesses (`render_aprs.c`, `render_gps.c`,
`render_maze.c`, `render_plot.c`, `render_track.c`: the app's own drawing code, a fake LCD and made-up data) into
`dist/apps/catalog/shots/*.png` (3x, 480 x 384). `modules/catalog.json` lists them per app (`screenshots`),
`publish_catalog.py` refuses a missing file, `info.mjs` shows them in the dialog with a note that they are
simulations. 11 shots: APRS 4, GPS 3, Maze Chase 2, SAT PASS 2. Brick Breaker (assembly), APRS Monitor and VFO
PROBE have no host render yet, so their dialogs have none.

- Added `render_satlist.c` (the SAT PASS list page: a fix and time fed through the modem ring, passes computed by the
  real code) and a `sat-list` screenshot as the first one on SAT PASS's card. `satpass.c`'s ring/tick macros are now
  `#ifndef`-guarded so the harness can override them (no change to the built module).

## 2026-11-11 APRS BEACON fixes (hardware: "doesn't stay enabled, won't beacon without GPS, no smart beaconing option")

Offline build and host tests only; **not re-run on the radio**.

- **Turned itself off**: starting and stopping were one Menu tap each. The keypad reads Menu for a few ms when the
  speaker amp starts and after a transmit (resource-flash-log.md, 2026-11-10), and a stock transmit happens right
  after a beacon, so a tap-to-toggle was fragile. Now a **hold** of 0.6 s (60 ticks) starts or stops it, a tap
  (3+ ticks) only changes the selected setting, Back needs 3+ ticks, and keys are ignored for 0.5 s after a
  transmit. `test_beacon_screen.c` drives the real screen loop with a scripted keypad: 2- and 3-tick glitches do not
  start it, a hold does, a hold stops it, a tap right after a TX changes nothing (the same tap 1 s later does), and
  it stays on through transmits.
- **Without GPS**: it no longer refuses. Position order: this session's last fix, else the fix the loader's
  background recorder saved on the chip (`fix_load_micro`), marked "H9 Toolbox, last known" and without course/speed.
  With neither it says NO POSITION YET and retries each minute without turning off. (`beacon_from_micro`, host tested
  incl. the hundredths-of-a-minute arithmetic, which the first version got wrong.)
- **Modes**: the screen now has MODE (SMART, the vendored scheduler, or TIMED), EVERY (1, 2, 5, 10, 15, 30 min, for
  TIMED) and SYMBOL, chosen with Up/Down and set with Menu; kept for the session in `A->beacon` (one byte, state
  magic stays APR3: `FRAME_MAX` 124 paid for it; state 1388 of 1392). TIMED: first position 15 s after the start,
  then every N minutes (`beacon_timed_due`). SMART starts with a pending first position, so the first goes out after
  the 15 s guard even without a fix.
- Still foreground only: it beacons while its screen is open and stops when you leave (a background beacon would
  use the SAT TRACKER's resident mechanism; not done). Sizes: APRS BEACON 6,737 B (limit 6,768); MAP is at 6,767.
- New `aprs-beacon` screenshot and catalog text (controls, notes).

## 2026-11-11 KISS terminal + VFO control page (`dist/tnc/`, unlinked, noindex)

Offline build and host tests only (`test/tnc.test.mjs`, 12 tests); **not run against a radio**. Demo mode verified in Chrome.

- One BLE connection, two routes. KISS terminal: AF00 service (AF01 write-without-response, one whole frame per write; AF02 notifications into a streaming decoder), from the static findings in `prototypes/kiss-tnc/H9-BLE-NOTES.md` (still unconfirmed on hardware). VFO control: the FF00 programming service, one short programming session per action, every write read back.
- VFO plumbing reused from `prototypes/vfo-control`: active side `0x3004` (hardware-verified), B channel `0x0CA5` (verified on most attempts), A channel `0x0CA4` and display modes `0x0CA2/3` by the same layout (A not yet tried). No live frequency command is known, so "Tune" writes frequency+name into a chosen memory channel (default 199, listen-only by default, needs an acknowledgement) and points the side at it. Unverified on the radio.
- `dist/channel.mjs` gained `channelBlocks(n)` (blocks one channel touches) so a tune reads/writes only those.
- Open hardware questions: do AF00 and FF00 coexist on one GATT connection; does the AF01 write reach RF; does AF02 forward received packets; does the display follow a retune.

## 2026-11-11 Why writes above 0x3000 work over BLE but not over serial / USB-C (static analysis)

Prompted by CHIRP issue #12216 (the TD-H9 module): writes to 0x3000+ (GNSS, APRS, SMS settings) "work 100% over
ble-serial, spotty over K-type serial or USB-C, with no sign of failure" (the module's author, 2026-11-07; others:
1 -> 0 changes stick, 0 -> 1 do not, or a corrupted area). Disassembled stock 1.0.38 (`app+X` = CPU `0x01E00100+X`;
call targets are `address + offset + 4`). **[STATIC]; not yet confirmed on the radio.**

- One clone state machine, `app+0x5CF18` (CPU `0x01E5D018`), serves every transport. Each transport's wrapper builds a
  small ops table on the stack and calls it: BLE `app+0x5D170` (table state `0x1202C`, send byte `0x4FD82`, send
  buffer `0x4FD9E`, write op `0x5017C`, flags 0); serial `app+0x78A6A` (CPU `0x01E78B6A`, state `0x12038`, send ops
  `0x781DA`/`0x7823E`, write op `app+0x78248` = CPU `0x01E78348`, three extra hooks, flags 7), called from the UART/USB
  task at `0x01E78BD8` with the bytes of one read (up to 256) and transport id 1 or 2.
- The `W` (87) branch needs the frame to be exactly **37 bytes** (`W`, address hi/lo, size 0x20, 32 data, checksum)
  in one hand-off (`if (len != 37)`), then calls the write op `(address, data+4, 32, first_block)` with
  `first_block = (address == 0) | (address == 0x5000)`, then sends the ACK `06` **without looking at the write's
  result**. Anything malformed, a frame split across transfers (BLE MTU too small, or two serial reads), or a write
  that failed is dropped or ACKed with no error reply: the "no indication" everyone reports.
- **BLE write op** (`0x5017C` is a thunk to `0x01E5012A`, partition 4, offset, data, length): sector-aware
  read-modify-write. It reads the 4 KiB sector into a RAM buffer, checks whether the target bytes are 0xFF, erases the
  sector if not (`call` to the sector erase), merges the new bytes and writes the sector back. Works at any address.
- **Serial write op** (`0x01E78348`): erases only when `first_block`: at address 0 it erases **2 sectors**
  (`0x0000-0x1FFF`) and at 0x5000 **1 sector** (partition 4, via `0x01E70DBC`); then, after the readiness check
  (`0x12234 == 0x5AA5`) and a range check (`0x01E4FECC`), it page-programs the bytes with `0x01E5006E` (256-byte
  pages, no erase). So over serial the sector holding `0x3000-0x3FFF` is **never erased**: programming flash can only
  clear bits, so `1 -> 0` changes stick and `0 -> 1` do not, and a value that needs a set bit comes out ANDed with
  the old one ("ignored" or "a corrupted data area"). That matches every report in the thread (GPS off works, on
  never does; "doesn't persist usually").
- The main block `0x0000-0x1FFF` works over serial because writing address 0 first erases it: clients that write in
  address order get it right, and ones that skip block 0 would see the same symptom there.
- **MTU**: the 37-byte frame must arrive in one GATT write, so the ATT MTU must be at least 40 (37 + 3 header)
  (`ble-serial -m 37`, as the thread says); a smaller one splits the frame and the radio silently drops it.
- Possible fixes: (1) clients: use BLE for 0x3000+, and over serial check by reading back; there is no protocol
  command that erases the 0x3000 sector. (2) Firmware (ours, would need a flash and hardware test): make the serial
  op use the BLE routine for addresses at or above 0x2000: patch the literal `0x01E78348` stored in the serial
  wrapper at `0x01E78B9E` to a small cave routine that tail-calls `0x01E5027C` for such addresses and the original
  op otherwise; or erase sector 3 on the first block. Unknown: what else lives in `0x2000-0x2FFF` and
  `0x3140-0x3FFF` (an erase must not wipe it).
- Settled: the `r2 = r4  #` right after the literal in the serial wrapper is a parallel pair (JieLi's objdump prints
  the paired store indented): `[sp+12] = r2` stores the literal while `r2 = r4` runs beside it, so the serial write op
  is `0x01E78348` (the bytes at app+0x78A9E are `c2ff 4883 e701`).

## 2026-11-11 Firmware fix: serial / USB clone writes at 0x2000-0x3FFF use the sector-aware writer (offline, not flashed)

- `src/stub.s` `ml_serial_write` (stub 1,152 B of the 1,632-byte cave, at `0x01EA7D36`): `if (r0 < 0x2000 || r0 >= 0x4000)`
  tail into the stock serial write op (saves rets/r4, `call`s `0x01E78348` with r0-r3 untouched); otherwise `r3 =
  0x01E5027C; goto r3`, Bluetooth's write thunk (partition 4, address, data, length; it overwrites r3, so the
  first-block flag is not needed). 0x4000-0x4FFF never reach it (the S2 write hook takes them first), nor do 0x5000+.
- `build.py` patches the 4-byte literal in the serial wrapper's ops table (`SERIAL_WRITE_SITE` app+0x78AA0, stock
  `4883e701`, checked in stock and in the base) to the shim's address, and adds flash sector `0x7D000` to the changed
  set. Build: app `926f623c` -> new package `b0135e0c066ed724ba6628b21eb2a77fc6bdd87cc25e8c51e5fcd7e0cf5bab0a`,
  image `a87fc774...`. The shim and the patched wrapper were decoded back with the pi32v2 decoder.
- Installer (`dist/firmware-update`): `release_update.mjs` made seeds against the new build; the build that was hosted
  (`357289b9`) is now PREVIOUS `release3`; every seed grew by the 0x7D000 sector. Node tests pass (132).
- **Risk/behaviour**: only 0x2000-0x3FFF changes. The main block (0x0000-0x1FFF, written after the block-0 erase) and the
  0x5000 region keep the stock path. Each block at 0x3000+ now costs Bluetooth's read-modify-write of a 4 KiB sector
  (a few ms, tens of ms when it erases), so serial clients get the ACK a little later than for the main block. A
  frame split across serial reads or a short BLE MTU is still silently dropped (the handler wants exactly 37 bytes).
- **Hardware test (not done)**: flash this build, then `python3 -I serial_clone_test.py <port>` (pyserial) over the
  K-plug or USB-C: it toggles the Talk around bit at 0x3014 (0 -> 1 is the write stock serial cannot do), reads back,
  and restores. PASS expected on this build, FAIL on stock firmware. `test_serial_clone.py` checks the tool against
  simulated stock and fixed radios.

- **Hardware (build b0135e0c, USB-C serial, `/dev/cu.usbmodem101` on macOS, normal mode)**: `serial_clone_test.py --read-only`
  read 0x3000-0x311F. The full test then wrote Talk around (0x3014 bit 0) from 0 to 1 with the radio running the new
  build: read back OK, original block restored OK, **PASS**. This is the 0 -> 1 write the stock serial op cannot do.
  Not yet done: the same test on the previous build for a FAIL baseline; the K-plug cable; a back-to-back write of
  the whole 0x3000-0x3120 range (CHIRP's pattern) to check ACK timing.

- **Hardware (build b0135e0c, USB-C serial): whole-range test** (`serial_clone_test.py --range`): read the 9 blocks of
  0x3000-0x311F, wrote them all back to back (the first with Talk around 0 -> 1, the rest unchanged), read all back:
  **PASS**, all 9 blocks restored. ACK times: first 30 ms, slowest 44 ms, mean 32 ms, 0.29 s in total. That is well inside
  the 0.5 s timeouts CHIRP logs show, so the sector-aware write's extra time is not a problem here.

## 2026-11-11 CHIRP patch for radios running H9 Toolbox Firmware

Downloaded Fred Trimble's `tdh8.py` (beta 103126.0, 145,949 B, from CHIRP issue 12216) to a scratch folder (not in
the repo, GPL) and made `chirp/tdh8-h9toolbox.patch`: writes at 0x3000+ are read back and verified (one retry, then a
`RadioError` that says why), prompt text, and a fix for an unset `_msg` in `_read_block`. The module's upload was run
against simulated radios through stubbed CHIRP imports (`chirp/test_chirp_patch.py`): fixed radio passes, stock serial
is reported (the original module passes silently there), clearing bits raises no false alarm. A ready-patched copy is
at `~/Documents/ChatGPT/TID-H9/chirp-test/tdh8_h9toolbox.py` (outside the repo) to load as a CHIRP test module. Not yet run in
CHIRP on a radio.

- **Hardware (build b0135e0c, K-plug with a CH340 cable, `/dev/cu.usbserial-10`, 38400 baud (CHIRP's rate), normal mode)**:
  read-only dump of 0x3000-0x311F identical to the USB-C one; single-bit test **PASS** (Talk around 0 -> 1 read back,
  restored); `--range` **PASS** (9 blocks back to back, all restored). ACK times: first 52 ms, slowest 52 ms, mean
  49 ms, 0.44 s for 9 blocks (slower than USB-C's 32 ms mean at 115200 because of the baud rate). So both serial
  transports now set bits above 0x3000. Still not done: the same tests on the previous build for a FAIL baseline, and the
  patched CHIRP module in CHIRP.

- **Hardware (build b0135e0c): patched CHIRP module in CHIRP**, Talk Around Off -> On (a 0 -> 1 write at 0x3014) uploaded and
  confirmed by a re-download, over **both the CH340 K-plug and USB-C serial**: worked, no error (the user's report).
  Not yet done: the error path on stock / the previous build, a Bluetooth (ble-serial) upload for read-back false
  alarms, and a channel edit in the main region through the patched module.

- **Hardware (build b0135e0c, USB-C serial): Fred Trimble's unmodified `tdh8.py` (beta 103126.0, SHA-256 79a98773...) loaded in
  CHIRP**: Talk Around Off -> On uploaded and read back On after a re-download (user's report): **no change to the
  CHIRP module is needed on this firmware.** The optional read-back patch (`chirp/`) only adds an error for radios that
  cannot set the bits (stock firmware over serial). K-plug not re-run with the original module.

- CHIRP: the optional patch is now only the Radio Information text for the TD-H3 Plus / TD-H9 (4 lines added, 1 changed, no
  code): serial / USB-C uploads above 0x3000 work on H9 Toolbox Firmware, and can silently fail to set bits on other
  firmware. The earlier read-back verification patch was dropped at the user's request. Tested against stubbed CHIRP only.

- CHIRP (revised at the user's request): the optional patch is the Radio Information text **plus** the read-back of writes
  at 0x3000+ again (30 lines added, 1 changed), using the user's wording for the read-back's comment. 7 simulated-radio
  and information tests pass (`chirp/test_chirp_patch.py`). Not run in CHIRP yet.

- The CHIRP patch, its test and the CHIRP stubs were removed from the repository (`chirp/` keeps a README with the notes);
  the patch to Fred's GPL module goes to him by email instead. They remain in the earlier commit `6bd1670`.
