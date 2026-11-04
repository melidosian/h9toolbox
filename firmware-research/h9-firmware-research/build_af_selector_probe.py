#!/usr/bin/env python3
"""Build an offline TD-H9 v1.0.38 RF audio-selector mute probe.

This globally forces mode zero for radio register 0x47. It is a bench probe,
not an APRS mute option. It may silence Bluetooth as well as speaker audio,
but it may also deprive the separate APRS decoder of analog audio. Do not
flash it until stock firmware has a confirmed decoded-packet baseline.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
from pathlib import Path
import sys

import firmware


STOCK_APP_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
PATCH_OFFSET = 0x0592E4
EXPECTED = bytes.fromhex("00 a8")  # r0 = r0 << 8 (RF AF mode)
REPLACEMENT = bytes.fromhex("40 20")  # r0 = 0 (RF AF mute mode)


def build(stock_path: Path) -> tuple[bytes, bytes]:
    stock = stock_path.read_bytes()
    image = firmware.checked_firmware(stock_path).image
    decoded = firmware.decode_app_area(image)
    _, app_entry, _ = firmware.app_layout(decoded)
    original = bytes(decoded[app_entry.data_offset:app_entry.data_offset + app_entry.size])
    if sha256(original).hexdigest() != STOCK_APP_SHA256:
        raise firmware.FormatError("Main app is not the exact analyzed TD-H9 v1.0.38 image")
    if original[PATCH_OFFSET:PATCH_OFFSET + len(EXPECTED)] != EXPECTED:
        raise firmware.FormatError("RF AF selector instruction does not match the analyzed bytes")
    replacement = bytearray(original)
    replacement[PATCH_OFFSET:PATCH_OFFSET + len(EXPECTED)] = REPLACEMENT
    changed = [i for i, (old, new) in enumerate(zip(original, replacement)) if old != new]
    if changed != [PATCH_OFFSET, PATCH_OFFSET + 1]:
        raise firmware.FormatError("Probe changed unexpected main-app bytes")
    package = firmware.rebuild(stock, replacement)
    checked = firmware.parse_firmware_bytes(package)
    decoded_check = firmware.decode_app_area(checked.image)
    _, checked_app, _ = firmware.app_layout(decoded_check)
    actual = decoded_check[checked_app.data_offset:checked_app.data_offset + checked_app.size]
    if actual != replacement:
        raise firmware.FormatError("Probe app failed the package round trip")
    return package, bytes(replacement)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise firmware.FormatError(f"Refusing to overwrite {args.output}")
        package, replacement = build(args.stock)
        with args.output.open("xb") as output:
            output.write(package)
        firmware.validate_repacked(args.stock, args.output)
        print(f"Diagnostic package: {args.output}")
        print(f"SHA-256 {sha256(package).hexdigest()}; app SHA-256 {sha256(replacement).hexdigest()}")
        print("Only app+0x0592E4..0x0592E5 changed: r0<<8 -> r0=0 at RF AF selector.")
        print("BENCH PROBE ONLY. It may silence the APRS decoder input; do not flash before a stock decode baseline.")
        return 0
    except (firmware.FormatError, OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
