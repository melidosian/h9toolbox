# TD-H9 KISS TNC for Pat

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

The hardware-verified update path was Windows. Install `pyserial`, power off, then hold **PTT+7** while powering on with USB connected. Confirm **APRS Update** on the radio. Use `aprs_flasher.py ports` to identify its COM port. Run this example in PowerShell from the repo root with your own port and stock recovery image path.

```powershell
py -3 -m pip install pyserial
py -3 firmware-research/h9-firmware-research/aprs_flasher.py flash `
  firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.bin `
  --port COM4 --baud 38400 --allow-modified `
  --release-manifest firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.release.json `
  --recovery-image "C:\path\to\stock\APRS-1.0.15.bin" --yes
```

The updater waits for 20 ms of serial silence before the first block. Power-cycle normally and check that **APRS Info** shows **V1.0.15**. If it stays at **Querying…**, restore the stock APRS image in the same mode with the stock file as the target, omitting the modified-image options. The modem patch can also be rebuilt with:

```text
python firmware-research/kiss-tnc/build_aprs_rx.py /path/to/stock/APRS-1.0.15.bin regenerated-aprs.bin
```

## Install the host tools

Use Python 3.11 or later, Git, Go 1.25.13 or later, and [Pat](https://getpat.io/) configured with your callsign and Winlink login. Run the following from this checkout’s `firmware-research/kiss-tnc/host` directory. Use `python3` or `py -3` if that is how you invoke Python.

```text
python -m pip install -r requirements.txt
git init tncd-src
git -C tncd-src fetch --depth 1 https://github.com/ben-kuhn/tncd.git c4de796ebe358234a56d1cf075a017d7d5ad850d
git -C tncd-src checkout --detach FETCH_HEAD
git -C tncd-src apply ../tncd-h9.patch
cd tncd-src
go build -o ../tncd-h9 ./cmd/tncd
cd ..
```

On Windows, change the build output to `../tncd-h9.exe`. The [tncd patch](host/tncd-h9.patch) applies to the pinned upstream revision and is supplied under [GPL-3.0](host/COPYING.tncd).

## Connect Pat to the radio

1. Select the gateway frequency on the H9, turn Bluetooth on, and leave the radio in normal mode. The tested setup used simplex with Dual Watch off.
2. Run `python bridge.py --scan` in the host directory. Select the H9’s identifier from that machine. macOS typically shows a UUID and Windows or Linux typically shows a Bluetooth address. Linux uses BlueZ and requires access to the local Bluetooth adapter. [Bleak platform support](https://bleak.readthedocs.io/en/latest/)
3. Run `pat env` to find `PAT_CONFIG_PATH`. Start the launcher with your device identifier, configuration path, and gateway. Specify `./tncd-h9.exe` on Windows.

```text
python run_pat.py --device DEVICE_ID --pat-config "PATH/TO/config.json" --tncd ./tncd-h9 --gateway K9ESV-10 --allow-tx
```

4. Open [Pat](http://127.0.0.1:8080/) and choose **Connect → H9 K9ESV-10**, or the alias for your chosen gateway. The terminal shows live AX.25 RX and TX. Press Ctrl+C to stop the three local processes.

The launcher keeps the Pat configuration in a temporary copy and uses the normal Pat mailbox. Use `--mbox PATH` to select a separate mailbox. A manual Connect can transmit everything queued in that mailbox. Omit `--allow-tx` for passive packet monitoring. The launcher disables automatic connections and binds its listeners to loopback.

### Settings from the successful upload

| Setting | Value |
| --- | --- |
| AX.25 | v2.0, 1200 baud, one outstanding data frame |
| Maximum information field | 80 bytes |
| Batch delay for adjacent Pat writes | 100 ms |
| Minimum interval between BLE writes | 1.60 s |
| Quiet time after a BLE receive notification | 0.70 s |
| AX.25 retry limit and idle poll | 3 retries, 18 s idle |
| H9 Bluetooth | AF00 service, AF01 write without response, AF02 notifications |

The batch window joins Pat’s short transfer header with the start of its binary body. The tncd patch also fixes retry accounting after a missed connection acknowledgment, duplicate packets on rejects, and disconnect retries. Each complete escaped KISS frame goes into one Bluetooth write. If a platform reports a smaller write capacity, the bridge reports the limit. The 80-byte profile was tested on a direct link. Longer digipeater paths can require a smaller information field.

The main image and APRS modem were verified on the radio, and the equivalent host configuration completed a real Pat send on macOS. The published firmware builds and host regression tests can be verified offline. The tncd package builds on macOS, Windows, and Linux. Bluetooth operation on Windows and Linux still needs hardware validation.
