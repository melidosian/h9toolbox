# H9 Bluetooth KISS bridge

The bridge connects your computer to a TD-H9 over Bluetooth and makes the
radio available to packet software on that computer. Install H9 Toolbox
Firmware, complete **Unlock KISS TNC** on the radio, and confirm **V1.0.15X**
in APRS Info. Turn Bluetooth on, leave the radio on its normal channel screen,
and close any other program currently connected to it.

## Set up the bridge

Install [Python 3.11 or later](https://www.python.org/downloads/). Unzip this
download and open a terminal in the **H9-KISS-Bridge** folder.

On Windows, open the folder in File Explorer, type `powershell` in the
address bar, and press Enter. Run these commands one line at a time.

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe bridge.py --scan
```

On macOS or Linux, open Terminal, type `cd ` with a space after it, drag the
unzipped folder into Terminal, and press Return. Run these commands.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python bridge.py --scan
```

Find the radio in the list and copy the identifier to the left of its name.
It may look like a long string of letters and numbers or a Bluetooth address.
If macOS asks for Bluetooth access for Terminal or Python, allow it and scan
again. On Linux, the computer needs a running BlueZ Bluetooth service.

## Connect packet software

Replace `DEVICE_ID` below with the identifier you copied. Keep the quotes.

**Windows**

```powershell
.\.venv\Scripts\python.exe bridge.py --device "DEVICE_ID"
```

**macOS and Linux**

```sh
.venv/bin/python bridge.py --device "DEVICE_ID"
```

In your packet program, choose **KISS TCP** and enter host **127.0.0.1**,
port **8100**. Keep the bridge terminal open. It starts in receive-only mode.
For licensed transmission, tune the radio, configure your callsign in your
packet software, then stop the bridge with Ctrl+C and restart it with
`--allow-tx` added to the command.

If it cannot connect, disconnect App Manager and other Bluetooth radio
tools, make sure Bluetooth is on at both ends, and rerun the scan.

## Pat and programs that use AGWPE

The ZIP also includes the Pat launcher and the patch for its matching tncd
packet engine. Install [Pat](https://getpat.io/), Git and Go 1.25.13 or later.
Configure your callsign and Winlink credentials in Pat. Stop the standalone
bridge if it is running. Run these commands from the bridge folder.

```text
git init tncd-src
git -C tncd-src fetch --depth 1 https://github.com/ben-kuhn/tncd.git c4de796ebe358234a56d1cf075a017d7d5ad850d
git -C tncd-src checkout --detach FETCH_HEAD
git -C tncd-src apply ../tncd-h9.patch
cd tncd-src
go build -o ../tncd-h9 ./cmd/tncd
cd ..
```

On Windows, use `../tncd-h9.exe` for the build output. Run `pat env` and copy
the path shown for `PAT_CONFIG_PATH`. Replace the device ID, config path,
and gateway callsign in the command for your computer.

**Windows**

```powershell
.\.venv\Scripts\python.exe run_pat.py --device "DEVICE_ID" --pat-config "PATH\TO\config.json" --tncd .\tncd-h9.exe --gateway GATEWAY-10 --open-browser
```

**macOS and Linux**

```sh
.venv/bin/python run_pat.py --device "DEVICE_ID" --pat-config "PATH/TO/config.json" --tncd ./tncd-h9 --gateway GATEWAY-10 --open-browser
```

The launcher starts the bridge, tncd, and [Pat](http://127.0.0.1:8080/). An
AGWPE client such as Paracon can use host **127.0.0.1**, port **8000**, radio
port **0**. Add `--allow-tx` to let the programs transmit when you connect
manually. Pat uses your normal mailbox by default and may send queued
messages. The launcher accepts `--mbox PATH` for a separate mailbox.

The tncd patch is supplied under GPL-3.0. See `COPYING.tncd`.
