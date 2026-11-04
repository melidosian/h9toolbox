# TD-H9 USB-C flasher

This Python 3.10+ command-line tool targets the **TD-H9 BR23 USB update mode** on Windows and Linux. It uses the device's USB mass-storage SCSI interface, which browser WebUSB cannot open. Ordinary writes accept only the supplied official `TD-H9-V1.0.38.fw` package (SHA-256 `c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6`). An explicit `--stock-base` option validates an exact app-only repack of that package. A one-byte diagnostic main image has now been flashed and booted, with APRS behavior still unverified. Python's standard library is sufficient; the JieLi protocol code and RAM loader are included under `vendor/`.

Hardware status: a Windows backup completed and the user reported a successful flash on 2026-11-03. The final console output and post-reboot firmware version were not independently captured. Linux hardware access has not been tested. Keep the official updater and a reliable way to recover the radio available. `inspect` is entirely offline; `probe` sends only SCSI INQUIRY. `backup` runs a temporary helper in the radio's RAM, checks its chip key and flash header, and saves the flash area being updated without erasing or writing flash. `flash` performs those checks, verifies or saves a backup, updates only sectors that differ, verifies each changed sector, then reads back the whole image. It preserves the area from `0xDA000` onward, including the reserved region at `0xFF000`.

## Windows

Use a data-capable USB-C cable and put the radio in USB update mode. Open **PowerShell as Administrator**. Identify the radio's disk number from the `BR23 UBOOT1.00` model; do not guess or use the number in this example without checking:

```powershell
Get-CimInstance Win32_DiskDrive | Select-Object Index, Model, DeviceID, PNPDeviceID
```

From this directory:

```powershell
py -3 h9_flasher.py inspect "C:\path\to\TD-H9-V1.0.38.fw"
py -3 h9_flasher.py probe --device '\\.\PHYSICALDRIVE1'
py -3 h9_flasher.py backup "C:\path\to\TD-H9-V1.0.38.fw" --device '\\.\PHYSICALDRIVE1' --backup "C:\path\to\h9-before-update.bin"
py -3 h9_flasher.py flash "C:\path\to\TD-H9-V1.0.38.fw" --device '\\.\PHYSICALDRIVE1' --backup "C:\path\to\h9-before-update.bin" --yes
```

Replace `PHYSICALDRIVE1` with the verified radio disk. If opening the disk returns `Access is denied`, use an elevated PowerShell session. After `backup`, power-cycle the radio and re-enter update mode before `flash`; then verify the disk number again. `flash` accepts an existing backup only if it exactly matches the current radio contents. This build also requires the `0x856014` SPI NOR ID observed during the first TD-H9 backup. Keep the radio attached until the command reports that readback verification passed.

## Linux

Identify the radio's SCSI generic device by its `BR23 UBOOT1.00` INQUIRY identity. `lsscsi -g` lists both the block and generic paths, when installed. Recheck the path every time you reconnect the radio, since `/dev/sgN` numbering can change. Your account needs read/write access to the SCSI generic device; root is simplest for initial use.

```sh
python3 h9_flasher.py inspect /path/to/TD-H9-V1.0.38.fw
sudo python3 h9_flasher.py probe --device /dev/sg2
sudo python3 h9_flasher.py backup /path/to/TD-H9-V1.0.38.fw --device /dev/sg2 --backup /path/to/h9-before-update.bin
sudo python3 h9_flasher.py flash /path/to/TD-H9-V1.0.38.fw --device /dev/sg2 --backup /path/to/h9-before-update.bin --yes
```

Replace `/dev/sg2` with the path verified by `probe`. Store the backup on the computer, not the radio. Power-cycle and re-enter update mode after `backup`, then recheck the `/dev/sgN` path. The command refuses a mismatched bootloader, chip key, PID, flash capacity, package hash, or an existing backup that differs from the radio. A failed write can leave the radio in update mode; keep the backup and use the official updater or a proven recovery method.

## Offline checks

```sh
python3 -m unittest discover -s . -p 'test_h9_flasher.py'
```

The test using the supplied firmware runs when that file exists at its original Windows download path; other tests run anywhere.

