# PTT+2 recovery: restore any main firmware to stock

`restore_stock.py` puts the radio's main app (`0x5000–0xD9FFF`, 213
sectors) back to exact stock v1.0.38 over the PTT+2 HID updater
(`4C4A:4155`), whatever custom firmware is installed. It is the recovery
path for Doom and other takeover builds, which may disable hold-8.

1. Power the radio on holding **PTT+2**.
2. Read only, to see what differs:

   ```
   ~/h9usb/bin/python restore_stock.py inspect --backup ~/h9-backups/h9-stock-v1.0.38-restore-point.bin --known <installed.fw>
   ```

3. Restore:

   ```
   ~/h9usb/bin/python restore_stock.py restore --backup ~/h9-backups/h9-stock-v1.0.38-restore-point.bin --known <installed.fw> --yes
   ```

4. Power-cycle. The radio runs stock. To install H9 Toolbox Firmware, use
   the web flasher (PTT+2) or hold-8.

How it works:

- It scans every app sector's CRC and rewrites each one that differs from
  the restore point.
- It also always rewrites the sectors any `--known` package changed, which
  covers the 1-in-65,536 chance of a CRC collision. `--all` rewrites
  everything.
- Writes go highest sector first, so the app header (`0x5000`) is written
  last.
- At the end it rescans and requires all 213 sectors to match.
- Re-running after an interruption continues where it stopped.
- It never writes past `0xD9FFF`: the settings VM and the far region are
  left alone. Stock ignores the far region.

Offline tests against a simulated radio: `~/h9usb/bin/python -m unittest test_restore_stock`.
No sudo is needed (HID, with `hidapi` in `~/h9usb`).
