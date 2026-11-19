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

## Start the bridge for Paracon or KISS TCP

Replace `DEVICE_ID` with the identifier you copied. Keep the quotes.

**Windows**

```powershell
.\.venv\Scripts\python.exe bridge.py --device "DEVICE_ID"
```

**macOS and Linux**

```sh
.venv/bin/python bridge.py --device "DEVICE_ID"
```

Keep the bridge terminal open and choose your connection type.

| Program | Connection type | Host | TCP port |
| --- | --- | --- | --- |
| Paracon and other AGWPE clients | AGWPE | `127.0.0.1` | `8000` |
| Packet programs with their own KISS support | KISS TCP | `127.0.0.1` | `8100` |

In Paracon, use radio port **0** (`Port1 TD-H9 BLE` in the port list).
Monitor works in the default receive-only mode. To initiate a connection
over the air, tune the radio to the packet station, configure your licensed
callsign in Paracon, and restart the bridge with `--allow-tx` at the end
of the command. The bridge handles one connected RF session at a time.
Disconnect your KISS TCP program before opening an AGWPE radio connection.

If Bluetooth cannot connect, close App Manager and other Bluetooth radio
tools, then run the scan again. If port 8000 is busy, stop the earlier packet
server before starting the bridge. Use `--agwpe-port 0` for KISS TCP only.

## Start the bridge and Pat together

Install [Pat](https://getpat.io/) and configure your callsign and Winlink
credentials in Pat. The bridge provides AX.25 and AGWPE directly. Stop the
standalone bridge if it is running, then run `pat env` and copy the path
shown for `PAT_CONFIG_PATH`.

Replace the device ID, configuration path, and gateway callsign below.
Include `--allow-tx` if you intend to make an over-the-air connection.

**Windows**

```powershell
.\.venv\Scripts\python.exe run_pat.py --device "DEVICE_ID" --pat-config "PATH\TO\config.json" --gateway GATEWAY-10 --open-browser
```

**macOS and Linux**

```sh
.venv/bin/python run_pat.py --device "DEVICE_ID" --pat-config "PATH/TO/config.json" --gateway GATEWAY-10 --open-browser
```

In [Pat](http://127.0.0.1:8080/), choose **Connect** and then the **H9**
entry for your gateway. The terminal shows AX.25 RX and TX. Press Ctrl+C
to close Pat and the bridge. You can also configure a separately running
Pat to use AGWPE address `127.0.0.1:8000`, radio port `0`, and the
connection URL `ax25+agwpe:///GATEWAY-10`.

A manual Winlink connection can send messages in Pat's Outbox. The launcher
uses your normal mailbox by default. Use `--mbox PATH` for a separate mailbox.