## Protocol provenance

`vendor/jltech/`, `vendor/scsiio/`, and `vendor/data/br23loader.bin` come from [kagaimiq/jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool), commit `adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a`, under the included MIT license. Local changes remove the `crcmod` dependency, correct Windows disk-access flags, and reject failed or short SCSI transfers. The UFW container handling was cross-checked against [TD-H9 firmware tooling](https://github.com/mjwgeek/TIDRADIO_TD-H9/blob/main/tools/patch_td_h9_v1033.py).

The BR23 loader file is stored MengLi-encrypted and the BR23 bootloader decrypts it during RAM upload. The host sends the bundled bytes unchanged.

## Custom main image, offline validation and recovery

`firmware-research/h9-firmware-research/firmware.py` can rebuild a `.fw` from the exact stock package and a same-size replacement `app.bin`. The custom flasher path reconstructs that package again and requires every byte to match; changes to the boot area, chip configuration, package padding, or resource layout are rejected. Inspect offline first:

```powershell
py -3 h9_flasher.py inspect 'C:\path\to\custom.fw' --stock-base 'C:\path\to\TD-H9-V1.0.38.fw'
```

The `backup`, `flash`, `resume`, and `restore` commands accept the same `--stock-base` option. Use a **new** backup filename for the first custom flash. Before any erase, the tool saves the current flash image and a neighboring `backup.bin.json` manifest binding its SHA-256, radio PID/chip key, and the exact target package hash. Custom `resume` and `restore` require that manifest and matching image; stock recovery with the original pinned backup remains available. Keep the backup, manifest, exact custom `.fw`, and stock `.fw` together. Example after a tested custom image exists:

```powershell
py -3 h9_flasher.py flash 'C:\path\to\custom.fw' --stock-base 'C:\path\to\TD-H9-V1.0.38.fw' --device '\\.\PHYSICALDRIVE1' --backup 'C:\path\to\before-custom.bin' --yes
py -3 h9_flasher.py restore 'C:\path\to\custom.fw' --stock-base 'C:\path\to\TD-H9-V1.0.38.fw' --device '\\.\PHYSICALDRIVE1' --backup 'C:\path\to\before-custom.bin' --yes
```

These commands are for the **main** BR23 `.fw`, not the separate APRS controller `.bin`. A valid checksum and exact app-only repack do not establish that modified Pi32 code boots or behaves correctly. The `--stock-base` hardware path succeeded on 2026-11-03 with a one-byte GPIO3-low diagnostic: three sectors were written, each verified, and the full 892,928-byte image matched on readback. The user reported a normal boot and a silent built-in speaker while Bluetooth still carried RF audio; APRS → Beacon List was empty. The custom `restore` path then rewrote three sectors and verified a full readback matching the pre-probe backup. The user confirmed normal boot and restored built-in speaker audio with Bluetooth disconnected. APRS decoding under the probe is still unverified. The diagnostic is not the requested per-channel mute option. See [the bench-probe record](../h9-firmware-research/README.md#speaker-gate-bench-probe-not-the-aprs-mute-option).

If a flash command fails after an erase or write, leave the radio connected and keep the backup. If commands stop responding, power-cycle and re-enter update mode, then verify the device path. `resume` retries the new firmware. `restore` writes the saved pre-update image back. Both require the exact backup hash captured from this radio (`975bf7f3ea6ec76139a8e1b48b402594b2ea34b43a6083532ece543c47ac53ee`) and recheck the bootloader, flash ID, and chip key before writing. For example, on Windows:

```powershell
py -3 h9_flasher.py resume "C:\path\to\TD-H9-V1.0.38.fw" --device '\\.\PHYSICALDRIVE1' --backup "C:\path\to\h9-before-update.bin" --yes
py -3 h9_flasher.py restore "C:\path\to\TD-H9-V1.0.38.fw" --device '\\.\PHYSICALDRIVE1' --backup "C:\path\to\h9-before-update.bin" --yes
```

Do not use these commands for a different radio. Neither command can help if the radio no longer enters USB update mode; the supplied vendor `Update App.exe` also needs USB access. In that case a hardware service or forced ROM entry method may be required.
