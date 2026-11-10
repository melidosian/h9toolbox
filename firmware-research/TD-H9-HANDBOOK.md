# TD-H9 handbook: what is known and how to work on it

A condensed reference for people and agents working on the TIDRADIO TD-H9
(JieLi BR23 / AC695x main MCU, stock firmware v1.0.38). The full history,
with commands and hashes, is in [resource-flash-log.md](resource-flash-log.md);
search it for the addresses below.

Tags: **[HW]** proven on the radio · **[STATIC]** from disassembly of the
pinned stock image · **[HYP]** a working hypothesis, not proven.

Addresses: *app+X* is an offset into the stock `app.bin`
(SHA-256 `11a07bc0…`). Code runs at **CPU = 0x01E00100 + app offset**; in
main flash that is **flash = app offset + 0x5100**, i.e.
`CPU = 0x01E00000 + (flash − 0x5000)`. RAM addresses are plain (`0x11AD0`).
ROM routines live at `0x0200xxxx`.

---

## 1. Rules for this repo

- **Public actions need an explicit "post it"**: GitHub comments, issues, PRs.
  Pushes and merges happen only when the user asks.
- **Never commit RAM dumps, full-chip dumps or `~/h9-backups` files**: they
  hold the owner's channels and settings.
- **What may be committed**: the user allowed the Doom release files
  (`dist/doom/files/`: firmware, code blob, Freedoom pack, music) and JieLi's
  toolchain archive (`toolchain/jieli/`). The old "no binaries" rule belongs
  to the separate `td-h9-open-firmware` repo, not this one.
- **Anything written to the 16 MB chip must be restorable**: only in erased
  tails, recorded by a directory, so that "restore" means erase. Stock must
  still boot afterwards.
- **Every custom firmware keeps a way back to USB update mode** (see §6).
- **Log every finding** in `resource-flash-log.md`: what was tried, results,
  hashes.
- User-facing text says "H9 Toolbox Firmware", not the research names
  (S1/S2).

## 2. Hardware

| Part | What we know |
|---|---|
| Main MCU | JieLi BR23 (chip key `0xF181`) [HW]. CPU about 135,000–200,000 busy-loop iterations per 10 ms tick (C-hello LOOP) [HW]; `clk_set("sys", 240 MHz)` changes nothing measurable, so it already runs fast [HW]. |
| Main flash | 1 MiB SPI NOR, ID `0x856014` [HW]. |
| Resource chip | 16 MiB SPI NOR on SPI2 (port A pins 24/26/25, CS = GPIO 23 = bank 1 `0x1E5040` bit 7) [HW]. Reads about 2 MB/s [HW]. |
| Screen | 160 × 128 RGB565 [HW]; a full-screen write takes 10–30 ms [HW]. Backlight duty = level × 2000 + 2000; stock caps it at 10000 [HW]. |
| Speaker | BR23 DAC to an amplifier enabled by **PA3** (bank `0x1E5000` bit 3). PA3 must be configured as an **output** (DIR bit 3 = 0) as well as driven high [HW]; the radio app does this at start-up, and stock's speaker writer (app+`0x52228`) only writes OUT. |
| LED | GPIO 9 (bank `0x1E5000` bit 9) [HW]. |
| Keypad | Resistor ladders on **two ADC channels (13 and 10)**, matched against voltage windows by the stock scan `0x01E515AC`, with no debounce [STATIC]. A single noisy sample can read as a key; take a key only when two readings in a row agree [HW]. |
| RF chip registers | Write with `0x01E5070A(reg, value)`, read with `0x01E505FE` [STATIC]. Register `0x30` bit 9 enables the RF chip's audio DAC (receive audio): receive setup writes `0xBDF1` (off), squelch open writes `0xBFF5`, close `0xBDF5` [HW]. |
| APRS MCU | A separate small MCU (probably ARM Cortex-M) running the 1200-baud AFSK modem; talks to the main MCU over USART2, with a handshake on GPIOB5 [STATIC]. Not identified; research parked. |
| Receiver chip | Demodulates in hardware and hands the MCUs **audio only**: no IQ or wideband samples (so no TV, no SDR). |

## 3. Main flash map (1 MiB)

