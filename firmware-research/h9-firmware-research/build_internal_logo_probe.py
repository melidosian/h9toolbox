#!/usr/bin/env python3
"""Build an experimental TD-H9 main-flash logo redirect and full-flash target.

The first bank-5 image (gImage_logo) is redirected to a presently erased 40 KiB
slot at 0xF0000 in this radio's 1 MiB main flash. Later bank-5 image addresses
are preserved algebraically. The slot is inside the VM reservation, so this is
a reversible bench probe, not a durable firmware release.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import struct
import sys

import firmware


STOCK_APP_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
RADIO_BACKUP_SHA256 = "2c6336b8b2c2a1549946e248a26624ef8f9f2189cf27d4bc1c54d17907af1359"
STOCK_FLASH_SHA256 = "74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648"
APP_LOGO_SIZE_OFFSET = 0xAA1D4
APP_BANK5_BASE_OFFSET = 0xB5358
APP_BANK5_SIZE_OFFSET = 0xB535C
ORIGINAL_LOGO_SIZE = 0xA000
ORIGINAL_BANK5_BASE = 0xA00000
ORIGINAL_BANK5_SIZE = 0x300000
SLOT = 0xF0000
FLASH_SIZE = 0x100000
PACKAGE_FLASH_SIZE = 0xDA000


def build(stock_path: Path, backup_path: Path, logo_path: Path) -> tuple[bytes, bytes, dict]:
    stock = stock_path.read_bytes()
    checked = firmware.checked_firmware(stock_path)
    backup = backup_path.read_bytes()
    logo = logo_path.read_bytes()
    if len(backup) != FLASH_SIZE or sha256(backup).hexdigest() != RADIO_BACKUP_SHA256:
        raise firmware.FormatError("Full radio backup is not the exact analyzed 1 MiB image")
    if sha256(backup[:PACKAGE_FLASH_SIZE]).hexdigest() != STOCK_FLASH_SHA256:
        raise firmware.FormatError("Main firmware portion of backup is not stock v1.0.38")
    if len(logo) != ORIGINAL_LOGO_SIZE:
        raise firmware.FormatError("Logo must be exactly 40,960 RGB565 bytes")
    if backup[SLOT:SLOT + len(logo)] != b"\xff" * len(logo):
        raise firmware.FormatError("Main-flash logo slot is not entirely erased")
    decoded = firmware.decode_app_area(checked.image)
    _, app_entry, _ = firmware.app_layout(decoded)
    app = bytearray(decoded[app_entry.data_offset:app_entry.data_offset + app_entry.size])
    if sha256(app).hexdigest() != STOCK_APP_SHA256:
        raise firmware.FormatError("App is not exact stock v1.0.38")
    before = [struct.unpack_from("<I", app, offset)[0] for offset in
              (APP_LOGO_SIZE_OFFSET, APP_BANK5_BASE_OFFSET, APP_BANK5_SIZE_OFFSET)]
    if before != [ORIGINAL_LOGO_SIZE, ORIGINAL_BANK5_BASE, ORIGINAL_BANK5_SIZE]:
        raise firmware.FormatError(f"Unexpected logo/resource table: {before!r}")
    delta = ORIGINAL_BANK5_BASE - SLOT
    struct.pack_into("<I", app, APP_LOGO_SIZE_OFFSET, ORIGINAL_LOGO_SIZE + delta)
    struct.pack_into("<I", app, APP_BANK5_BASE_OFFSET, SLOT)
    struct.pack_into("<I", app, APP_BANK5_SIZE_OFFSET, ORIGINAL_BANK5_SIZE + delta)
    if SLOT + ORIGINAL_LOGO_SIZE != ORIGINAL_BANK5_BASE + ORIGINAL_LOGO_SIZE - delta:
        raise AssertionError("Logo offset arithmetic failed")
    # The first image moves to SLOT. Each subsequent image's cumulative offset
    # gains delta, exactly cancelling the lowered bank base.
    if SLOT + ORIGINAL_LOGO_SIZE + delta != ORIGINAL_BANK5_BASE + ORIGINAL_LOGO_SIZE:
        raise AssertionError("Later image addresses changed")
    package = firmware.rebuild(stock, bytes(app))
    parsed = firmware.parse_firmware_bytes(package)
    if len(parsed.image) != PACKAGE_FLASH_SIZE:
        raise firmware.FormatError("Rebuilt package flash length changed")
    target = bytearray(backup)
    target[:PACKAGE_FLASH_SIZE] = parsed.image
    target[SLOT:SLOT + len(logo)] = logo
    changed = [offset for offset in range(0, FLASH_SIZE, 0x1000)
               if backup[offset:offset + 0x1000] != target[offset:offset + 0x1000]]
    expected = set(range(SLOT, SLOT + ORIGINAL_LOGO_SIZE, 0x1000))
    if not expected.issubset(changed) or any(offset >= PACKAGE_FLASH_SIZE and offset not in expected
                                          for offset in changed):
        raise firmware.FormatError("Unexpected changes in main-flash VM/tail")
    manifest = {
        "schema": 1,
        "purpose": "experimental main-flash logo redirect; VM slot may be reclaimed by firmware",
        "backup_sha256": sha256(backup).hexdigest(),
        "target_sha256": sha256(target).hexdigest(),
        "package_sha256": sha256(package).hexdigest(),
        "logo_sha256": sha256(logo).hexdigest(),
        "logo_slot": f"0x{SLOT:X}",
        "logo_length": len(logo),
        "changed_sectors": [f"0x{offset:X}" for offset in changed],
    }
    return package, bytes(target), manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock", type=Path)
    parser.add_argument("--backup", required=True, type=Path)
    parser.add_argument("--logo", required=True, type=Path)
    parser.add_argument("--output-prefix", required=True, type=Path)
    args = parser.parse_args()
    try:
        paths = (args.output_prefix.with_suffix(".fw"),
                 args.output_prefix.with_suffix(".full.bin"),
                 args.output_prefix.with_suffix(".json"))
        for path in paths:
            if path.exists():
                raise firmware.FormatError(f"Refusing to overwrite {path}")
        package, target, manifest = build(args.stock, args.backup, args.logo)
        paths[0].parent.mkdir(parents=True, exist_ok=True)
        paths[0].write_bytes(package)
        paths[1].write_bytes(target)
        paths[2].write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        firmware.validate_repacked(args.stock, paths[0])
        print(json.dumps(manifest, indent=2))
        print("Offline build only; nothing was flashed.")
        return 0
    except (firmware.FormatError, OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
