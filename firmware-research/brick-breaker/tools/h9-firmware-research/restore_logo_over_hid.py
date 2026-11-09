#!/usr/bin/env python3
"""Restore the failed TD-H9 logo probe through its confirmed USB HID updater.

Only the exact pre-probe 1 MiB backup is accepted. The radio is identified by
its complete, live 251-sector CRC map before writing. Changed sectors are
restored with per-sector erase/write/CRC verification; the first app sector is
written last. Run `inspect` before `restore` and keep the radio in PTT+2 mode.
"""

from __future__ import annotations

import argparse
import binascii
from pathlib import Path
import platform
import struct
import sys

import hid

from flash_internal_logo_probe import load_plan
from probe_hid_recovery import (APP_START, BLOCK_SIZE, FLASH_SIZE,
                                REPORT_DESCRIPTORS, VID, PID,
                                exchange, fetch_crcs, init_packet, make_packet)


BLANK_CRC = binascii.crc_hqx(b"\xff" * BLOCK_SIZE, 0)
WRITE_CHUNK = 40  # below vendor HID host's maximum payload


def open_radio():
    matches = [device for device in hid.enumerate(VID, PID)
               if device.get("usage_page") == 0xFF00
               and device.get("usage") == 1]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one JieLi vendor HID interface, got {len(matches)}")
    device = hid.device()
    device.open_path(matches[0]["path"])
    try:
        # hidapi exposes this on Windows and some Linux backends, but not on
        # every macOS build. The exact VID/PID, usage, geometry and CRC map
        # are checked independently before any sector write.
        descriptor = getattr(device, "get_report_descriptor", None)
        if descriptor is not None:
            try:
                actual_descriptor = bytes(descriptor())
            except (OSError, NotImplementedError):
                if platform.system() != "Darwin":
                    raise
            else:
                if actual_descriptor not in REPORT_DESCRIPTORS:
                    raise RuntimeError("Unexpected HID report descriptor")
        body = exchange(device, init_packet(), 0xC0, show_raw=False)
        if len(body) != 16:
            raise RuntimeError("Unexpected app_dir_head response")
        geometry = struct.unpack("<IIII", body)
        if geometry[0] != APP_START or geometry[2:] != (0, BLOCK_SIZE):
            raise RuntimeError(f"Unexpected app_dir_head geometry: {geometry!r}")
        print(f"Radio HID updater: {VID:04X}:{PID:04X}; app start 0x{APP_START:X}; "
              f"block size {BLOCK_SIZE}", flush=True)
        return device
    except Exception:
        device.close()
        raise