| Range | Contents |
|---|---|
| `0x00000–0x04FFF` | Boot region. HID recovery cannot read or write it [HW]. |
| `0x05000–0xD9FFF` | App image, **encrypted**: each 32-byte block is XORed with JieLi's CRC stream keyed `0xF181 ^ ((flash − 0x5000) >> 2)` [HW]. `firmware.py` repacks it. The flash mapping decrypts it for the CPU. |
| `0xDA000–0xE9FFF` | Settings VM, two 32 KiB banks (`0xDA000`, `0xE2000`) [STATIC]. VM init clamps its reserved size to 64 KiB (app+`0x8C9D4`). |
| `0xEA000–0xFBFFF` | **Free, used by no stock code** (72 KiB) [HW]. Stored **plain** and runs in place at `0x01EE5000` [HW]. Encrypted code here hangs [HW]. Code here can be called after start-up, but cannot be the boot entry itself [HW]. |
| `0xFC000–0xFFFFF` | Stock data (`0xFD000` holds about 10 bytes) [HW]. Leave alone. |

Free code space inside the app, once `my_app` (the radio app) is replaced:
app+`0x79E32–0x7D64E` and `0x7D69E–0x890E4` (about 62 KB; trap-tested 713 s
with no hit) [HW], plus the cave at app+`0xA77F0` (1,704 B; H9 Toolbox's
resource-service tables start at app+`0xA7E50`) [HW].

## 4. The 16 MB resource chip

| Range | Use |
|---|---|
| `0x3A7000–0x3FFFFF` | Voice partition tail: erased, never written by stock [HW]. |
| `0x717000–0x7FFFFF` | Font partition tail: erased, never written by stock [HW]. |
| `0x900000` | `rw_freq`: **stock writes here**. Never use it. |
| `0xA00000` | Boot logo (H9 Toolbox boot-logo tool). |
| `0xA1F000` | Game slot directory (`H9DATA01`); data from `0xA20000` to `0xCC0000` (2.6 MB), then the font and voice tails. |
| `0xCC0000` | Modules slot directory; `H9MODS01` bundle at `0xCC1000` (≤ 124 KiB) for H9 Toolbox's module loader. |
| `0xCE0000` | Music slot directory; data `0xCE1000–0xCFFFFF` (124 KiB). |
| `0x802000`, `0x80B606` | Stock APRS message inbox and beacon list (in `sys_data`) [STATIC]. |

Directory sector: magic `H9DATA01`, version, length, SHA-256, name, segment
list, CRC-16; `H9DATA-COMPLETE` is written in the last 32 bytes **only after
a full read-back**. Format: [dist/doom/upload.mjs](../dist/doom/upload.mjs);
readers: `doom-firmware/port/td_nor.c`.

The stock partition reader (`0x01E52CB2`) refuses to run unless RAM
`0x12234 = 0x5AA5` (set by `my_app` once the chip is up). Without `my_app`,
set up SPI2 and read with raw NOR reads (§7).

## 5. RAM landmarks

| Address | Meaning |
|---|---|
| `0x1D8B0` | 100 Hz tick counter [HW]. |
| `0xC3AF` | Key byte of the stock scan: **digit d → d+2**; Menu 12, Up 13, Down 14, Back 15; one key at a time [HW]. |
| `0x12234` | NOR "ready" marker `0x5AA5` (above). |
| `0x11AD0` | app_core state block: **+100 requested mode, +101 current mode** [HW]; +138 BLE connection handle [STATIC]; +6636 DAC handle [STATIC]. |
| `0x1D7D0` | Heap free-bit map: 128-byte blocks, bit set = free. Summary word per 32 blocks at +192 [STATIC]. |
| `0x1D8A8` / `0x1D8AA` | Heap blocks used / total [HW]. |
| `0x1E4D8–0x2BF00` | Stock heap first range (end at app+`0x05FA9E`) [STATIC]; a second range is `0x2D27F–0x2FF80`. |
| `0x127B0` | Clone-protocol reply buffer (80 B) [STATIC]. |
| `0xC4A1` / `0xC4A7` | Own APRS callsign (6 chars) / SSID [STATIC]. |
| `0x13D10` | Last decoded AX.25 packet (324 B: dest +0, source +7, digis +14, info +59, info length u16 +316) [STATIC]. |
| `0x16634` | Main-side APRS serial ring (2048 B; write index `h[0x16E34]`, read index `h[0x16E36]`); it keeps filling while a module runs [HW]. |
| `0x29F00–0x2BEFF` | **Module-loader builds**: 8 KiB module RAM (heap end moved to `0x29F00`); "run next" mailbox in its last 32 bytes (`0x2BEE0`, magic `HRUN`). |

