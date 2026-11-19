# TD-H9 on-radio APRS updater

H9 Toolbox Firmware v0.1.5 bench build for the Type-C TD-H9. The
computer stages a matching updater and the approved RX15 and unsigned-byte
APRS 1.0.15 image in spare main flash. At a later cold boot, the radio sends
the image over its internal UART with USB disconnected. The first build
accepts only this specific modem image. The Toolbox **ABOUT** app reports
`FIRMWARE V0.1.5`. The APRS modem reports its own version, `V1.0.15`.

**Hardware status.** USB readback verified the v0.1.5 main firmware and
stage on the resident-beacon bench radio. With USB disconnected, PTT+7
and Menu reached **APRS STAGE READY**, and Back returned to normal
operation. The subsequent staged write displayed **APRS BOOT VERIFIED /
V1.0.15**. After a normal power cycle, APRS Info showed `V1.0.15` and
Toolbox ABOUT showed `FIRMWARE V0.1.5`.

## Build

Use [the JieLi toolchain](../toolchain/) and the official Type-C
`TD-H9-V1.0.38.fw` referenced in the [setup guide](../setup/README.md). From
this directory, run:

```sh
python3 build.py --stock-package "/path/to/TD-H9-V1.0.38.fw"
cc -std=c11 -Wall -Wextra -Werror update.c test_update.c -o build/test_update
build/test_update ../kiss-tnc/APRS-1.0.15-rx15-rxff.bin
python3 -m unittest test_stage
```

The default base is the pinned KISS package. To preserve the separately built
resident-beacon and live-squelch changes on a bench radio, pass that exact
package with `--base-package`. The manifest identifies the base and the
firmware version. The standard site installer still uses its own hosted build.

The outputs are `build/h9-kiss-aprs-stage.fw`,
`build/aprs-stage-0xf4000.bin`, and `build/manifest.json`. Keep the three
files together. The main package installs the launch and integrity hooks.
The stage contains the updater code and the modem image at main flash
`0xF4000–0xF9FFF`, below the resident beacon sectors at `0xFA000–0xFBFFF`.
The host verifies device checksums, then the main firmware
checks the updater code before executing it and the updater checks the
modem image before sending it.

## Stage and install

1. Have a charged radio, the official main firmware and the working Windows
   APRS recovery path ready. Close any radio flasher that might be using
   the USB port.
2. Connect USB and power on with **PTT+2** for the main HID updater. On the
   computer, install the Python `hidapi` package in a virtual environment.
   Run `python3 stage.py` to inspect the slot. If the slot is available,
   run `python3 stage.py --write`. This programs only erased staging sectors
   and checks aligned and shifted device checksums. It uses the main
   flash's HID interface.
3. On Windows, install `build/h9-kiss-aprs-stage.fw` with the [main USB
   flasher's stock-derived package procedure](../h9-flasher/README.md#custom-main-image-offline-validation-and-recovery)
   using **hold 8** at power-on, the official Type-C file as `--stock-base`,
   and a fresh backup path. The package and staging slot are separate. The
   main USB flasher writes below `0xDA000`, so the verified stage stays in
   place. Check a normal boot before trying the update.

The same staging command can resume when the slot contains only complete
sectors from the matching build and erased sectors. A partial write or a
previous Doom installation can occupy the slot. If it reports other data,
stop at inspection and preserve the slot until its contents can be backed
up and identified.

## Run on the radio

1. Power off, unplug USB, and power on with **PTT+7**. Release both keys
   when **APRS Update** appears.
2. Press **Menu** once. **APRS STAGE READY** means the staged updater has
   started and validated the image. For the first hardware check, press
   **Back** here to cancel, then confirm the usual **APRS Info** version
   appears in normal operation. If the stock screen remains after pressing
   Menu, check that the staged bundle matches the installed main package.
3. Repeat the boot sequence with **PTT+7**, press **Menu**, then release
   it. At **APRS STAGE READY**, press **Menu** again to write. Leave power on.
   The radio waits for 20 ms of quiet on the internal UART and sends 136
   blocks at the stock update setting of 19,200 baud. It sends each block
   once and waits for the modem's acknowledgment before continuing.
4. Wait for **APRS BOOT VERIFIED / V1.0.15**. After the modem acknowledges
   exit, the radio sends the normal launch command, switches to 115,200 baud,
   and requires a framed modem version reply. Open **APRS Info** again
   after a normal power cycle. A version reply verifies a running modem
   application. To verify the KISS patch, also check that a short AX.25
   control frame passes in both directions.

If the radio reports **NO BLOCK ACK** or **NO EXIT ACK**, power off. Re-enter
**PTT+7** with USB connected and keep the radio on the stock **APRS Update**
screen. Use the known-good Windows [APRS procedure](../kiss-tnc/README.md)
with the stock v1.0.15 modem image, then check **APRS Info** after a normal
power cycle. Use the same recovery path if the updater says **APRS NOT
VERIFIED** and APRS Info still shows **Querying…**. Use the saved main-app
backup to restore the main firmware if normal main boot fails. The [PTT+2
main recovery procedure](../hid-recovery/README.md) remains available.

See the [stock startup and UART trace](../h9-firmware-research/evidence/stock/aprs-update-entry.md)
for the write, exit and boot handshake.
