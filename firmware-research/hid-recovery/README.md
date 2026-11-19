# Restore the TD-H9 main firmware to stock over USB

Use `restore_stock.py` if a TIDRADIO TD-H9 can still enter **PTT + 2** USB update mode but browser recovery is unavailable or fails. The tool restores the main firmware to stock v1.0.38 and verifies the result. It leaves the protected boot area, saved settings, and APRS modem firmware alone.

## What you need

- The full H9 Toolbox repository. The script imports USB and firmware helpers from nearby folders.
- Python 3.10 or later with the USB dependencies from step 1 of the [setup guide](../setup/README.md).
- A data-capable USB-C cable and a charged radio.
- The verified **raw stock v1.0.38 main-flash restore point**. The script checks that the file is exactly `0xDA000` bytes and has SHA-256 `74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648`. Browser `.h9hid` archives belong in the browser updater. For this script, use the verified raw stock `.bin`.
- The exact custom `.fw` package you installed, if available. Passing it with `--known` makes the tool rewrite the sectors that package changed even if their checksums happen to match stock.

If you don’t have the raw stock restore point, use step 2 of the [setup guide](../setup/README.md) to fetch the verified Type-C v1.0.38 package. From the repository root, run the following command to extract the raw image to a new file called `stock-restore.bin`. No radio connection is needed for this step.

```sh
python -c "import sys; from pathlib import Path; sys.path.insert(0, 'firmware-research/h9-flasher'); from h9_flasher import parse_firmware; image = parse_firmware(Path('firmware-research/setup/stock/TD-H9-V1.0.38.fw')).image; open('stock-restore.bin', 'xb').write(image)"
```

Run the recovery commands from `firmware-research/hid-recovery` with the Python environment that has the USB dependencies installed. Replace `stock-restore.bin` and `installed.fw` with the actual paths to those files. Keep any personal backups outside your Git checkout.

## 1. Connect the radio

1. Turn the radio off and plug it directly into the computer with a data-capable USB-C cable.
2. Hold the side **PTT** button and the keypad **2** while turning the radio on. The screen stays blank and the top light turns green.
3. Close any browser updater tabs or other tools using the radio. If the script can’t find it, try another cable or port and repeat the power-on sequence. The USB device in this mode has ID `4C4A:4155`.

## 2. Inspect before restoring

```sh
python restore_stock.py inspect --backup "stock-restore.bin" --known "installed.fw"
```

This reads the main firmware checksums and shows the restore plan without writing. If you no longer have the custom package, omit `--known "installed.fw"` from both commands. You can add `--all` to both commands to plan and rewrite all 213 main firmware sectors, including sectors whose checksums already match.

If the script rejects the restore point, check the path and the file’s SHA-256 before continuing.

## 3. Restore and verify

```sh
python restore_stock.py restore --backup "stock-restore.bin" --known "installed.fw" --yes
```

Keep the USB cable connected and the radio powered on until the script reports **All 213 app sectors match stock v1.0.38**. If the process stops, reconnect in **PTT + 2** mode, run `inspect` again, and repeat the restore with the same files and options.

Once verification passes, unplug USB, switch the radio off, then switch it back on normally. Confirm that the channel screen comes up and check your saved channels and settings.

## If USB update mode is unavailable

Stock firmware and H9 Toolbox Firmware can also expose the BR23 USB loader by holding **8** at power-on. Use the [BR23 USB flasher instructions](../h9-flasher/README.md) if your computer sees `BR23 UBOOT1.00`. If neither USB mode appears after trying a different cable and port, contact TIDRADIO support with the firmware version, the error you saw, and what the radio does at power-on.

Offline tests can run without a radio. From this directory, run `python -m unittest test_restore_stock`.