Stock (H9 Toolbox) runs with 288 of 527 heap blocks used. **Bluetooth needs
about 30 KB of heap** [HW]: allocation failure jumps to ROM `0x0200000A`,
which shows up as a boot loop. The module loader's 8 KiB carve (heap end
`0x29F00`) still leaves room for Bluetooth uploads [HW].

## 6. Modes, recovery and tools

| How | Mode |
|---|---|
| **PTT + 2** at power-on | Vendor HID updater `4C4A:4155` [HW]. Used by the web flasher, `hid-recovery/restore_stock.py` (no sudo) and `/doom/`. |
| Hold **8** at power-on (stock / H9 Toolbox) | ROM USB loader `BR23UBOOT1.00`, `4C4A:2342`, mass storage [HW]. Used by `h9-flasher/h9_flasher.py` (sudo). |
| Calling ROM **`0x02000D68`** from our code | The same USB loader [HW]. Doom: hold **3** at power-on, or for 2 s on error screens. |
| PTT + 1 / PTT + 3 / PTT + 7 | Serial / USB-serial updater / APRS [HW]. |

- Takeover builds (C-hello, Doom) replace `my_app`, whose key-8 check is the
  stock route to the USB loader. They must provide their own exit, as Doom's
  hold-3 does.
- **HID protocol** [HW]: 64-byte reports `AA 55 len cmd …`, XOR key
  `4E 61 BC 00`, CRC-16. Commands: `C0` init (`app_dir_head`; app at
  `0x5000`, 4 KiB blocks), `C2` erase sector, `C3` write up to 40 bytes,
  `C4` CRC windows. Addresses are relative to `0x5000`.
  - CRC reads cover the whole 1 MiB, and `readback.mjs` rebuilds bytes from
    shifted CRC windows.
  - Writes are stored **exactly as sent**; nothing is encrypted [HW].
  - The far region `0xEA000+` can be erased and written [HW]
    (`hid-recovery/hid_far_probe.py`).
- **USB loader** [HW]: a RAM helper reads, erases and writes; the ROM
  decrypts WRITE_MEMORY and encrypts READ_MEMORY. The flasher's commands:
  - `dump`
  - `flash` (backup, write, verify; `--far <blob> --far-backup <file>`
    writes the far region in the same session)
  - `write-far`
  - `resume` and `restore`
- **Restore to stock** from any app: PTT + 2, then
  `restore_stock.py restore --backup <stock restore point> --known <installed .fw> --yes`.

## 7. Stock routines we call

The wrappers are in `c-hello/entry.s` and the `EXTRA_API` of
`doom-firmware/build.py`. Calling convention: JieLi clang ABI, arguments
r0–r3, result r0; wrappers save `{rets, r14-r4}`.

