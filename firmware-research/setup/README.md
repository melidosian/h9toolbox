# Setting up a machine for the firmware research

Everything the builds and radio tools need that is not stored in the repo,
fetched and checked by hash. Run from the repository root.

| Step | Command | Result |
|---|---|---|
| 1. Python tools | `python3 -m venv ~/h9usb && ~/h9usb/bin/pip install -r firmware-research/setup/requirements.txt` | hidapi + pyusb for `h9-flasher/`, `hid-recovery/` and the probes. macOS: also `brew install libusb`. |
| 2. Stock firmware | `python3 -I firmware-research/setup/fetch_stock.py` | `setup/stock/TD-H9-V1.0.38.fw` (`c4d0beb5…`) and `app.bin` (`11a07bc0…`), from the pinned Type-C v1.0.38 ZIP on the TDH9 GitHub mirror (7.1 MB). Offline: `--zip <file>` or `--fw <file>`. |
| 3. JieLi toolchain | `sh firmware-research/toolchain/jieli/setup.sh` | Extracts the bundled archive (`a1545cdd…`, 135 MB extracted). |
| 4. Docker images | `docker build --platform linux/amd64 -t h9-jieli firmware-research/toolchain`<br>`docker build --platform linux/amd64 -t h9-jieli-dev -f firmware-research/toolchain/Dockerfile.dev firmware-research/toolchain` | `h9-jieli` runs the JieLi tools (x86-64); `h9-jieli-dev` adds gcc for the Doom host tests. On Apple Silicon: `colima start --vm-type vz --vz-rosetta`. |
| 5. DMC100-Doom (Doom only) | `sh firmware-research/setup/fetch_dmc100.sh` | `setup/dmc100` at commit `0cdc78eb…` with the TD-H9 pack patch applied (git clone from GitHub). |

The stock firmware is TIDRADIO's and DMC100-Doom is a separate GPL-2.0
project; both stay out of git (`setup/.gitignore`).

After setup, the Doom firmware builds with no path arguments:

```sh
python3 -I firmware-research/doom-firmware/build.py      # uses setup/stock, setup/dmc100, the bundled toolchain
python3 -I firmware-research/doom-firmware/test/host_test.py --pack <DOOM.DAT>
```

The Doom game pack and music for the radio are already in the repo
(`dist/doom/files/`). Regenerating them needs Freedoom 0.13.0
(`freedoom-0.13.0.zip`, BSD, SHA-256 `3f9b264f…`), DMC100's `tools/conv`
and `doom-firmware/music.py`.

The flasher needs root for USB mass storage: run it as
`sudo ~/h9usb/bin/python firmware-research/h9-flasher/h9_flasher.py …`. The
HID tools (PTT+2) do not need root.
