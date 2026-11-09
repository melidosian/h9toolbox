#!/usr/bin/env python3
"""Restore any TD-H9 main firmware to exact stock v1.0.38 over PTT+2 HID.

General PTT+2 recovery, not tied to one build: it compares every main-app
sector (0x5000-0xD9FFF, 213 sectors) on the radio with the user's stock
restore point by CRC, rewrites each sector that differs, then rescans and
requires the whole map to match stock. Use it for Doom and other custom
firmware, then install H9 Toolbox Firmware from stock (web flasher) if
wanted.

CRC-16 per sector cannot tell every change apart from stock (about 1 in
65,536 per changed sector). Pass the package(s) you installed with --known:
every sector those differ from stock in is rewritten even if its CRC happens
to match. --all rewrites all 213 sectors.

Protocol and write order are those of the hardware-proven logo and C-hello
restores (erase C2, 40-byte C3 writes, C4 CRC windows; the first app sector,
which holds the app header, is written last). Re-running after an
interruption simply continues: only sectors that still differ are written.

    restore_stock.py inspect --backup <stock restore point> [--known <.fw> ...]
    restore_stock.py restore --backup <stock restore point> [--known <.fw> ...] --yes
"""

from __future__ import annotations

import argparse
import binascii
from hashlib import sha256
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parent
TOOLS = ROOT.parent / "brick-breaker" / "tools"
sys.path.insert(0, str(TOOLS / "h9-firmware-research"))
sys.path.insert(0, str(ROOT.parent / "h9-flasher"))

APP_START = 0x5000
MAIN_END = 0xDA000          # end of the app image; the settings VM follows
BLOCK_SIZE = 4096
WRITE_CHUNK = 40            # below the vendor HID host's maximum payload
STOCK_SHA256 = "74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648"
SECTORS = tuple(range(APP_START, MAIN_END, BLOCK_SIZE))
BLANK = b"\xff" * BLOCK_SIZE


def crc(data: bytes) -> int:
    return binascii.crc_hqx(data, 0)


# Hardware access: three thin wrappers over the proven HID packets, replaced
# by a simulated radio in the tests.
def hid_crcs(device, physical: int, count: int) -> list[int]:
    from probe_hid_recovery import fetch_crcs
    return fetch_crcs(device, physical - APP_START, count)


def hid_erase(device, physical: int) -> None:
    from probe_hid_recovery import exchange, make_packet
    exchange(device, make_packet(0xC2, struct.pack("<II", physical - APP_START, 2)), 0xC2, show_raw=False)


def hid_write(device, address: int, payload: bytes) -> None:
    from probe_hid_recovery import exchange, make_packet
    body = struct.pack("<II", address - APP_START, len(payload)) + payload
    exchange(device, make_packet(0xC3, body), 0xC3, show_raw=False)


def open_radio():
    from restore_logo_over_hid import open_radio as open_hid
    return open_hid()


def load_stock(path: Path) -> bytes:
    stock = path.read_bytes()
    if len(stock) != MAIN_END or sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Backup is not the pinned stock v1.0.38 main-flash restore point (74243eae…)")
    return stock


def known_sectors(stock: bytes, packages: list[Path]) -> set[int]:
    """Sectors any installed package changed from stock (rewritten regardless of CRC)."""
    from h9_flasher import parse_firmware
    sectors: set[int] = set()
    for package in packages:
        image = parse_firmware(package).image
        if len(image) != MAIN_END:
            raise ValueError(f"{package.name}: unexpected main image length {len(image)}")
        if image[:APP_START] != stock[:APP_START]:
            raise ValueError(f"{package.name}: boot area differs from stock; not an app-only build")
        sectors |= {s for s in SECTORS if image[s:s + BLOCK_SIZE] != stock[s:s + BLOCK_SIZE]}
    return sectors


