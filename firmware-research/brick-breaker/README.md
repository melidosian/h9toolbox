# TD-H9 Brick Breaker

This folder is the independently buildable source and tool set for the
stock-UI Brick Breaker image tested on one TD-H9. On normal boot, the radio
shows its usual channel screen. Holding **Back** for about one second launches
the game; a short Back tap continues to work in the stock menus. In the game,
**Menu** moves the paddle left, **Back** moves it right, and **Up** restarts after
GAME OVER. Power-cycle without holding Back to return to the stock UI.

The owner reported that normal boot, short Back taps, the launcher, display,
and game all worked. This is a stock-derived hybrid: it keeps the official
v1.0.38 reset path, `app_core`, key scanning, and low-level LCD transport.
It is not a standalone replacement main firmware. Radio functions may remain
active during the game; game-mode transmit inhibition has not been added.

## Contents

- `src/game.s`: Brick Breaker game and LCD routines. Its assembled game bytes
  are identical to the earlier hardware-tested stage-26 game body.
- `src/launcher.s`: stock loop hook that observes Back state and press duration
  without calling the stock decoder again. This preserved short Back taps in
  the hardware trial.
- `src/brick_breaker.ld`, `build.py`: pinned layout, exact stock-input checks,
  assembler/linker invocation, and firmware repacking.
- `trial.ps1`: BR23 identity check, fresh backup, recovery-plan check, flash,
  and complete readback in one Administrator session.
- `tools/h9-firmware-research/restore.py`: pinned PTT+2 HID recovery for the
  five changed sectors, with full accessible-sector CRC scans.
- `tools/`: copies of the Pi32v2 assembler, package and flasher utilities,
  HID protocol code, and BR23 RAM loader needed to build and recover this
  exact image. Third-party notices are in the tool directories.

