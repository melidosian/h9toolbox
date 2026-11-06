#!/usr/bin/env python3
"""Pinned USB HID fallback for the two-sector H9 main boot probe.

`plan` is offline. `inspect` reads CRCs. `restore --yes` writes only the
first app and final metadata sectors from a fresh stock main-flash backup.
This path is prepared but has not been exercised for this exact probe.
"""

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
STOCK_FLASH_SHA256 = "74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648"
PACKAGE_SHA256 = "d669b900f1814b218199739bd0f801d6f22977de8e441f538a952379e4767f8d"
TARGET_FLASH_SHA256 = "48a370878081d8fff07200b87a9f3e08de211016290a58d4928477ac99f0ec60"
CHANGED_SECTORS = (0x5000, 0xD9000)


def load_plan(backup_path: Path, package_path: Path) -> tuple[bytes, bytes]:
    backup = backup_path.read_bytes()
    if len(backup) != MAIN_END or sha256(backup).hexdigest() != STOCK_FLASH_SHA256:
        raise ValueError("Backup is not the pinned stock v1.0.38 main flash image")
    if sha256(package_path.read_bytes()).hexdigest() != PACKAGE_SHA256:
        raise ValueError("Package differs from the exact two-sector boot probe")
    target = parse_firmware(package_path).image
    if len(target) != MAIN_END or sha256(target).hexdigest() != TARGET_FLASH_SHA256:
        raise ValueError("Embedded flash image differs from the pinned boot probe")
    changed = tuple(offset for offset in range(0, MAIN_END, BLOCK_SIZE)
                    if backup[offset:offset + BLOCK_SIZE] !=
                    target[offset:offset + BLOCK_SIZE])
    if changed != CHANGED_SECTORS or backup[:APP_START] != target[:APP_START]:
        raise ValueError("Boot probe changes unexpected flash sectors")
    return backup, target


def crc_of(data: bytes, sector: int) -> int:
    return binascii.crc_hqx(data[sector:sector + BLOCK_SIZE], 0)


def inspect_live(device, backup: bytes, target: bytes) -> dict[int, int]:
    actual = {}
    unexpected = []
    for sector in range(APP_START, MAIN_END, BLOCK_SIZE * 16):
        count = min(16, (MAIN_END - sector) // BLOCK_SIZE)
        for index, value in enumerate(fetch_crcs(device, sector - APP_START, count)):
            physical = sector + index * BLOCK_SIZE
            actual[physical] = value
            if physical not in CHANGED_SECTORS and value != crc_of(backup, physical):
                unexpected.append(physical)
    if unexpected:
        raise RuntimeError("Unexpected changes outside boot-probe sectors: " +
                           ", ".join(f"0x{x:X}" for x in unexpected))
    for sector in CHANGED_SECTORS:
        value = actual[sector]
        state = ("stock" if value == crc_of(backup, sector) else
                 "probe" if value == crc_of(target, sector) else "partial/unknown")
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
    print(f"Pinned two-sector recovery plan: {', '.join(hex(x) for x in CHANGED_SECTORS)}")
    if args.command == "plan":
        print("Offline validation passed; no device opened")
        return 0
    device = open_radio()
    try:
        before = inspect_live(device, backup, target)
        if args.command == "inspect":
            return 0
        # Restore metadata first and app entry/header last.
        for sector in reversed(CHANGED_SECTORS):
            if before[sector] != crc_of(backup, sector):
                restore_sector(device, sector, backup)
        after = inspect_live(device, backup, target)
        if any(after[sector] != crc_of(backup, sector) for sector in CHANGED_SECTORS):
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