| Purpose | CPU address | Notes |
|---|---|---|
| Key scan | `0x01E515AC` | then read `0xC3AF` [HW] |
| Delay | `0x01E516C6` | ms / 10, then the ROM OS delay `0x02001F82` (yields) [HW] |
| LCD window / write | `entry.s` (`hello_set_window`, `hello_transfer_submit`) | big-endian RGB565, ≤ 128 bytes a write [HW] |
| IRQ off / on | ROM `0x02001634` / `0x02001650` | [HW] |
| SPI2 setup | `0x01E4C2DA(2)` | plus GPIO 23 as an output [HW] |
| NOR read | `0x01E4FF22(dst, addr, len)` | **DMA only if len ≥ 16 and dst is 4-byte aligned**, otherwise byte by byte [STATIC] |
| Partition read | `0x01E52CB2` | needs `0x12234 = 0x5AA5` [HW] |
| Post an app_core event | `0x01E2D87A(code, value)` | code `0xFFFF` is ignored by the handler [HW] |
| DAC rate / start / write | `0x01E434C8(dac, hz)` / `0x01E43534(dac)` / `0x01E43698(pcm128)` | dac = `0x11AD0+6636`; 11025 Hz is supported [HW] |
| Volume | `0x01E60EDA(state 0–4, level ≤ 30)` | app_audio_set_volume [STATIC] |
| Speaker gate | `0x01E5234C(source, on)` | source 1 is direct; it only writes PA3's OUT [HW] |
| Key-tone player | `0x01E5241A(n)` | stock voice prompts ("zero"); needs heap [HW] |
| System clock | `0x01EC548E(name, hz)` | name `0x01EB1C7B` = "sys" [HW, no effect seen] |
| ROM task post | `0x02002C9C` | used for the `btstack` wake [STATIC] |
| **Deliberate restart** | stock `cpu_reset` `0x01EC6FC8` / `0x01EC6FF4` | IRQs off (`0x02001634`), set bit 4 of `[0x1E0000]`, spin. **Calling ROM `0x0200000A` directly froze the radio** [HW]. |
| KISS transmit | `0x01E5DBCC(buf, len)` | stock: deframes KISS, AX.25 decode, PTT handler, modem send, airtime wait, end of TX [HW, APRS SEND] |
| RF register write / read | `0x01E5070A(reg, value)` / `0x01E505FE` | see §2 [STATIC] |

## 8. How the system behaves

- **H9 Toolbox Firmware is now the module loader** (`module-loader/`,
  2026-11-10) [HW]:
  - Brick Breaker moved out of main flash; holding **Back** opens a menu
    of modules from the chip's modules slot.
  - Modules are copied into the 8 KiB RAM slot at `0x29F00` and checked
    (CRC) before running; they get the stock NOR read, LCD, keys and delay
    through `modules/common/h9lib.s`.
  - The menu also offers RADIO (a clean `cpu_reset`) and USB UPDATE.
  - The App Manager (`dist/apps/`) installs modules over Bluetooth; the
    first modules are the menu, Brick Breaker, APRS and APRS Monitor.
  - Doom is unaffected: it is built on the earlier H9 Toolbox app
    (`1334a32c`) with its own heap patch (`0x22000`).

- **app_core** (app+`0x66648`) runs a mode state machine [HW]. Doom and
  other takeovers sit in **mode 10** (start-up). Stock runs in **mode 3**,
  which starts Bluetooth.
  - Mode table at app+`0x667F2`: 1 → `0x63390`, 2 → `0x633BE`,
    3 → `0x63C9A`, 4 → `0x65700`, 5 → `0x6616A`, 10 → the message loop `0x665B2`.
  - To switch, write the request byte (+100), then post any event:
    mode 10's loop only applies a request after a message [HW].
  - Mode 3 needs the heap given back first (see §9, upload mode), or it
    resets [HW].
- **Bluetooth stack** [STATIC]:
  - Operations go through a dispatcher at app+`0x16552(op, n, …)`: op 9 is
    TX space (answered directly), op 10 is notify/send (queued), op 11 is
    the connection-parameter request.
  - The `btstack` task is woken with a task post (app+`0x164F8`).
  - The clone ("programming") protocol runs in the BT data callback
    app+`0x5DBA8` (registered at app+`0x24004`), through `0x5D170` and the
    handler `0x5CF18`.
  - H9 Toolbox's S2 service hooks app+`0x5D018` (R) and `0x5D122` (W).
- **Fast Bluetooth reads start at 32 KiB bank boundaries** [HW]: a 4 KiB
  read at `0xA1F000` costs a 32 KiB read from `0xA18000`. To check a
  directory, read only its first 128 bytes and last 32 bytes, with 32-byte
  reads.
- **Bluetooth in Doom's upload mode is slow** [HW]:

  | | Read | Write |
  |---|---|---|
  | H9 Toolbox | about 6.5 KiB/s | about 0.77 KiB/s |
  | Doom upload mode | about 1.3 KiB/s | about 0.11 KiB/s |

  - Reads waited on app_core's loop. Posting `0xFFFF` every 10 ms
    helped 2.5× [HW].
  - Write ACKs take about 260 ms each [HW]; the static trace found no wait
    on the radio's path. The open suspects are link timing and btstack
    scheduling [HYP].
  - The page now logs GATT-write and ACK-wait times for each sector. Get
    a baseline on H9 Toolbox, then measure Doom upload mode.
