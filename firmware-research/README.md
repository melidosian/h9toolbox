# TD-H9 firmware research

This folder collects the TD-H9 v1.0.38 firmware investigation and tools. It is research material, not a complete firmware source tree. The H9 Toolbox web programmer in `dist/` uses Bluetooth to edit radio configuration; these command-line tools target separate USB update interfaces.

## Start here

- [Firmware analysis and recovery record](h9-firmware-research/README.md): container layout, APRS module, audio and transmit routing, startup logo, failed logo experiment, and successful HID recovery.
- [APRS code map](h9-firmware-research/APRS_CODE_MAP.md): current hypotheses and the decoded receive/transmit paths.
- [APRS TX route models](h9-firmware-research/aprs_tx_route_model.py): offline reconstruction of the shared TX CH selector and its [timed-beacon caller](h9-firmware-research/aprs_timed_beacon_model.py); neither measures RF.
- [Analysis scripts and text evidence](h9-firmware-research/): extraction, repacking, Pi32 tracing, APRS inspection, Ghidra scripts, and selected generated reports under `evidence/`.
- [Replacement firmware source, first pass](replacement-source/README.md): compilable C for the observed APRS serial protocol and controller commands, plus new main-radio APRS mute and TX policy. This is a host-tested source core, not a complete target image.
- [Windows/Linux USB-C flasher](h9-flasher/README.md): the BR23 mass-storage update path. Its vendored JieLi helper retains the upstream MIT license in `vendor/LICENSE.jl-uboot-tool`.
- [Browser HID update page](../dist/firmware-update/): a separate, unlinked WebHID path at `/firmware-update/` for the verified official v1.0.38 package. It requires a raw 0xDA000-byte main-flash backup from the same radio, validates the protected boot bytes locally, compares every accessible sector CRC before writing, writes changed sectors from high addresses down with the app header last, and verifies each sector and the final image by CRC. The full browser flow has not yet been tested on a physical radio. HID cannot create a byte-for-byte backup or touch the first 0x5000 bytes; the [command-line flasher](h9-flasher/README.md) remains the recovery path.
- [Early browser USB probe](h9-usb-probe/index.html): exploratory WebUSB UI. The mass-storage update device cannot be accessed through WebUSB; use the Python flasher for that interface.

## Hardware findings

The stock main firmware image and APRS module are separate. The startup logo is addressed through resource bank 5 at logical `0xA00000`; the supplied main `.fw` does not contain those resource bytes. An attempted redirect into the main flash produced a black screen despite a verified write. **Do not flash that diagnostic image.** Holding PTT+2 during power-on exposed a `4C4A:4155` HID updater. The original main-flash sectors were restored using `restore_logo_over_hid.py`, and the user confirmed the normal channel screen and saved channels returned. The recovery record explains the exact validation and its limits.

The built-in speaker GPIO3 mute probe silenced the local speaker while Bluetooth audio still carried RF and APRS bursts. It did not establish whether decoding continued. The requested APRS-only mute, including Bluetooth, is still research. A timed beacon also made the A display line turn red with B-Lock selected and Dual Watch on; actual RF frequency was not measured. Do not treat that display observation alone as proof of the transmitted frequency.

## Inputs and scope

Official firmware packages, extracted application bytes, APRS images, full-radio backups, diagnostic packages, update logs, and generated artwork patches are intentionally absent. These may be vendor material or contain private radio settings. File hashes and analysis manifests remain so local inputs can be checked. Place private inputs in a directory outside the repository. Optional integration tests use `H9_STOCK_FW` and `H9_PREUPDATE_BACKUP`; the historical GPIO3 PowerShell scripts use `H9_STOCK_FW` and `H9_GPIO3_BACKUP`.

Generated reports in `h9-firmware-research/evidence/` are snapshots of the investigation. Some were produced before the Pi32 byte-offset and Sleigh fixes; use the corrected reports and the code map when findings differ. The scripts need local firmware input to reproduce them. No hardware access is required for the offline tests.

Run from this repository root:

```powershell
py -3 -m unittest discover -s firmware-research/h9-firmware-research -p test_firmware.py
py -3 -m unittest discover -s firmware-research/h9-firmware-research -p test_aprs_flasher.py
py -3 -m unittest discover -s firmware-research/h9-firmware-research -p test_aprs_tx_route_model.py
py -3 -m unittest discover -s firmware-research/h9-firmware-research -p test_aprs_timed_beacon_model.py
py -3 -m unittest discover -s firmware-research/h9-flasher -p test_h9_flasher.py
```

On Linux, use `python3` in place of `py -3`. Any USB flash or restore must follow the tool-specific README, identify the current device, and use a verified backup from that same radio.
