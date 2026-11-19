# TD-H9 KISS TNC for Pat and packet terminals

This setup carried a Winlink upload over 1200-baud AX.25 through K9ESV-10. It pairs the TD-H9 Type-C main firmware with the separate APRS 1.0.15 modem and a local Bluetooth bridge. The same Python host setup targets macOS, Windows, and Linux.

## Radio firmware

Install the main and APRS images separately. Keep the factory images and a radio backup for recovery. Use only these versions for this setup.

| Component | Image | What changed |
| --- | --- | --- |
| Main CPU, Type-C v1.0.38 | [TD-H9-v1.0.38-KISS.fw](TD-H9-v1.0.38-KISS.fw) · [SHA-256 and patch offsets](main_kiss_patch.json) | Accepts port-zero KISS writes on AF01 and forwards raw AX.25 on AF02, including 15-byte link control frames. The active channel is used for packet receive. |
| Separate APRS modem v1.0.15 | [APRS-1.0.15-rx15-rxff.bin](APRS-1.0.15-rx15-rxff.bin) · [release manifest](APRS-1.0.15-rx15-rxff.release.json) | Accepts 15-byte AX.25 bodies with a valid FCS and preserves `0xFF` as data. The stock modem could mistake that byte for an empty queue. |

### Main CPU

Use the [main USB updater](../h9-flasher/README.md#custom-main-image-offline-validation-and-recovery) with the factory Type-C v1.0.38 package as `--stock-base`. This tool uses the BR23 USB mass-storage loader, entered by holding **8** at power-on. Follow the guide for your operating system to identify the radio, create a backup, and verify the write. The separate HID recovery mode remains available with **PTT+2**. [HID recovery guide](../hid-recovery/README.md)

To verify or reproduce the distributed main image from the repo root:

```text
python firmware-research/setup/fetch_stock.py
python firmware-research/h9-flasher/h9_flasher.py inspect firmware-research/kiss-tnc/TD-H9-v1.0.38-KISS.fw --stock-base firmware-research/setup/stock/TD-H9-V1.0.38.fw
python firmware-research/kiss-tnc/build_main_kiss.py firmware-research/setup/stock/TD-H9-V1.0.38.fw rebuilt-kiss.fw
```

The builder applies the exact tested changes to the repo’s pinned H9 Toolbox main app. It checks the starting package and app, verifies every patch span and the resulting app hash, and rebuilds the official factory package with the patched app. It reproduces the shipped file byte for byte.

### APRS modem

The [experimental dev-site updater](https://dev.h9toolbox.com/aprs-update/) uses Web Serial in desktop Chrome or Edge. It supports the pinned packet fix and stock recovery image. A macOS Chrome hardware test left APRS Info on **Querying…** after reboot. The page offers recovery downloads on macOS and disables writing there. Use the Windows procedure below for the hardware-verified route.

Use Windows for the hardware-verified update path. Before entering update mode, have the modified image, its release manifest, and the stock APRS 1.0.15 recovery image ready. The [stock image reference](../h9-firmware-research/README.md#aprs-and-pi32-work) gives its source and SHA-256. Both images are 17,304 bytes. The APRS modem firmware requires its own stock recovery image.

Run this preflight in PowerShell from the repo root. Set `$stock` to your copy of the verified stock image. The modified and stock hashes should match `image_sha256` and `base_image_sha256`, respectively, in the release manifest. The `plan` command runs without opening a serial port.

```powershell
py -3 -m pip install pyserial
$flasher = "firmware-research/h9-firmware-research/aprs_flasher.py"
$image = "firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.bin"
$manifest = "firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.release.json"
$stock = "C:\path\to\stock\APRS-1.0.15.bin"
Get-FileHash -Algorithm SHA256 -LiteralPath $image,$stock
py -3 $flasher plan $image
```

With USB connected, power the radio off, then hold **PTT+7** while powering on. The screen should show **APRS Update**. List ports and substitute the radio’s actual COM port for `COM4`.

```powershell
py -3 $flasher ports
py -3 $flasher flash $image --port COM4 --baud 38400 --allow-modified `
  --release-manifest $manifest --recovery-image $stock --yes
```

The updater sets DTR and RTS before opening the port and waits for **20 ms of serial silence** before the first block. A completed transfer has 136 block acknowledgments and an exit acknowledgment. The updater saves them to a JSONL journal. After a normal power cycle, check that **APRS Info** shows **V1.0.15**, then verify an over-the-air packet connection and a received data frame. Earlier Mac runs returned transfer acknowledgments and still showed **Querying…** after reboot.

If the write fails or APRS Info stays on **Querying…**, re-enter **APRS Update** on Windows and restore the stock image from the beginning.

```powershell
py -3 $flasher flash $stock --port COM4 --baud 38400 --yes
```

The modem patch can also be rebuilt with:

```text
python firmware-research/kiss-tnc/build_aprs_rx.py /path/to/stock/APRS-1.0.15.bin regenerated-aprs.bin
```

## Connect a computer

Follow the [bridge README](host/README.md) for setup on macOS, Windows or Linux.
The bridge exposes AGWPE at `127.0.0.1:8000` for Pat and Paracon, and keeps
standard port-zero KISS TCP at `127.0.0.1:8100`. The Pat launcher uses the
bridge's built-in AX.25 engine and the same local Pat mailbox.

The AX.25 engine retains the H9 profile used by the earlier host setup.
It uses v2.0 sequencing, a one-frame window, up to 80 information bytes
per frame, a 100 ms initial write batch, three retries and an 18-second
idle poll. The bridge keeps the 1.60-second interval between BLE writes
and 0.70-second receive-to-transmit guard. It fits encoded data into a single
Bluetooth write, including when a digipeater path or escaping uses more bytes.

The earlier host configuration completed a Winlink upload on macOS.
The Pat and Paracon client libraries exchanged data through the integrated
engine against a simulated radio. The integrated engine still needs an
over-the-air check.
See the [historical tncd patch](host/tncd-h9.patch) for the previous setup.
