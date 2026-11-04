# TD-H9 replacement firmware source, first pass

This is the beginning of a source rewrite for the **main Pi32v2 firmware** and the separate **ARM APRS controller**. It is not a complete replacement image and must not be flashed. Source behavior below is tied to the captured stock v1.0.38 main app (`app.bin` SHA-256 `11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`) and APRS v1.0.15 image (SHA-256 `1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`). Neither vendor image is checked into the repository.

## Implemented source

- `common/h9_aprs_wire.*` encodes and decodes the `0xC0`-delimited, `0xDB`-escaped serial framing observed at APRS addresses `0x08004894` and `0x08005B8C`.
- `aprs/h9_aprs_controller.*` implements the observed command dispatch for `0x10`, `0x11`, `0x13`, and `0x20`, packet forwarding, and the ADC callback ordering at `0x08002EF8`: demodulate every sample before an optional TX waveform update. Board-specific UART, GPIO, ADC, DAC, modem, and packet TX operations are explicit callbacks.
- `aprs/h9_aprs_bitstream.*` ports the recovered-bit framing path at `0x080055C4`, including HDLC flag recognition, stuffed-zero removal, byte assembly, and the original `0x1B` escape convention. The ADC-to-bit demodulator and frame CRC still need reconstruction.
- `aprs/h9_aprs_ring.*` ports the receive-byte queue's full, empty, and wrap behavior from `0x080053E0..0x08005470`; the selected Ghidra report is in `../h9-firmware-research/evidence/aprs-bitstream-helpers.txt`.
- `main/h9_main_aprs.*` implements source-level TX and RX-audio policy. The TX enum comes from main app+`0x05C2D4` and clone offset `0x3117`. B-Lock retains the stock current-side guard, while the replacement policy requires both selected side and programmed TX frequency to match B before keying. The mute policy leaves the selected APRS decoder feed enabled while asking **both** speaker and Bluetooth paths to be silent. No hardware gate has yet been identified that can enact both requests without cutting decoder input.

The framing and command source follows observed behavior where established. The mute and post-setup TX verification are **new desired behavior**, not claims that stock firmware already performs them. Unsupported commands are ignored in this first pass. The full main UI, channel store, RF driver, Bluetooth driver, boot setup, and APRS modem/demodulator are still absent. The APRS status command's 48-byte content remains a hardware callback because its fields have not been fully reconstructed. No Pi32 build toolchain or link map has been validated for the radio; CMake currently builds a **host test executable only**.

## Build and test on Windows

With Visual Studio Build Tools installed:

```powershell
cmake -S firmware-research/replacement-source -B firmware-research/replacement-source/build -G "Visual Studio 17 2022" -A x64
cmake --build firmware-research/replacement-source/build --config Debug
ctest --test-dir firmware-research/replacement-source/build -C Debug --output-on-failure
```

On Linux, select an available native C compiler and omit the Visual Studio generator and `-A x64`. These tests do not access the radio.

## Next source work

1. Recover the main-to-APRS serial handler and wire it to the controller's incoming packet path.
2. Reconstruct the APRS ADC-to-bit demodulator (`0x08002308`), frame CRC, and TX waveform generator (`0x08002468`) from the ARM image; connect them to the recovered-bit path already ported.
3. Identify the main radio's APRS ADC tap and the two audible-output gates; connect the mute policy only after that signal path is established.
4. Replace Pi32 RF preparation and TX state restoration with source-backed implementations, then verify the actual emitted frequency into a dummy load.
5. Implement boot, storage, UI, Bluetooth, interrupts, and board initialization. Build both target images, validate update formats, and test recovery before a radio trial.

See [the firmware research map](../h9-firmware-research/APRS_CODE_MAP.md) for the observed offsets and limits of the current evidence.
