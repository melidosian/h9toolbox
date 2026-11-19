# Changelog

## H9 Toolbox Firmware v0.1.5

### Firmware and installer

- The Type-C TD-H9 browser installer checks and stages the APRS modem
  upgrade while installing H9 Toolbox Firmware. It verifies the stage
  before writing the main firmware and preserves occupied staging space.
- Optional **Step 5 · Unlock KISS TNC** explains how to unplug USB, start
  the radio with **PTT + 7**, and finish the modem update from the keypad.
  The updater validates the image and checks the modem's version reply
  after flashing. **V1.0.15X** identifies the patched APRS modem.
- The KISS firmware passes 15-byte AX.25 control frames through Bluetooth
  and preserves received data bytes with value `0xFF`.
- App Manager can connect over USB-C as an experimental option.
  The APRS Beacon app can keep Smart Beacon active when returning to
  the radio with **Back**.
- Earlier Toolbox packages are recognized for backup and upgrade.
  Restoring the main firmware leaves channels, settings, and the
  separate APRS modem firmware in place.

### Packet software

- **Step 5** includes a downloadable bridge ZIP with setup instructions.
  The Python bridge connects to the H9 over Bluetooth and serves plain
  KISS TCP on `127.0.0.1:8100`.
- Pat and Paracon can use the bridge's built-in AX.25 and AGWPE endpoint
  on `127.0.0.1:8000`, radio port **0**. The Pat launcher starts the bridge
  and Pat directly.
- The bridge defaults to receive-only mode. `--allow-tx` enables RF
  transmission, and the terminal shows live AX.25 RX and TX.

### Validation

- A Type-C bench radio completed the unplugged modem flash and still
  showed **V1.0.15X** in APRS Info after reboot.
- Pat completed secure login and uploaded one test message over the air
  through KC9GMW-12 with the built-in AX.25 bridge. Pat moved the
  message to Sent. Inbox loopback delivery remains unconfirmed.
- The Pat and Paracon client libraries exchanged data through a simulated
  radio. The browser installation flow also passed simulation, including
  stage verification, an occupied staging slot, and main firmware restore.
  The exact browser package still needs a physical install check.

See the [firmware build changelog](firmware-research/module-loader/CHANGELOG.md)
and [bridge setup guide](firmware-research/kiss-tnc/host/README.md).
