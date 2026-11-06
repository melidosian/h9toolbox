# TD-H9 replacement firmware: current state

The main radio uses a Pi32v2 application and a separate ARM APRS controller.
This folder contains source ports for both. The **only replacement main task
booted on the radio so far** is a small, stock-derived display task: it replaces
`my_app` in the official v1.0.38 main application while retaining stock reset,
`app_core`, controller setup, key scanning, and lower LCD transfer code. It is
not yet a standalone replacement firmware or a full radio application.

The detailed, chronological experiments and source inventory are preserved in
[RESEARCH_LOG.md](RESEARCH_LOG.md). The main firmware and APRS research map is in
[../README.md](../README.md).

## Proven hardware result

- [The stage-23 Snake task](main/display_task_entry_stage23.s) booted on one
  TD-H9. The user reported that its display and controls looked and worked
  great. Up, Down, Menu, and Back steer; Back starts a new round to the right.
  Food growth and collision behavior have not been individually described.
- Its first version, stage 22, drew horizontal stripes because the start x/y
  coordinates were reversed when opening an LCD cell window. Stage 23 swaps
  them. The [trial record](RESEARCH_LOG.md#stage-twenty-three-snake-coordinate-correction)
  includes the observed failure and correction.
- The earlier [stage-21 task](main/display_task_entry_stage21.s) established
  fast, four-direction key-driven redraw. Stages 17–20 established the panel
  initialization, pixel path, buffer transfers, and stock key decoder.
- After the stage-23 trial, the pinned HID restore returned all 213 accessible
  main-firmware sectors to the stock CRC map. The user confirmed the normal
  channel screen and controls after power-cycling.

The working stage-23 flash image is generated under ignored
`build/display-task-stage23/h9_hello_display_OFFLINE_EXPERIMENT.fw` (SHA-256
`dac982915910fc49076d1eac8cc01581ca78f5eacb92b44096c2b710805515d0`).
It changes only four 4096-byte sectors: `0x5000`, `0x8D000`, `0xAC000`, and
`0xD9000`. The target main-flash SHA-256 is
`0b62151294309a2e6ce6c406970b9894b6ba14a4cdb996baf9ed9cac6f6d67f9`.

## Rebuild and recover

The stock v1.0.38 application input is pinned to SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The official `.fw` package and extracted `app.bin` are local inputs and are
not committed. From the repo root, rebuild with:

```powershell
py -3 firmware-research/replacement-source/build_display_task_stage23.py --stock-image <stock-app.bin> --stock-package <stock-v1.0.38.fw>
```

The builder checks the stock input, erased code cave, hook, and exact changed
sector set. Rebuilding against the pinned inputs reproduces the package hash
above. The [stage-23 trial script](../h9-firmware-research/trial_display_task_stage23.ps1)
verifies the BR23 updater, reads and hashes a fresh stock backup, checks the
pinned HID restore plan, writes four sectors, and reads back the full main
image in one Administrator run. It expects this workspace's ignored local
inputs. The [HID restore script](../h9-firmware-research/restore_display_task_stage23_over_hid.py)
checks the exact backup and package, scans the accessible sectors, restores
changed sectors, and rescans their CRCs. A backup from the same radio is
required before any write.

The latest stage-23 radio backup is kept under ignored `private/downloads/`.
Generated packages, logs, extracted vendor bytes, and radio backups stay out
of Git; backups can contain private settings. See the [USB flasher](../h9-flasher/README.md)
and [HID recovery notes](../h9-firmware-research/README.md) for protocol details.

## Source work beyond the display task

The `main/`, `aprs/`, and `common/` trees contain host-tested ports of selected
stock functions and replacement policies. The main diagnostic link check is
**not flashable**: board initialization, much RAM code, RF, storage, UI,
Bluetooth, and final packaging are incomplete. The APRS diagnostic link check
also lacks complete board behavior. Build steps, module inventory, and limits
are in [RESEARCH_LOG.md](RESEARCH_LOG.md#implemented-source).

The next display-task work is to verify food growth and collision behavior
separately and tune game timing. The next standalone-main work is to connect
the proven task and display path to authored reset, board initialization,
interrupts, and storage in small, recoverable increments. None of the Snake
hardware results establishes that this standalone main build can boot.
