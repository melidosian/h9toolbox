# Changelog

## H9 Toolbox Firmware v0.1.7

### Quick Change

- Hold **Back** from the channel screen to open Quick Change, even with
  Monitor on or the squelch set to 0. Hold **#** to start scanning and
  hold **\*** to lock or unlock the keypad.
- Use **1–9** for Power, Squelch, Tone, Scan Resume, Monitor or VFO TX
  frequency, VOX, Dual Watch, Width and Brightness. Press **0** for
  Radio Settings.
- Brightness cycles and updates on the grid. Squelch can be raised from
  level 0 while the receiver is open.

### Toolbox menu

- Press **Menu** on the channel screen to open Toolbox. The unified menu
  brings together radio settings, channel editing and APRS.
- The Type-C TD-H9 release includes the optional on-radio APRS modem
  update and a matching App Manager catalog.

### APRS app v1.0

- The six-message on-radio inbox and outbox now survive power cycles in a
  guarded two-sector resource-flash journal. Outgoing messages retain their
  packet ID and ACK state. Other stations' messages no longer evict them.
- Messages distinguish incoming and outgoing traffic. Outgoing messages show
  SENT until an ACK or REJ from the recipient is heard; the detail shows
  NO ACK YET, ACK RECEIVED or REJECTED.
- The mailbox refuses writes when the game directory claims either sector,
  the sector contains foreign data, or the saved callsign differs. The Messages
  screen reports when storage is unavailable. A first interrupted write can
  resume for the same callsign; later writes preserve the previous commit.
- The game uploader and firmware S2 service reserve the six-sector voice tail
  starting at 0x3FA000 for the mailbox, settings, VFO recovery and GPS fix.
- Menu on a message detail opens Send addressed to that station. Send remembers a
  draft while switching APRS screens, and focuses the message field when a
  recipient is already selected. Send from Map uses its highlighted station.
- The matching App Manager catalog includes the hidden mailbox worker.
- Station notes, messages and send previews wrap at word boundaries on the
  radio's narrow screen.

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