Generated firmware, official inputs, backups, and logs are ignored by Git.
They are deliberately absent from this public repository. The guided CLI
downloads the Type-C v1.0.38 ZIP from
[`teedubyeah/TDH9` at a pinned commit](https://github.com/teedubyeah/TDH9/blob/1ccb67c2684204b0097533486c545607caaf65b8/Firmware/1.0.38/H9_Update-Type-C_1.0.38.zip),
checks the ZIP SHA-256 `1d1b6f87705f026dc5c5b118786d133cbc7f0589f606c8572581df981492155a`,
extracts only the firmware file, then checks its SHA-256:

```
c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6
```

## One-command guided install

Python 3.11 or newer and a USB data cable are required. On Windows, paste this
into PowerShell or Command Prompt:

```powershell
py -3 -c "import urllib.request as u; exec(u.urlopen('https://raw.githubusercontent.com/melidosian/h9toolbox/main/firmware-research/brick-breaker/bootstrap.py').read().decode())"
```

On macOS or Linux, paste this into a terminal:

```sh
python3 -c "import urllib.request as u; exec(u.urlopen('https://raw.githubusercontent.com/melidosian/h9toolbox/main/firmware-research/brick-breaker/bootstrap.py').read().decode())"
```

The bootstrap copies this source folder into `~/TD-H9-Brick-Breaker/<commit>`
and creates an isolated Python environment with `hidapi==0.15.0` for recovery.
The CLI downloads and checks stock, builds and checks the exact tested package,
then prompts you to put the radio in USB Update mode (power on, hold **8**).
It identifies the single BR23 updater, saves and verifies a fresh backup, and
requires typing `FLASH` before writing the five changed sectors. On Windows,
one Administrator prompt covers backup and flash. Linux needs access to the
radio's `/dev/sg*` device, usually through an elevated terminal or a suitable
udev rule. The complete main image is read back after the write.

On macOS, the CLI uses PTT+2 HID mode instead of BR23 SCSI. Its backup is
reconstructed from CRC windows: it checks the exact stock main region and
recovers the accessible settings tail. The protected boot region comes from
the pinned stock package. This HID install path has passed synthetic offline
tests but has **not** been tested on a Mac radio. It may take substantially
longer than the USB Update path. Do not use it if the radio's main image is
not the exact pinned stock version.

To build only, add `build` after the one-line command. The generated `.fw` is
saved in the copied folder's `build/` directory.

## Guided HID restore

From the copied source folder, run `cli.py restore` with its `.venv` Python,
or run the same bootstrap one-liner with `restore` appended after the closing
quote. The CLI selects the latest backup in that package folder; pass
`--backup PATH` to choose a different backup. It verifies the pinned package,
backup and live HID CRC map, then requires typing `RESTORE`. Enter HID mode by
powering off and then on while holding **PTT+2** with the data cable connected;
the top LED should be solid green. After recovery, disconnect USB and boot
normally. Keep both the backup `.bin` and adjacent `.json` manifest.

The CLI cannot press radio buttons or force the device into either update mode.
USB and HID access depend on the host's driver and permissions. Only one radio
has been hardware-tested; automatic install on other radios has not been
validated. The CLI stops before writing if it cannot establish the expected
stock backup and recovery plan.

## Manual build

Use Python 3.11 or newer. From this folder in PowerShell:

```powershell
$stock = 'C:\path\to\TD-H9-V1.0.38.fw'
py -3 tools/h9-firmware-research/firmware.py extract $stock --output input/stock
py -3 build.py --stock-image input/stock/app.bin --stock-package $stock
py -3 tools/h9-flasher/h9_flasher.py inspect build/h9_brick_breaker_hotkey_OFFLINE_EXPERIMENT.fw --stock-base $stock
```

The extracted `app.bin` must be 867,372 bytes with SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The package built and hardware-tested here has SHA-256
`208409d8d7e335904f70f9e28e27e9c413c6296a3d185a69a55707ac8ddaee68`.
Its 892,928-byte main-flash image has SHA-256
`741bbeb87eb84cd7c6988054c56a267bb73c89bbe5cc74dc4e5bc0d75ce7e193`.
The builder rejects any changed stock input, unexpected hook bytes, occupied
code cave, altered game machine code, or changed sector set. Exactly five
4 KiB sectors differ from stock: `0x5000`, `0x8D000`, `0xAC000`, `0xAE000`,
and `0xD9000`.

## Flash and recover

The tested radio entered BR23 USB Update mode over a USB-A-to-C data cable
by powering on and holding **8**. Run the combined trial in an Administrator
PowerShell session, passing a new backup path outside this folder:

```powershell
./trial.ps1 -Action trial -DiskNumber 1 -StockPackage $stock -BackupPath 'C:\path\to\h9-before-brick-breaker.bin'
```

Check the actual BR23 disk number before using the example. The script refuses
to write unless the updater identity, stock package and generated package
hashes, fresh radio backup, and HID recovery plan all match the pinned values.
It verifies the entire main-flash readback after the five-sector write. The
backup is required for rollback; keep its adjacent `.json` manifest.

To restore the pinned stock backup, power off, connect the data cable, and
power on while holding **PTT+2**. The top LED should be solid green. Python
`hidapi` is required for the HID restore (`py -3 -m pip install hidapi`):

```powershell
py -3 tools/h9-firmware-research/restore.py inspect --backup 'C:\path\to\h9-before-brick-breaker.bin' --package build/h9_brick_breaker_hotkey_OFFLINE_EXPERIMENT.fw
py -3 tools/h9-firmware-research/restore.py restore --backup 'C:\path\to\h9-before-brick-breaker.bin' --package build/h9_brick_breaker_hotkey_OFFLINE_EXPERIMENT.fw --yes
```

The pinned HID restore accepts a backup identical to the official v1.0.38
main-flash image. Another radio with different main-flash bytes needs its own
validated backup and recovery plan. The generic BR23 flasher is included for
inspection and full-image USB Update recovery, but the HID path above was the
one exercised on the test radio. Keep PTT untouched during gameplay because
stock `app_core` still runs.

## Provenance

The Pi32v2 assembler source and instruction table were copied from
[`jvitkauskas/pi32v2-isa`](https://github.com/jvitkauskas/pi32v2-isa), commit
`66ac37bb2ae40e5dbbd5588a6a7c2d7baf829d7f`, under MIT-0; see
`tools/pi32v2-isa/LICENSE`. The BR23 loader and USB/SCSI transport derive
from `kagaimiq/jl-uboot-tool` under MIT; see
`tools/h9-flasher/vendor/LICENSE.jl-uboot-tool`. The exact stock firmware and
radio backup are not included.
