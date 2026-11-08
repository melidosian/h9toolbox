#!/usr/bin/env python3
"""Pinned PTT+2 HID fallback for the S1/S2 resource-flash service images."""

from __future__ import annotations

import argparse
import binascii
from hashlib import sha256
from pathlib import Path
import sys

TOOLS = Path(__file__).resolve().parent.parent / "brick-breaker" / "tools"
sys.path.insert(0, str(TOOLS / "h9-firmware-research"))
sys.path.insert(0, str(TOOLS / "h9-flasher"))
from probe_hid_recovery import APP_START, BLOCK_SIZE, fetch_crcs  # noqa: E402
from restore_logo_over_hid import open_radio, restore_sector  # noqa: E402
from h9_flasher import parse_firmware  # noqa: E402

MAIN_END = 0xDA000
STOCK_SHA256 = "74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648"
# Package SHA-256 -> embedded main-flash SHA-256 for each pinned service build.
PACKAGES = {
    "93f7a701726c274a1a7c4f1c6085f6956e25add5bc459676fc09b6792f5a6a0d":
        "152b3bd42b99b71b21b74ba30cb1e9221038642325901482d9cd3cceffec79d1",  # S1 (with bulk read)
    "639b7beac4b47b2e10c252a5b9b08adcd8792064088c40c7522e38a4932aae0b":
        "5bd6918875bf5e64d9710046b32da8d5406ef974aa01ac18e0f2d058709b35c5",  # S2 (with bulk read)
    "400b12bd407c69cf91a7eafbe1b34da7f16d51f36f615f79b7aa7b0f647898c3":
        "1d96c1500bad991a7efbed66bbe7a35a11f7b6ccd57bc128a47d80886145d23f",  # S1 (fill + wait bulk)
    "4f87f98efc676dee9a7213639b7beed3f3286076089a1a4c3a607f9f30134ddf":
        "caa1977b5e934fa62f669accd5c3d191ecd56eaa078009f5f9e04d3c12573680",  # S2 (fill + wait bulk)
    "1160c6d7c08def4fcd1fb681385792ed198558b353e341ef3e401e04e420e193":
        "8b3c9f560d9d4f11c7836dfffbc8b7990ce62ab88bb396f3b56e2a93f5666c8a",  # S1 (64-byte reply reserve)
    "e60d4cd5308c1be8c7fb25f01f8eb1301cbe8122e5f4df12dedf6e435c36a071":
        "71dc36d4374fbb7a2cf56925f335568b14c65619389b54e073b024d1e4a5b2c6",  # S2 (64-byte reply reserve)
    "4b2eaaa3b8ce4b34f35d3b00917a709f49c1c04ce800972d824cd729f7c4f4cd":
        "ae4ec5a8231192384abb68ffa72a3f2bdd8164af7f1e408979b9fba8760c0248",  # S1 (fast BLE link request)
    "200ce8e6ccc610875b1c269458213336c63212d79445e3a95a8a20194ec8f4cc":
        "c442fdfb2c2e998f92aa00d4c7fa52e4ec25bf312763b96f06802ad933fe23db",  # S1 (fixed 15 ms link slot)
    "29c500417b65bcc7a672919827bd53e19d7ab591ce5b0f95106d93b8f61ceaf7":
        "e61d181b76582bfca06f75852d3cbba5c4020fe5f88c5c37b5c1f97cc884cd96",  # S2 (write module in cave 2)
    "b9d24cb43937621358053356382ab72608bfbab677468febe4db867242098dc5":
        "83f79fb3281c37ab22a4fd3f154a6c52a50d3cf0d8063b546eb47ce3c603bac6",  # Brick Breaker + S2
}
# Changed sectors per target flash; S1 builds use the first cave only.
S1_SECTORS = (0x5000, 0x62000, 0xAC000, 0xD9000)
TWO_CAVE_SECTORS = (0x5000, 0x62000, 0xAC000, 0xAE000, 0xAF000, 0xD9000)
MERGED_SECTORS = (0x5000, 0x62000, 0x8D000, 0xAA000, 0xAC000, 0xAE000, 0xAF000, 0xD9000)
SECTORS = {"e61d181b76582bfca06f75852d3cbba5c4020fe5f88c5c37b5c1f97cc884cd96": TWO_CAVE_SECTORS,
           "83f79fb3281c37ab22a4fd3f154a6c52a50d3cf0d8063b546eb47ce3c603bac6": MERGED_SECTORS}


def crc(data: bytes, sector: int) -> int:
    return binascii.crc_hqx(data[sector:sector + BLOCK_SIZE], 0)


def load_plan(backup_path: Path, package_path: Path) -> tuple[bytes, bytes]:
    backup = backup_path.read_bytes()
    if len(backup) != MAIN_END or sha256(backup).hexdigest() != STOCK_SHA256:
        raise ValueError("Backup is not the pinned stock v1.0.38 main flash image")
    expected = PACKAGES.get(sha256(package_path.read_bytes()).hexdigest())
    if expected is None:
        raise ValueError("Package is not a pinned S1 or S2 resource service build")
    target = parse_firmware(package_path).image
    if len(target) != MAIN_END or sha256(target).hexdigest() != expected:
        raise ValueError("Embedded flash image differs from the pinned service build")
    changed = tuple(sector for sector in range(0, MAIN_END, BLOCK_SIZE)
                    if backup[sector:sector + BLOCK_SIZE] != target[sector:sector + BLOCK_SIZE])
    if changed != SECTORS.get(expected, S1_SECTORS) or backup[:APP_START] != target[:APP_START]:
        raise ValueError("Service image changes unexpected flash sectors")
    return backup, target, changed


def scan(device, backup: bytes, target: bytes, changed: tuple[int, ...]) -> dict[int, int]:
    actual = {}
    unexpected = []
    for sector in range(APP_START, MAIN_END, BLOCK_SIZE * 16):
        count = min(16, (MAIN_END - sector) // BLOCK_SIZE)
        for index, value in enumerate(fetch_crcs(device, sector - APP_START, count)):
            physical = sector + index * BLOCK_SIZE
            actual[physical] = value
            if physical not in changed and value != crc(backup, physical):
                unexpected.append(physical)
    if unexpected:
        raise RuntimeError("Unexpected changes outside the service image: " +
                           ", ".join(hex(x) for x in unexpected))
    for sector in changed:
        value = actual[sector]
        state = ("stock" if value == crc(backup, sector) else
                 "service" if value == crc(target, sector) else "partial/unknown")
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
    backup, target, changed = load_plan(args.backup, args.package)
    print(f"Pinned {len(changed)}-sector recovery plan: " + ", ".join(hex(x) for x in changed))
    if args.command == "plan":
        print("Offline validation passed; no device opened")
        return 0
    device = open_radio()
    try:
        before = scan(device, backup, target, changed)
        if args.command == "inspect":
            return 0
        # Restore app metadata first and the first application sector last.
        for sector in reversed(changed):
            if before[sector] != crc(backup, sector):
                restore_sector(device, sector, backup)
        after = scan(device, backup, target, changed)
        if any(after[sector] != crc(backup, sector) for sector in changed):
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
