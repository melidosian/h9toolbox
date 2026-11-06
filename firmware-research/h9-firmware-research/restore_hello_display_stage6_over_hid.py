#!/usr/bin/env python3
"""Pinned HID fallback for the H9 HELLO WORLD stage-six transfer image."""

from __future__ import annotations

import argparse
import binascii
from hashlib import sha256
from pathlib import Path
import sys

from probe_hid_recovery import APP_START, BLOCK_SIZE, fetch_crcs
from restore_logo_over_hid import open_radio, restore_sector

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "h9-flasher"))
from h9_flasher import parse_firmware  # noqa: E402


MAIN_END = 0xDA000
STOCK_SHA256 = "74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648"
PACKAGE_SHA256 = "1d6fca78735058df08ff7e3e9a3844c33b0eb04820bce606d8d01e000f119785"
TARGET_SHA256 = "e2bb2f954d01dbac8434a6476543d84be107ad2af31a462ce292ba02848642a7"
CHANGED = (0x5000, 0x8D000, 0xAC000, 0xD9000)


def crc(data: bytes, sector: int) -> int:
    return binascii.crc_hqx(data[sector:sector + BLOCK_SIZE], 0)


def load_plan(backup_path: Path, package_path: Path) -> tuple[bytes, bytes]:
    backup = backup_path.read_bytes()
    if len(backup) != MAIN_END or sha256(backup).hexdigest() != STOCK_SHA256:
        raise ValueError("Backup is not the pinned stock v1.0.38 main flash image")
    package = package_path.read_bytes()
    if sha256(package).hexdigest() != PACKAGE_SHA256:
        raise ValueError("Package differs from the exact stage-six HELLO WORLD experiment")
    target = parse_firmware(package_path).image
    if len(target) != MAIN_END or sha256(target).hexdigest() != TARGET_SHA256:
        raise ValueError("Embedded flash image differs from the pinned display image")
    changed = tuple(sector for sector in range(0, MAIN_END, BLOCK_SIZE)
                    if backup[sector:sector + BLOCK_SIZE] !=
                    target[sector:sector + BLOCK_SIZE])
    if changed != CHANGED or backup[:APP_START] != target[:APP_START]:
        raise ValueError("Display image changes unexpected flash sectors")
    return backup, target


def scan(device, backup: bytes, target: bytes) -> dict[int, int]:
    actual = {}
    unexpected = []
    for sector in range(APP_START, MAIN_END, BLOCK_SIZE * 16):
        count = min(16, (MAIN_END - sector) // BLOCK_SIZE)
        for index, value in enumerate(fetch_crcs(device, sector - APP_START, count)):
            physical = sector + index * BLOCK_SIZE
            actual[physical] = value
            if physical not in CHANGED and value != crc(backup, physical):
                unexpected.append(physical)
    if unexpected:
        raise RuntimeError("Unexpected changes outside display image: " +
                           ", ".join(hex(x) for x in unexpected))
    for sector in CHANGED:
        value = actual[sector]
        state = ("stock" if value == crc(backup, sector) else
                 "display" if value == crc(target, sector) else "partial/unknown")
        print(f"Sector 0x{sector:X}: {state} (CRC 0x{value:04X})")
    print(f"Verified {len(actual)} accessible main-firmware sectors by CRC")
    return actual


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("plan", "inspect", "restore"))
    parser.add_argument("--backup", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--yes", action="store_true")
    args = parser.parse_args()
    if args.command == "restore" and not args.yes:
        parser.error("restore requires --yes")
    backup, target = load_plan(args.backup, args.package)
    print("Pinned four-sector recovery plan: " + ", ".join(hex(x) for x in CHANGED))
    if args.command == "plan":
        print("Offline validation passed; no device opened")
        return 0
    device = open_radio()
    try:
        before = scan(device, backup, target)
        if args.command == "inspect":
            return 0
        # Restore app metadata first and the first application sector last.
        for sector in reversed(CHANGED):
            if before[sector] != crc(backup, sector):
                restore_sector(device, sector, backup)
        after = scan(device, backup, target)
        if any(after[sector] != crc(backup, sector) for sector in CHANGED):
            raise RuntimeError("Post-restore CRCs differ from stock")
        print("Stock main-firmware CRC map restored; power-cycle the radio")
        return 0
    finally:
        device.close()


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"HID fallback error: {error}", file=sys.stderr)
        raise SystemExit(1)
