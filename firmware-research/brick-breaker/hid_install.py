"""HID backup and five-sector install for hosts without BR23 SCSI access.

The HID interface has no direct flash-read command. This reconstructs the
accessible settings tail from shifted 4096-byte CRC windows after verifying
the exact stock main image against both aligned and shifted CRCs.
"""

from __future__ import annotations

import binascii
from hashlib import sha256
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools" / "h9-firmware-research"))
sys.path.insert(0, str(ROOT / "tools" / "h9-flasher"))

from h9_flasher import parse_firmware  # noqa: E402
from probe_hid_recovery import APP_START, BLOCK_SIZE, FLASH_SIZE, fetch_crcs  # noqa: E402
from restore_logo_over_hid import open_radio, restore_sector  # noqa: E402


MAIN_END = 0xDA000
SECTORS = (FLASH_SIZE - APP_START) // BLOCK_SIZE
APP_SECTORS = (MAIN_END - APP_START) // BLOCK_SIZE
CHANGED = (0x5000, 0x8D000, 0xAC000, 0xAE000, 0xD9000)
SHIFTS = (1, 37, 1024, 2048, 3072, 4095)


def crc(data: bytes) -> int:
    return binascii.crc_hqx(data, 0)


def step(value: int, byte: int) -> int:
    return binascii.crc_hqx(bytes((byte,)), value)


BYTE_EFFECT = {step(0, value): value for value in range(256)}
REMOVE_CRC = [crc(bytes((value,)) + bytes(BLOCK_SIZE - 1)) for value in range(256)]


def scan(device) -> list[int]:
    values: list[int] = []
    for sector in range(0, SECTORS, 16):
        values.extend(fetch_crcs(device, sector * BLOCK_SIZE, min(16, SECTORS - sector)))
    if len(values) != SECTORS:
        raise RuntimeError("Incomplete HID sector CRC scan")
    return values


def verify_stock_seed(device, aligned: list[int], stock: bytes) -> None:
    if len(stock) != MAIN_END:
        raise ValueError("Unexpected main-flash image length")
    for index in range(APP_SECTORS):
        offset = APP_START + index * BLOCK_SIZE
        if crc(stock[offset:offset + BLOCK_SIZE]) != aligned[index]:
            raise RuntimeError(f"Radio main flash differs from stock at 0x{offset:X}")
    for shift in SHIFTS:
        for base in range(0, APP_SECTORS - 1, 16):
            count = min(16, APP_SECTORS - 1 - base)
            observed = fetch_crcs(device, base * BLOCK_SIZE + shift, count)
            for index, value in enumerate(observed):
                offset = APP_START + (base + index) * BLOCK_SIZE + shift
                if value != crc(stock[offset:offset + BLOCK_SIZE]):
                    raise RuntimeError(f"Shifted HID CRC differs from stock at 0x{offset:X}")


def recover_next_sector(previous: bytes, windows: list[int], expected_crc: int) -> bytes:
    if len(previous) != BLOCK_SIZE or len(windows) != BLOCK_SIZE:
        raise ValueError("Incomplete CRC window data")
    result = bytearray(BLOCK_SIZE)
    for offset in range(BLOCK_SIZE):
        rest = windows[offset] ^ REMOVE_CRC[previous[offset]]
        observed = expected_crc if offset == BLOCK_SIZE - 1 else windows[offset + 1]
        next_byte = BYTE_EFFECT.get(observed ^ step(rest, 0))
        if next_byte is None:
            raise RuntimeError(f"HID backup reconstruction failed at byte {offset}")
        result[offset] = next_byte
    if crc(result) != expected_crc:
        raise RuntimeError("Reconstructed sector CRC differs from radio")
    return bytes(result)


def reconstruct_accessible(device, aligned: list[int], stock: bytes) -> bytes:
    sectors = [stock[APP_START + i * BLOCK_SIZE:APP_START + (i + 1) * BLOCK_SIZE]
               for i in range(APP_SECTORS)]
    for base in range(APP_SECTORS - 1, SECTORS - 1, 16):
        count = min(16, SECTORS - 1 - base)
        windows = [[aligned[base + index]] + [0] * (BLOCK_SIZE - 1)
                   for index in range(count)]
        for shift in range(1, BLOCK_SIZE):
            observed = fetch_crcs(device, base * BLOCK_SIZE + shift, count)
            for index, value in enumerate(observed):
                windows[index][shift] = value
            if shift % 512 == 0:
                print(f"Reconstructing settings: window {shift}/4095 in group {base}", flush=True)
        for index in range(count):
            sectors.append(recover_next_sector(sectors[-1], windows[index],
                                               aligned[base + index + 1]))
    if len(sectors) != SECTORS:
        raise RuntimeError("Incomplete accessible flash reconstruction")
    if any(crc(sector) != aligned[index] for index, sector in enumerate(sectors)):
        raise RuntimeError("Reconstructed flash differs from sector CRC scan")
    if scan(device) != aligned:
        raise RuntimeError("Radio changed during HID backup")
    return b"".join(sectors)


def save_backup(backup_path: Path, main: bytes, accessible: bytes,
                stock_package: Path, target_package: Path) -> None:
    if backup_path.exists() or backup_path.with_suffix(backup_path.suffix + ".json").exists():
        raise FileExistsError(f"Backup already exists: {backup_path}")
    full_path = backup_path.with_suffix(backup_path.suffix + ".accessible.bin")
    if full_path.exists():
        raise FileExistsError(f"Accessible backup already exists: {full_path}")
    backup_path.parent.mkdir(parents=True, exist_ok=True)
    backup_path.write_bytes(main)
    full_path.write_bytes(accessible)
    manifest = {
        "format": "h9-brick-breaker-hid-backup-v1",
        "main_sha256": sha256(main).hexdigest(),
        "accessible_start": hex(APP_START),
        "accessible_length": len(accessible),
        "accessible_sha256": sha256(accessible).hexdigest(),
        "protected_boot_region": "0x0000..0x4fff from stock package, not read through HID",
        "stock_package_sha256": sha256(stock_package.read_bytes()).hexdigest(),
        "target_package_sha256": sha256(target_package.read_bytes()).hexdigest(),
    }
    backup_path.with_suffix(backup_path.suffix + ".json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Saved verified main backup: {backup_path}")
    print(f"Saved reconstructed accessible flash: {full_path}")


def install(stock_package: Path, target_package: Path, backup_path: Path) -> None:
    stock = parse_firmware(stock_package).image
    target = parse_firmware(target_package).image
    if len(stock) != MAIN_END or len(target) != MAIN_END:
        raise ValueError("Unexpected firmware image size")
    device = open_radio()
    try:
        before = scan(device)
        verify_stock_seed(device, before, stock)
        accessible = reconstruct_accessible(device, before, stock)
        save_backup(backup_path, stock, accessible, stock_package, target_package)
        print("Backup complete; five-sector HID write is ready.", flush=True)
        if input("Type FLASH to write Brick Breaker: ").strip() != "FLASH":
            print("No write sent; backup retained")
            return
        for sector in reversed(CHANGED):
            restore_sector(device, sector, target)
        after = scan(device)
        expected = before.copy()
        for sector in CHANGED:
            expected[(sector - APP_START) // BLOCK_SIZE] = crc(target[sector:sector + BLOCK_SIZE])
        if after != expected:
            raise RuntimeError("Post-write HID CRC map differs from target")
        print("Brick Breaker HID flash verified; power-cycle normally")
    finally:
        device.close()
