#!/usr/bin/env python3
"""Restore the failed TD-H9 internal-logo experiment from a pinned backup.

This writes only the known stock main firmware chip (JEDEC 0x856014), after
pinning an exact full-chip backup and generated target/manifest. The target
caused a black screen on hardware. Flash and resume are permanently disabled.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "h9-flasher"))
from h9_flasher import (ERASE_SIZE, TRANSFER_SIZE, FLASH_ID, FlashError,
                        load_protocol, parse_flash_header, read_flash,
                        require_bootloader, start_loader)  # noqa: E402


BACKUP_HASH = "2c6336b8b2c2a1549946e248a26624ef8f9f2189cf27d4bc1c54d17907af1359"
TARGET_HASH = "62b3af328050c340a3e1257f8966cc76fc48aea0d7f927f868abe49b19b346c7"
LOGO_HASH = "5faf006df5a83f3b50d2a1b981ad89d4f0268fe7be7e2c4eb5cb2549a0782c43"
FLASH_SIZE = 0x100000
SLOT = 0xF0000
SLOT_END = 0xFA000
MAIN_END = 0xDA000


def load_plan(backup_path: Path, target_path: Path, manifest_path: Path):
    backup = backup_path.read_bytes()
    target = target_path.read_bytes()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if len(backup) != FLASH_SIZE or sha256(backup).hexdigest() != BACKUP_HASH:
        raise FlashError("Full-chip backup does not match this radio's pinned backup")
    if len(target) != FLASH_SIZE:
        raise FlashError("Target is not exactly 1 MiB")
    if sha256(target).hexdigest() != TARGET_HASH:
        raise FlashError("Target differs from the offline-validated logo probe")
    if sha256(target[SLOT:SLOT_END]).hexdigest() != LOGO_HASH:
        raise FlashError("Target logo slot differs from the supplied artwork")
    if manifest.get("schema") != 1 or manifest.get("backup_sha256") != BACKUP_HASH:
        raise FlashError("Manifest does not match pinned backup")
    if manifest.get("target_sha256") != sha256(target).hexdigest():
        raise FlashError("Manifest does not match target")
    changed = [offset for offset in range(0, FLASH_SIZE, ERASE_SIZE)
               if backup[offset:offset + ERASE_SIZE] != target[offset:offset + ERASE_SIZE]]
    if manifest.get("changed_sectors") != [f"0x{offset:X}" for offset in changed]:
        raise FlashError("Manifest changed-sector list does not match target")
    if not set(range(SLOT, SLOT_END, ERASE_SIZE)).issubset(changed):
        raise FlashError("Target lacks full logo slot")
    if any(MAIN_END <= offset < SLOT or offset >= SLOT_END for offset in changed):
        raise FlashError("Target changes unexpected VM sectors")
    if parse_flash_header(backup[:MAIN_END]) != parse_flash_header(target[:MAIN_END]):
        raise FlashError("Target changes main flash identity/capacity header")
    return backup, target, changed


def write_sector(loader, offset: int, data: bytes) -> None:
    loader.flash_erase_sector(offset)
    for page in range(offset, offset + ERASE_SIZE, TRANSFER_SIZE):
        loader.flash_write(page, data[page:page + TRANSFER_SIZE])
    readback = b"".join(loader.flash_read(page, TRANSFER_SIZE)
                        for page in range(offset, offset + ERASE_SIZE, TRANSFER_SIZE))
    if readback != data[offset:offset + ERASE_SIZE]:
        raise FlashError(f"Sector verification failed at 0x{offset:X}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inspect", "restore"))
    parser.add_argument("--backup", required=True, type=Path)
    parser.add_argument("--target", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--device")
    parser.add_argument("--yes", action="store_true")
    args = parser.parse_args()
    try:
        backup, target, changed = load_plan(args.backup, args.target, args.manifest)
        print(f"Pinned backup: {sha256(backup).hexdigest()}")
        print(f"Target: {sha256(target).hexdigest()}")
        print(f"Changed sectors ({len(changed)}): {', '.join(f'0x{x:X}' for x in changed)}")
        if args.command == "inspect":
            print("Offline validation complete. No device opened.")
            return 0
        if not args.device or not args.yes:
            raise FlashError("Hardware commands require --device and --yes")
        SCSIDev = load_protocol()[0]
        with SCSIDev(args.device) as device:
            require_bootloader(device)
            loader = start_loader(device, load_protocol())
            if loader.online_device()["id"] != FLASH_ID or loader.chip_key() != 0xF181:
                raise FlashError("Flash ID or chip key changed")
            current = read_flash(loader, FLASH_SIZE)
            if parse_flash_header(current[:MAIN_END]) != parse_flash_header(backup[:MAIN_END]):
                raise FlashError("Radio flash identity changed")
            if args.command == "flash" and current != backup:
                raise FlashError("Radio no longer matches pinned full-chip backup")
            if args.command in ("resume", "restore"):
                for offset in range(0, FLASH_SIZE, ERASE_SIZE):
                    if offset not in changed and current[offset:offset + ERASE_SIZE] != backup[offset:offset + ERASE_SIZE]:
                        raise FlashError(f"Unplanned sector differs at 0x{offset:X}")
            desired = backup if args.command == "restore" else target
            # Program artwork first; leave the bootable original main app until last.
            order = (list(range(SLOT, SLOT_END, ERASE_SIZE)) +
                     [offset for offset in changed if offset < MAIN_END])
            for offset in order:
                if current[offset:offset + ERASE_SIZE] == desired[offset:offset + ERASE_SIZE]:
                    continue
                print(f"Writing sector 0x{offset:X}", flush=True)
                write_sector(loader, offset, desired)
            verified = read_flash(loader, FLASH_SIZE)
            if verified != desired:
                raise FlashError("Full-chip readback mismatch")
            print(f"Full-chip readback verified: {sha256(verified).hexdigest()}")
            print("Restore complete" if args.command == "restore" else "Logo probe flash complete")
            return 0
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
