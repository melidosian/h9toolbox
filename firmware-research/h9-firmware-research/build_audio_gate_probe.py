#!/usr/bin/env python3
"""Build an offline TD-H9 v1.0.38 diagnostic image with low GPIO gate writes.

This is a bench probe, not the requested APRS mute option. When the audio-gate
writer is called, it sends zero to GPIO index 3. The caller can skip that
write if its software state already matches a request, so this patch does not
guarantee a low pin at all times. Physical audio wiring is unverified.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
from pathlib import Path
import sys

import firmware


STOCK_APP_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
PATCH_OFFSET = 0x05222E
EXPECTED = bytes.fromhex("41 16")  # r1 = r4 (requested audio gate state)
REPLACEMENT = bytes.fromhex("41 20")  # r1 = 0 (GPIO index 3 always low)


def build(stock_path: Path) -> tuple[bytes, bytes]:
    stock = stock_path.read_bytes()
    image = firmware.checked_firmware(stock_path).image
    decoded = firmware.decode_app_area(image)
    _, app_entry, _ = firmware.app_layout(decoded)
    original = bytes(decoded[app_entry.data_offset:app_entry.data_offset + app_entry.size])
    if sha256(original).hexdigest() != STOCK_APP_SHA256:
        raise firmware.FormatError("Main app is not the exact analyzed TD-H9 v1.0.38 image")
    if original[PATCH_OFFSET:PATCH_OFFSET + len(EXPECTED)] != EXPECTED:
        raise firmware.FormatError("Audio gate instruction does not match the analyzed bytes")
    replacement = bytearray(original)
    replacement[PATCH_OFFSET:PATCH_OFFSET + len(EXPECTED)] = REPLACEMENT
    if [i for i, (old, new) in enumerate(zip(original, replacement)) if old != new] != [PATCH_OFFSET + 1]:
        raise firmware.FormatError("Probe changed an unexpected main-app byte")
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
        print("Only app+0x05222F changed: 0x16 -> 0x20 (r1=r4 -> r1=0 at GPIO gate call).")
        print("BENCH PROBE ONLY. Gate calls write low; audio behavior and APRS decoding are unverified.")
        return 0
    except (firmware.FormatError, OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