def scan(device, stock: bytes) -> dict[int, int]:
    """Live CRC of every app sector."""
    actual: dict[int, int] = {}
    for first in range(APP_START, MAIN_END, BLOCK_SIZE * 16):
        count = min(16, (MAIN_END - first) // BLOCK_SIZE)
        for index, value in enumerate(hid_crcs(device, first, count)):
            actual[first + index * BLOCK_SIZE] = value
    if set(actual) != set(SECTORS):
        raise RuntimeError("Incomplete CRC scan")
    differing = [s for s in SECTORS if actual[s] != crc(stock[s:s + BLOCK_SIZE])]
    print(f"Scanned {len(actual)} app sectors: {len(actual) - len(differing)} stock, "
          f"{len(differing)} differ", flush=True)
    return actual


def plan(actual: dict[int, int], stock: bytes, known: set[int], everything: bool) -> list[int]:
    differing = {s for s in SECTORS if actual[s] != crc(stock[s:s + BLOCK_SIZE])}
    chosen = set(SECTORS) if everything else differing | known
    # Highest first, so the first app sector (header) is written last.
    return sorted(chosen, reverse=True)


def restore_sector(device, physical: int, stock: bytes) -> None:
    sector = stock[physical:physical + BLOCK_SIZE]
    hid_erase(device, physical)
    erased = hid_crcs(device, physical, 1)[0]
    if erased != crc(BLANK):
        raise RuntimeError(f"Erase verification failed at 0x{physical:X}: 0x{erased:04X}")
    if sector != BLANK:
        ranges = [(32, BLOCK_SIZE), (0, 32)] if physical == APP_START else [(0, BLOCK_SIZE)]
        for start, end in ranges:
            for offset in range(start, end, WRITE_CHUNK):
                payload = sector[offset:min(offset + WRITE_CHUNK, end)]
                if payload != b"\xff" * len(payload):
                    hid_write(device, physical + offset, payload)
    written = hid_crcs(device, physical, 1)[0]
    if written != crc(sector):
        raise RuntimeError(f"Restore verification failed at 0x{physical:X}: "
                           f"0x{written:04X} != 0x{crc(sector):04X}")


def run(device, stock: bytes, known: set[int], *, write: bool, everything: bool = False) -> list[int]:
    actual = scan(device, stock)
    todo = plan(actual, stock, known, everything)
    extra = sorted(s for s in todo if actual[s] == crc(stock[s:s + BLOCK_SIZE]))
    print(f"Restore plan: {len(todo)} sector(s)"
          + (f", {len(extra)} rewritten despite a stock CRC (known/--all)" if extra else ""), flush=True)
    if todo:
        print("  " + ", ".join(f"0x{s:X}" for s in sorted(todo)), flush=True)
    if not write:
        print("Inspect only; nothing written", flush=True)
        return todo
    for index, physical in enumerate(todo, 1):
        restore_sector(device, physical, stock)
        print(f"Restored 0x{physical:X} ({index}/{len(todo)})", flush=True)
    after = scan(device, stock)
    bad = [s for s in SECTORS if after[s] != crc(stock[s:s + BLOCK_SIZE])]
    if bad:
        raise RuntimeError("Sectors still differ from stock: " + ", ".join(f"0x{s:X}" for s in bad))
    print("All 213 app sectors match stock v1.0.38; power-cycle the radio", flush=True)
    return todo


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=("inspect", "restore"))
    parser.add_argument("--backup", type=Path, required=True, help="stock v1.0.38 restore point (74243eae…)")
    parser.add_argument("--known", type=Path, action="append", default=[],
                        help="package installed on the radio; its changed sectors are always rewritten")
    parser.add_argument("--all", action="store_true", help="rewrite all 213 app sectors")
    parser.add_argument("--yes", action="store_true")
    args = parser.parse_args()
    if args.command == "restore" and not args.yes:
        parser.error("restore requires --yes")
    try:
        stock = load_stock(args.backup)
        known = known_sectors(stock, args.known)
        device = open_radio()
        try:
            run(device, stock, known, write=args.command == "restore", everything=args.all)
        finally:
            device.close()
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Restore error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