- **Audio** [HW]:
  - The DAC registers are at `0x1E2F00`: +0 CON, +4 buffer, +8 length,
    +16/+20 hardware read and software write pointers, +40 volume.
  - The DAC stays stopped until stock's start runs.
  - Recipe: PA3 as an output and high, speaker gate, volume, rate,
    start, then write 128-byte chunks paced to the tick
    (`doom-firmware/port/td_sound.c`).
- **The `fm_inside` module** [HW]:
  - The ops table at app+`0xA14` (init app+`0x4AEAC`) is the built-in
    **FM receiver**, and DAC CON bit 24 is its analog link.
  - Calling its power-up mid-run **froze the radio**. It is not the
    speaker path.
- **Low-power timer** [STATIC]: event 3 arms a timeout (app+`0x5E208` →
  `0x4F4EC`) that later calls app+`0x5E3E2` (`clk_set` 24 MHz). It was not
  active in our runs.

## 9. The Doom firmware (`doom-firmware/`)

- **Layout** [HW]:
  - A 4-byte call at `my_app`'s entry (app+`0x88180`) leads to the
    start-up stub in the cave.
  - Engine code goes in the far region and the `my_app` block.
  - The RAM block `0x22000–0x2BF00` comes from shrinking the stock heap
    (patch at app+`0x05FA9E`).
  - The image is built on H9 Toolbox, so the BT upload service is
    included.
- **Size (release 36)**:
  - about 114 KB of code: far 66,518 B, R2A 39,888 B, R1 6,784 B, start-up
    stub 844 B; about 23 KB free
  - RAM: 20,448 B `.data` + `.bss`, 15,208 B frame, 5,048 B spare
  - game pack: Freedoom E1M1S, 1,056,192 B
- **Speed** [HW]: about 22–24 fps close to walls, 8–14 fps in far views
  (about 70–75 KB of chip reads a frame). Batching 16 columns per LCD
  window doubled the close-up rate. Drawing dominates far views; the music
  costs nothing measurable.
- **Keys**: 2/5 move, 4/6 turn, 1 fire, 3 use, 7 music. At power-on: 2
  for upload mode (gives the heap back, requests mode 3, wakes app_core
  every 10 ms), 3 for the USB loader.
- **Sound**: DMC100's `s_pcspk.c`, played through `td_sound.c` (a 140 Hz
  stepper turned into 11025 Hz square waves). **Music**: `music.py` turns
  MIDI into H9MU, and `td_music.c` plays it with 7 square voices and 1 drum
  voice.
- **Build**: set the machine up once with [setup/README.md](setup/README.md)
  (Python tools, stock firmware, toolchain, Docker images, DMC100), then
  `build.py` with no arguments. Host tests: `test/host_test.py`
  (Docker `h9-jieli-dev`). Release: `stage_release.py`. Web install:
  `dist/doom/` (`/doom/`).

## 10. Dead ends (do not repeat)

- **Starting the boot entry in the far region**: black screen. Use the cave
  stub.
- **Encrypted code in the far region**: hangs. Store it plain.
- **A NULL zone base for DMC100** (`I_ZoneBase`): `Z_Init` overwrote RAM
  code at address 0 and hung after PACK OK. Use a real 1 KiB buffer.
- **Bluetooth beside Doom**: heap exhaustion leads to a reset loop. Use the
  separate upload mode.
- **Sound** (builds 14–23): driving the DAC registers, volume, DAC CON
  bit 24, mode 3, PA0/PC pins and the FM power-up all failed. The answer was
  the PA3 direction.
- **Upload speed**: 240 MHz made no difference; waking app_core every 2 ms
  (style F) was no better than every 10 ms and wrote more slowly.
- **`0xFB000` data**: a leftover from early Doom builds whose far blobs
  filled 18 sectors. Nothing reads it; it is not a system write.

## 11. Open questions

- What stretches Bluetooth write ACKs to about 260 ms in Doom's upload
  mode (§8).
- Multi-key input: the keypad is two ADC resistor ladders (§2). Two keys on
  the same ladder cannot be told apart; one on each channel might be read
  together by sampling both channels ourselves [HYP].
- The APRS MCU's identity, and its firmware.
- Faster far-view rendering (texture caching, overlapping reads with
  drawing).
