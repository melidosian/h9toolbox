# TD-H9 replacement firmware source, first pass

This is the beginning of a source rewrite for the **main Pi32v2 firmware** and the separate **ARM APRS controller**. It is not a complete replacement image and must not be flashed. Source behavior below is tied to the captured stock v1.0.38 main app (`app.bin` SHA-256 `11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`) and APRS v1.0.15 image (SHA-256 `1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`). Neither vendor image is checked into the repository.

## Implemented source

- `common/h9_aprs_wire.*` encodes and decodes the `0xC0`-delimited, `0xDB`-escaped serial framing observed at APRS addresses `0x08004894` and `0x08005B8C`.
- `aprs/h9_aprs_controller.*` implements the observed command dispatch for `0x00`, `0x10`, `0x11`, `0x13`, and `0x20`, packet forwarding, and the ADC callback ordering at `0x08002EF8`: demodulate every sample before an optional TX waveform update. Board-specific UART, GPIO, ADC, DAC, modem, and packet TX operations are explicit callbacks.
- `aprs/h9_aprs_bitstream.*` ports the recovered-bit framing path at `0x080055C4`, including HDLC flag recognition, stuffed-zero removal, byte assembly, and the original `0x1B` escape convention.
- `aprs/h9_aprs_ring.*` ports the receive-byte queue's full, empty, and wrap behavior from `0x080053E0..0x08005470`; the selected Ghidra report is in `../h9-firmware-research/evidence/aprs-bitstream-helpers.txt`.
- `aprs/h9_aprs_demod.*` ports the five-sample delay line, correlation/filter, timing recovery, and NRZI bit extraction at `0x08002308`, using the initialization at `0x08002604`. The controller now runs this source on each ADC sample before updating the TX DAC.
- `aprs/h9_aprs_frame.*` ports the byte-queue parser at `0x080051FC`: HDLC flags, escaped bytes, a 792-byte receive limit, and the stock CRC-16/X-25 residue check. The controller can drain the demodulator queue and forward a valid packet to the main radio as serial command `0x00`, excluding its two FCS bytes. The source-input verifier checks all 256 CRC table entries against the APRS image.
- `aprs/h9_aprs_oscillator.*` ports the 9-bit phase accumulator and quarter-wave table in the tail of `0x08002468`. The table is copied from stock APRS-image offsets `0x40D8..0x4157`; the 0x40 and 0x75 phase steps are tested against vectors independently calculated from those bytes.
- `aprs/h9_aprs_tx.*` ports the transmit queue, leading/trailing flag counts, eight waveform samples per bit, NRZI tone changes, and stuffed-zero insertion from `0x08002664` and `0x08002468`. The controller can use this scheduler for its DAC output.
- `aprs/h9_aprs_tx_packet.*` ports `0x0800530C`/`0x0800534C`: an opening flag, payload with `0x1B` escapes for reserved bytes, complemented FCS low byte first, and a closing flag. Its resumable pump handles the stock 64-byte transmit queue without a blocking wait. The byte stream round-trips through the receive frame parser in host tests. The serial command `0x00` is now dispatched to a synchronous board callback for the stock `0x08005760` send path.
- `aprs/h9_aprs_tx_gate.*` ports the `0x0800211C` random generator and the busy-channel, persistence, and receive-event decisions from `0x08005760` into a cooperative step. Its inputs are abstract until the board's receive/busy signals are mapped.
- `aprs/h9_aprs_tx_service.*` owns a serial TX request, waits for the gate, then feeds the packet encoder into the waveform queue as samples drain. A host test sends an 80-byte packet through serial command `0x00` and this service, then loops its generated samples through the ported receive demodulator and frame parser; the original packet is recovered. This is an ideal software loopback, not a recorded RF or board ADC/DAC test. The board must still supply carrier/receive signals and handle PTT and DAC timing.
- `main/h9_main_aprs.*` implements source-level TX and RX-audio policy. The TX enum comes from main app+`0x05C2D4` and clone offset `0x3117`. B-Lock retains the stock current-side guard, while the replacement policy requires both selected side and programmed TX frequency to match B before keying. The plan records when an Auto mode must restore the prior side after sending, matching the stock selector's temporary side switch. The mute policy leaves the selected APRS decoder feed enabled while asking **both** speaker and Bluetooth paths to be silent. No hardware gate has yet been identified that can enact both requests without cutting decoder input.
- The same main module ports the channel-state bit update at app+`0x05A538..0x05A676`: when the working frequency matches A or B, it clears or sets bit 7 of RAM `0x00011B50`. This bit feeds the stock APRS TX selector. Its caller's timing relative to Dual Watch and the highlighted display line remains unverified; a B bit alone does not prove the RF frequency emitted during a beacon.
- The same main APRS module ports the packet-path RX-channel comparison at app+`0x086166..0x08619C`. This comparison maps the working RX frequency to A or B before checking `APRS RX CH`; `Off` bypasses this local comparison, which does not establish that APRS reception is globally enabled in that setting.
- `main/h9_main_beacon.*` ports the timed-beacon counter, interval threshold, and state/priority guard from main app+`0x082EB2`. The outer scheduler timing and receive/busy conditions remain to be ported.

