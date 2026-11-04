#!/usr/bin/env python3
"""Probe the TD-H9 serial recovery handshake without erasing or writing flash.

This sends only the first `app_dir_head` request observed in the vendor
updata.exe log with ENCRYPT NUMBER 12345678. It never sends an image.
"""

from __future__ import annotations

import argparse
import sys
import time


# Vendor updata.exe's first request, captured from the stock-image attempt.
# The command name printed by that updater is `app_dir_head`.
APP_DIR_HEAD = bytes.fromhex(
    "aa5513008e61dd703e3ed8693c3ed4652f05bc004e61bcb538"
)
ADAPTERS = {
    "ftdi": (0x0403, 0x6001),
    "usb-c": (0x4C4A, 0x4155),
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Serial port, e.g. COM6")
    parser.add_argument("--transport", choices=ADAPTERS, default="ftdi")
    parser.add_argument("--seconds", type=float, default=5.0)
    args = parser.parse_args()
    if not 0.5 <= args.seconds <= 15.0:
        parser.error("--seconds must be between 0.5 and 15")

    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.error("pyserial is required: py -3 -m pip install --user pyserial")

    matches = [p for p in list_ports.comports() if p.device.upper() == args.port.upper()]
    if len(matches) != 1:
        parser.error(f"Expected one connected {args.port} device, found {len(matches)}")
    found = matches[0]
    actual_id = (found.vid, found.pid)
    expected_id = ADAPTERS[args.transport]
    if actual_id != expected_id:
        parser.error(
            f"Refusing {args.port}: USB ID {actual_id!r} is not the selected "
            f"{args.transport} interface ({expected_id[0]:04X}:{expected_id[1]:04X})"
        )

    print(f"Adapter: {found.device} ({found.vid:04X}:{found.pid:04X})", flush=True)
    print(f"Baud: 1000000, 8N1; DTR and RTS off", flush=True)
    print(f"Sending one {len(APP_DIR_HEAD)}-byte app_dir_head request: "
          f"{APP_DIR_HEAD.hex(' ')}", flush=True)

    port = serial.Serial(port=None, baudrate=1_000_000, bytesize=8,
                         parity="N", stopbits=1, timeout=0.1, write_timeout=1)
    port.dtr = False
    port.rts = False
    port.port = found.device
    try:
        port.open()
        # Capture bytes sent spontaneously by the radio before the request.
        before = port.read_all()
        if before:
            print(f"Before request: {before.hex(' ')}", flush=True)
        written = port.write(APP_DIR_HEAD)
        port.flush()
        if written != len(APP_DIR_HEAD):
            raise RuntimeError(f"Only {written}/{len(APP_DIR_HEAD)} bytes sent")
        reply = bytearray()
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            chunk = port.read(256)
            if chunk:
                reply.extend(chunk)
        print(f"Reply ({len(reply)} bytes): {reply.hex(' ') if reply else '<none>'}")
        print("Stopped after the identification request. No erase or image write sent.")
        return 0 if reply else 2
    finally:
        port.close()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"Probe error: {exc}", file=sys.stderr)
        sys.exit(1)
