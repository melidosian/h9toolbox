#!/usr/bin/env python3
"""Rebuild the verified TD-H9 KISS main firmware from pinned packages."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
RESEARCH = HERE.parent
sys.path.insert(0, str(RESEARCH / "brick-breaker/tools/h9-firmware-research"))
from firmware import app_layout, decode_app_area, parse_firmware, rebuild  # noqa: E402

DEFAULT_BASE = RESEARCH.parent / "dist/firmware-update/h9-toolbox-firmware-v1.0.38.fw"


def check(data, digest, label):
    if sha256(data).hexdigest() != digest:
        raise ValueError(f"{label} SHA-256 does not match the release manifest")


def build(stock_path, base_path):
    manifest = json.loads((HERE / "main_kiss_patch.json").read_text(encoding="utf-8"))
    stock = stock_path.read_bytes()
    check(stock, manifest["stock_package_sha256"], "Factory main firmware")
    check(base_path.read_bytes(), manifest["base_package_sha256"], "H9 Toolbox main firmware")
    decoded = decode_app_area(parse_firmware(base_path).image)
    _, app, _ = app_layout(decoded)
    patched = bytearray(decoded[app.data_offset:app.data_offset + app.size])
    check(patched, manifest["base_app_sha256"], "H9 Toolbox app")
    previous_end = 0
    for patch in manifest["patches"]:
        offset = int(patch["offset"], 16)
        before = bytes.fromhex(patch["before"])
        after = bytes.fromhex(patch["after"])
        end = offset + len(before)
        if len(before) != len(after) or not previous_end <= offset < end <= len(patched):
            raise ValueError(f"Invalid patch span at app+0x{offset:X}")
        if patched[offset:end] != before:
            raise ValueError(f"Unexpected original bytes at app+0x{offset:X}")
        patched[offset:end] = after
        previous_end = end
    check(patched, manifest["app_sha256"], "KISS app")
    output = rebuild(stock, bytes(patched))
    check(output, manifest["package_sha256"], "KISS main firmware")
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock", type=Path, help="Factory TD-H9 Type-C v1.0.38 .fw")
    parser.add_argument("output", type=Path, help="Path for a new KISS .fw")
    parser.add_argument("--base", type=Path, default=DEFAULT_BASE, help="Pinned H9 Toolbox .fw")
    args = parser.parse_args()
    try:
        output = build(args.stock, args.base)
        with args.output.open("xb") as destination:
            destination.write(output)
    except (OSError, RuntimeError, ValueError) as exc:
        parser.exit(1, f"{exc}\n")
    print(f"{sha256(output).hexdigest()}  {args.output}")


if __name__ == "__main__":
    main()