The framing and command source follows observed behavior where established. The mute and post-setup TX verification are **new desired behavior**, not claims that stock firmware already performs them. Unsupported commands are ignored in this first pass. The full main UI, channel store, RF driver, Bluetooth driver, boot setup, and APRS hardware integration are still absent. The receive chain has passed ideal software loopback but has not been validated with recorded packet audio. The APRS status command's 48-byte content remains a hardware callback because its fields have not been fully reconstructed. No Pi32 build toolchain or production link map has been validated for the radio. The CMake test executable is host-only; the cross-compile script below creates target objects and archives, not linked firmware.

## Build and test on Windows

With Visual Studio Build Tools installed:

```powershell
cmake -S firmware-research/replacement-source -B firmware-research/replacement-source/build -G "Visual Studio 17 2022" -A x64
cmake --build firmware-research/replacement-source/build --config Debug
ctest --test-dir firmware-research/replacement-source/build -C Debug --output-on-failure
```

On Linux, select an available native C compiler and omit the Visual Studio generator and `-A x64`. These tests do not access the radio.

## Cross-compile the source modules

`build_target_objects.py` compiles the current source sets separately and records the compiler, flags, source hashes, object hashes, and archive hash in `build-manifest.json`. It runs on Windows or Linux. The APRS build uses [Arm GNU Toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain)'s `arm-none-eabi-gcc` and `arm-none-eabi-ar`. The conservative `cortex-m0`/Thumb setting only establishes a compatible ARM object build; the precise APRS CPU variant has not been confirmed. The main build requires JieLi's **Pi32v2 r3** clang and LTO archiver. The [official JieLi SDK build](https://doc.zh-jieli.com/SDKWiki/master/Jieli-Tech/fw-AC63_GP_MCU/5-build-tools/1-build-system-workspace.html) uses `C:/JL/pi32/bin` on Windows and `/opt/jieli/pi32v2/bin` on Linux, with `-target pi32v2 -mcpu=r3 -flto -Oz`.

```powershell
py -3 firmware-research/replacement-source/build_target_objects.py aprs --compiler 'C:\path\to\arm-none-eabi-gcc.exe'
py -3 firmware-research/replacement-source/build_target_objects.py main --compiler 'C:\JL\pi32\bin\clang.exe'
```

On Linux, use `python3` and omit `--compiler` if the respective compiler is at its documented path or on `PATH`. `H9_ARM_GCC` and `H9_PI32_CLANG` can also set explicit compiler paths. The resulting `.o` files and `.a` archive are under ignored `build/target-aprs` or `build/target-main`. The APRS archive was built and checked with Arm GNU Toolchain 15.3.1 on Windows. **Neither archive is a bootable or flashable image**: startup, linker memory map, peripheral drivers, and final image packaging are still missing. The Pi32 compilation path remains untested until that vendor compiler is available.

To check the source constants against the private stock images, run `py -3 firmware-research/replacement-source/verify_source_inputs.py <path-to-stock-app.bin> <path-to-APRS-1.0.15.bin>` from the repository root. This reads both files and validates their pinned hashes, the main TX selector call/table, all 128 waveform-table bytes, and the APRS CRC table. It does not create or flash an image.

## APRS diagnostic link check

`link_aprs_check.py` compiles all APRS modules, forces every object into one ARM ELF, and writes a linker map and manifest under ignored `build/link-check-aprs/`. Its diagnostic linker script uses the stock image base `0x08002000`, stock image length 17,304 bytes, and stock initial stack pointer `0x20001A50` as conservative address bounds. The optional stock-image argument rechecks those values against the pinned private image hash. This is a **link check**, not a complete firmware build: its 16-entry vector table and inert reset handler do not initialize RAM, clocks, interrupts, UART, ADC, DAC, radio controls, or the APRS application. It creates no `.bin` update file.

```powershell
py -3 firmware-research/replacement-source/link_aprs_check.py --compiler 'C:\path\to\arm-none-eabi-gcc.exe' --stock-image 'C:\path\to\APRS-1.0.15.bin'
```

The Windows Arm GNU Toolchain 15.3.1 check linked 11 source objects into a 4,144-byte diagnostic ELF and confirmed its vector table. This establishes that the current modules have no unresolved link symbols within that diagnostic envelope; it does not establish that the radio can run them.

## Next source work

1. Recover the main-to-APRS serial handler and wire it to the controller's incoming packet path.
2. Validate the ported ADC-to-packet receive chain with recorded APRS audio, then map the board's carrier-sense and receive-event signals and connect the send gate to packet encoding, PTT, and DAC hardware.
3. Identify the main radio's APRS ADC tap and the two audible-output gates; connect the mute policy only after that signal path is established.
4. Replace Pi32 RF preparation and TX state restoration with source-backed implementations, then verify the actual emitted frequency into a dummy load.
5. Implement boot, storage, UI, Bluetooth, interrupts, and board initialization. Build both target images, validate update formats, and test recovery before a radio trial.

See [the firmware research map](../h9-firmware-research/APRS_CODE_MAP.md) for the observed offsets and limits of the current evidence.