def scan(device, reference: bytes) -> dict[int, tuple[int, int]]:
    mismatches = {}
    checked = 0
    for relative in range(0, FLASH_SIZE - APP_START, BLOCK_SIZE * 16):
        count = min(16, (FLASH_SIZE - APP_START - relative) // BLOCK_SIZE)
        for index, actual in enumerate(fetch_crcs(device, relative, count)):
            physical = APP_START + relative + index * BLOCK_SIZE
            expected = binascii.crc_hqx(reference[physical:physical + BLOCK_SIZE], 0)
            if actual != expected:
                mismatches[physical] = (actual, expected)
            checked += 1
    print(f"Scanned {checked} accessible sectors; "
          f"{len(mismatches)} CRC mismatch(es)", flush=True)
    return mismatches


def sector_crc(device, physical: int) -> int:
    return fetch_crcs(device, physical - APP_START, 1)[0]


def restore_sector(device, physical: int, backup: bytes) -> None:
    relative = physical - APP_START
    sector = backup[physical:physical + BLOCK_SIZE]
    expected_crc = binascii.crc_hqx(sector, 0)
    print(f"Restoring sector 0x{physical:X}", flush=True)
    exchange(device, make_packet(0xC2, struct.pack("<II", relative, 2)),
             0xC2, show_raw=False)
    erased_crc = sector_crc(device, physical)
    if erased_crc != BLANK_CRC:
        raise RuntimeError(f"Erase verification failed at 0x{physical:X}: "
                           f"0x{erased_crc:04X} != 0x{BLANK_CRC:04X}")
    if sector != b"\xff" * BLOCK_SIZE:
        # The upstream host writes the first app header last. Preserve that
        # ordering when restoring the first physical app sector.
        ranges = [(32, BLOCK_SIZE), (0, 32)] if physical == APP_START else [(0, BLOCK_SIZE)]
        for start, end in ranges:
            for offset in range(start, end, WRITE_CHUNK):
                payload = sector[offset:min(offset + WRITE_CHUNK, end)]
                if payload == b"\xff" * len(payload):
                    continue
                body = struct.pack("<II", relative + offset, len(payload)) + payload
                exchange(device, make_packet(0xC3, body), 0xC3, show_raw=False)
    actual_crc = sector_crc(device, physical)
    if actual_crc != expected_crc:
        raise RuntimeError(f"Restore verification failed at 0x{physical:X}: "
                           f"0x{actual_crc:04X} != 0x{expected_crc:04X}")
    print(f"Verified sector 0x{physical:X}: CRC 0x{actual_crc:04X}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inspect", "restore"))
    parser.add_argument("--backup", required=True, type=Path)
    parser.add_argument("--target", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--resume", action="store_true",
                        help="Allow backup/mixed CRCs after an interrupted restore")
    parser.add_argument("--yes", action="store_true")
    args = parser.parse_args()
    if args.command == "restore" and not args.yes:
        parser.error("restore requires --yes")
    if args.command == "inspect" and args.resume:
        parser.error("--resume is only meaningful with restore")

    backup, target, changed = load_plan(args.backup, args.target, args.manifest)
    changed_set = set(changed)
    device = open_radio()
    try:
        before = scan(device, target)
        unexpected = sorted(physical for physical in before if physical not in changed_set)
        if unexpected:
            raise RuntimeError("Unexpected flash changes outside restore plan: "
                               + ", ".join(f"0x{x:X}" for x in unexpected))
        if args.command == "inspect":
            restored = 0
            pending = 0
            partial = []
            for physical in changed:
                actual = sector_crc(device, physical)
                backup_crc = binascii.crc_hqx(
                    backup[physical:physical + BLOCK_SIZE], 0)
                target_crc = binascii.crc_hqx(
                    target[physical:physical + BLOCK_SIZE], 0)
                if actual == backup_crc:
                    restored += 1
                elif actual == target_crc:
                    pending += 1
                else:
                    partial.append(physical)
            print(f"Pinned restore plan: {restored} restored, {pending} pending, "
                  f"{len(partial)} partial/unknown sectors; no writes sent")
            if partial:
                print("Partial/unknown: " + ", ".join(f"0x{x:X}" for x in partial))
            return 0
        if not args.resume and before:
            raise RuntimeError("Current flash differs from the pinned failed image; "
                               "run inspect, then use --resume if recovering an interrupted restore")

        # High addresses first; physical 0x5000 (the app header) is last.
        for physical in sorted(changed, reverse=True):
            current = sector_crc(device, physical)
            backup_crc = binascii.crc_hqx(
                backup[physical:physical + BLOCK_SIZE], 0)
            target_crc = binascii.crc_hqx(
                target[physical:physical + BLOCK_SIZE], 0)
            if current == backup_crc:
                print(f"Already restored 0x{physical:X}", flush=True)
                continue
            if current != target_crc and not args.resume:
                raise RuntimeError(f"Unexpected CRC at 0x{physical:X}; no erase sent")
            restore_sector(device, physical, backup)

        after = scan(device, backup)
        if after:
            raise RuntimeError("Post-restore CRC scan differs from pinned backup: "
                               + ", ".join(f"0x{x:X}" for x in sorted(after)))
        print("Accessible primary flash matches the pinned pre-logo backup "
              "at every 4 KiB CRC. No reboot command sent.")
        return 0
    finally:
        device.close()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"Restore error: {exc}", file=sys.stderr)
        sys.exit(1)
