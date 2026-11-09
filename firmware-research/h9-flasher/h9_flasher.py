#!/usr/bin/env python3
"""TD-H9 v1.0.38 USB-C flasher for Windows and Linux.

The firmware parser is independent of the transport. The USB/SCSI protocol code
and BR23 RAM loader are adapted from kagaimiq/jl-uboot-tool (MIT licensed).
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from hashlib import sha256
import json
import os
from pathlib import Path
import struct
import sys


VENDOR = "BR23"
PRODUCT = "UBOOT1.00"
CHIP = b"AC695X"
FLASH_ID = 0x856014  # SPI NOR ID read from this TD-H9 during the backup
PACKAGE_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
BACKUP_SHA256 = "975bf7f3ea6ec76139a8e1b48b402594b2ea34b43a6083532ece543c47ac53ee"
UFW_KEY = 0xF181
FLASH_HEADER_KEY = 0xFFFF
ERASE_SIZE = 0x1000
TRANSFER_SIZE = 256
LOADER_ADDRESS = 0x12000
LOADER_ARGUMENT = 1  # SPI NOR flash
VENDOR_DIR = Path(__file__).resolve().parent / "vendor"
LOADER_FILE = VENDOR_DIR / "data" / "br23loader.bin"
LOADER_SHA256 = "7384429e33ee1f340fbffe46aa928a0099c163e1374a40f6f9f27468a6f16ddf"


class FlashError(RuntimeError):
    pass


def crc16(data: bytes | bytearray) -> int:
    crc = 0
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


def xor_jieli(data: bytes, key: int) -> bytes:
    result = bytearray(data)
    for index in range(len(result)):
        result[index] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF
    return bytes(result)


def chipkey_from_config(data: bytes) -> int:
    if len(data) < 34 or crc16(data[:32]) != int.from_bytes(data[32:34], "little"):
        raise FlashError("Invalid chip-key record in firmware")
    threshold = sum(data[:16]) & 0xFF
    if threshold >= 0xE0:
        threshold = 0xAA
    elif threshold <= 0x10:
        threshold = 0x55
    key = 0
    for bit in range(16):
        if (data[16 + bit] ^ data[15 - bit]) < threshold:
            key |= 1 << bit
    return key


@dataclass(frozen=True)
class FlashHeader:
    vid: bytes
    pid: bytes
    flash_size: int


def parse_flash_header(image: bytes) -> FlashHeader:
    if len(image) < 32:
        raise FlashError("Flash image is too short")
    decoded = xor_jieli(image[:32], FLASH_HEADER_KEY)
    if crc16(decoded[2:]) != int.from_bytes(decoded[:2], "little"):
        raise FlashError("Invalid embedded flash header CRC")
    # JieLi leaves the displayed VID and PID strings in their original byte form.
    header = decoded[:4] + image[4:8] + decoded[8:16] + image[16:32]
    _, _, vid, flash_size, _, _, _, _, pid = struct.unpack("<HH4sIBBBB16s", header)
    return FlashHeader(vid.rstrip(b"\0"), pid.rstrip(b"\0\xff"), flash_size)


@dataclass(frozen=True)
class Firmware:
    package_sha256: str
    image_sha256: str
    image: bytes
    header: FlashHeader
    chipkey: int


def parse_firmware(path: Path) -> Firmware:
    package = path.read_bytes()
    if len(package) < 0x200 or len(package) > 16 * 1024 * 1024:
        raise FlashError("Invalid UFW package size")
    header = bytearray(package[:0x200])
    header[:0x40] = xor_jieli(header[:0x40], UFW_KEY)
    header_crc, list_crc, image_size, count, _, header_size, chip = struct.unpack_from(
        "<HHIHHI48s", header
    )
    if crc16(header[2:0x40]) != header_crc:
        raise FlashError("Invalid UFW header CRC")
    if image_size != len(package) or header_size != 0x200:
        raise FlashError("UFW size/header mismatch")
    if chip.split(b"\0", 1)[0] != CHIP:
        raise FlashError("UFW is not an AC695X package")
    if not 1 <= count <= (0x200 - 0x40) // 0x50:
        raise FlashError("Invalid UFW entry count")
    entries_end = 0x40 + count * 0x50
    if crc16(header[0x40:entries_end]) != list_crc:
        raise FlashError("Invalid UFW entry-list CRC")

    image = None
    for offset in range(0x40, entries_end, 0x50):
        record = xor_jieli(header[offset:offset + 0x50], UFW_KEY)
        kind, _, data_crc, _, data_offset, size, _, _, name = struct.unpack(
            "<HHHHIII44s16s", record
        )
        if kind == 0 and name.split(b"\0", 1)[0] == b"flash.bin":
            if image is not None or data_offset < 0x200 or data_offset + size > len(package):
                raise FlashError("Invalid flash.bin entry")
            image = package[data_offset:data_offset + size]
            if crc16(image) != data_crc:
                raise FlashError("Embedded flash.bin CRC mismatch")
    if image is None:
        raise FlashError("UFW has no flash.bin entry")
    if len(image) == 0 or len(image) % ERASE_SIZE:
        raise FlashError("Flash image is not aligned to erase sectors")

    flash_header = parse_flash_header(image)
    if len(image) > flash_header.flash_size or flash_header.flash_size > 16 * 1024 * 1024:
        raise FlashError("Embedded image exceeds declared flash capacity")

    # The top-level JieLi file list is encrypted one header at a time with 0xFFFF.
    config = None
    for offset in range(0x20, 0x20 + 16 * 32, 32):
        if offset + 32 > len(image):
            break
        record = xor_jieli(image[offset:offset + 32], FLASH_HEADER_KEY)
        record_crc = int.from_bytes(record[:2], "little")
        if crc16(record[2:]) != record_crc:
            raise FlashError("Invalid flash file-list CRC")
        _, data_offset, size, _, _, last, name = struct.unpack("<HIIBBH16s", record[2:])
        if name.split(b"\0", 1)[0] == b"isd_config.ini":
            if data_offset + size > len(image):
                raise FlashError("Chip-key record extends past flash image")
            config = image[data_offset:data_offset + size]
        if last:
            break
    if config is None:
        raise FlashError("Firmware chip-key record not found")

    return Firmware(
        package_sha256=sha256(package).hexdigest(),
        image_sha256=sha256(image).hexdigest(),
        image=image,
        header=flash_header,
        chipkey=chipkey_from_config(config),
    )


def checked_firmware(path: Path) -> Firmware:
    firmware = parse_firmware(path)
    if firmware.package_sha256 != PACKAGE_SHA256:
        raise FlashError("Flashing is locked to the supplied official TD-H9 v1.0.38 file")
    return firmware


def checked_custom_firmware(path: Path, stock_path: Path) -> Firmware:
    """Validate a custom package as an exact app-only rebuild of known stock."""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "h9-firmware-research"))
    from firmware import validate_repacked
    return validate_repacked(stock_path, path)


def backup_manifest_path(backup_path: Path) -> Path:
    return Path(str(backup_path) + ".json")


def backup_manifest(backup: bytes, firmware: Firmware) -> dict:
    return {
        "schema": 1,
        "backup_sha256": sha256(backup).hexdigest(),
        "firmware_package_sha256": firmware.package_sha256,
        "chipkey": firmware.chipkey,
        "pid_hex": firmware.header.pid.hex(),
        "flash_size": firmware.header.flash_size,
        "image_size": len(firmware.image),
    }


def require_backup_manifest(backup_path: Path, backup: bytes, firmware: Firmware) -> None:
    path = backup_manifest_path(backup_path)
    try:
        saved = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise FlashError(f"Custom recovery requires a valid backup manifest: {path}") from error
    if saved != backup_manifest(backup, firmware):
        raise FlashError(f"Backup manifest does not match the backup and target firmware: {path}")


def ensure_backup_manifest(backup_path: Path, backup: bytes, firmware: Firmware) -> None:
    path = backup_manifest_path(backup_path)
    if path.exists():
        require_backup_manifest(backup_path, backup, firmware)
        return
    with path.open("x", encoding="utf-8") as output:
        json.dump(backup_manifest(backup, firmware), output, indent=2)
        output.write("\n")
        output.flush()
        os.fsync(output.fileno())
    print(f"Recovery manifest saved: {path}")


def load_protocol():
    # Imported only for commands that touch hardware; inspect needs no packages.
    sys.path.insert(0, str(VENDOR_DIR))
    try:
        from scsiio import SCSIDev
        from jltech.uboot import JL_UBOOT, JL_LoaderV2
    except ImportError as error:
        raise FlashError("Install the Python requirements before accessing the radio") from error
    return SCSIDev, JL_UBOOT, JL_LoaderV2


def inquiry(device) -> tuple[str, str, str]:
    reply = bytearray(36)
    residue = device.execute(bytes((0x12, 0, 0, 0, 36, 0)), None, reply)
    if residue:
        raise FlashError("Short SCSI INQUIRY response")
    return tuple(reply[start:end].decode("ascii", errors="replace").strip()
                 for start, end in ((8, 16), (16, 32), (32, 36)))


def require_bootloader(device) -> None:
    vendor, product, revision = inquiry(device)
    if (vendor, product) != (VENDOR, PRODUCT):
        raise FlashError(f"Refusing device {vendor!r} {product!r} {revision!r}; expected BR23 UBOOT1.00")
    print(f"Radio bootloader: {vendor} {product} {revision}")


def start_loader(device, protocol) -> object:
    _, JL_UBOOT, JL_LoaderV2 = protocol
    loader_data = LOADER_FILE.read_bytes()
    if len(loader_data) != 11264 or sha256(loader_data).hexdigest() != LOADER_SHA256:
        raise FlashError("Unexpected BR23 RAM loader contents")
    boot = JL_UBOOT(device)
    for offset in range(0, len(loader_data), 512):
        chunk = loader_data[offset:offset + 512]
        if offset % 2048 == 0:
            print(f"Uploading RAM helper: {offset:,}/{len(loader_data):,} bytes", flush=True)
        try:
            # BR23 decrypts MengLi data as it accepts it. The bundled file is
            # already encrypted, so sending it through the cipher again is wrong.
            boot.mem_write(LOADER_ADDRESS + offset, chunk)
        except RuntimeError as error:
            raise FlashError(f"RAM helper upload failed at 0x{LOADER_ADDRESS + offset:X}: {error}") from error
    print("Starting RAM helper", flush=True)
    try:
        boot.mem_jump(LOADER_ADDRESS, LOADER_ARGUMENT)
    except RuntimeError as error:
        raise FlashError(f"RAM helper start failed: {error}") from error
    loader = JL_LoaderV2(device)
    online = loader.online_device()
    if online["type"] != JL_LoaderV2.DevType.SPI0_NOR:
        raise FlashError(f"Unexpected flash type {online['type']}; expected SPI NOR")
    print(f"SPI NOR flash ID: 0x{online['id']:06X}")
    if online["id"] != FLASH_ID:
        raise FlashError(f"Unexpected flash ID 0x{online['id']:06X}; expected 0x{FLASH_ID:06X}")
    return loader


def read_flash(loader, size: int) -> bytes:
    data = bytearray()
    for offset in range(0, size, TRANSFER_SIZE):
        if offset % 0x10000 == 0:
            print(f"Reading: {offset:,}/{size:,} bytes", flush=True)
        data.extend(loader.flash_read(offset, min(TRANSFER_SIZE, size - offset)))
        if len(data) != min(offset + TRANSFER_SIZE, size):
            raise FlashError(f"Short flash read at 0x{offset:X}")
    print(f"Read: {len(data):,}/{size:,} bytes", flush=True)
    return bytes(data)


def run_dump(device_path: str, output: Path, size: int) -> None:
    """Read-only: load the RAM helper and read `size` bytes from flash offset 0.

    Covers the whole 1 MiB chip, including the area past the app image
    (0xDA000 onward, retained settings) that backups do not include. Sends no
    erase or write commands.
    """
    if size <= 0 or size > 0x100000 or size % ERASE_SIZE:
        raise FlashError("Dump size must be a multiple of 4 KiB, at most 1 MiB")
    if output.exists():
        raise FlashError(f"Refusing to overwrite {output}")
    protocol = load_protocol()
    with protocol[0](device_path) as device:
        require_bootloader(device)
        loader = start_loader(device, protocol)
        print(f"Chip key: 0x{loader.chip_key():04X}")
        data = read_flash(loader, size)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    print(f"Dump saved (read-only): {output} ({len(data):,} bytes, SHA-256 {sha256(data).hexdigest()})")


# Erased main-flash space after the settings VM (2 x 32 KiB at 0xDA000) and
# before the end of the reserved area; no stock code writes here (see
# firmware-research/resource-flash-log.md, 2026-11-09).
FAR_START = 0xEA000
FAR_END = 0xFC000


def run_write_far(device_path: str, blob_path: Path, address: int, backup_path: Path) -> None:
    """Write raw bytes into the far region only, with a backup and readback.

    The blob is written as-is (already in on-flash form). It is padded with
    0xFF to whole 4 KiB sectors; sectors already holding the target bytes are
    skipped.
    """
    blob = blob_path.read_bytes()
    if not blob:
        raise FlashError("Far blob is empty")
    padded = blob + b"\xff" * (-len(blob) % ERASE_SIZE)
    end = address + len(padded)
    if address % ERASE_SIZE or address < FAR_START or end > FAR_END:
        raise FlashError(f"Far writes must be sector-aligned within 0x{FAR_START:X}-0x{FAR_END:X}")
    if backup_path.exists():
        raise FlashError(f"Refusing to overwrite {backup_path}")
    protocol = load_protocol()
    with protocol[0](device_path) as device:
        require_bootloader(device)
        loader = start_loader(device, protocol)
        print(f"Chip key: 0x{loader.chip_key():04X}")
        current = b"".join(loader.flash_read(page, TRANSFER_SIZE) for page in range(address, end, TRANSFER_SIZE))
        if len(current) != len(padded):
            raise FlashError("Short read of the far region")
        backup_path.parent.mkdir(parents=True, exist_ok=True)
        backup_path.write_bytes(current)
        print(f"Far region backup: {backup_path} (0x{address:X}-0x{end:X}, SHA-256 {sha256(current).hexdigest()})")
        changed = [o for o in range(0, len(padded), ERASE_SIZE) if current[o:o + ERASE_SIZE] != padded[o:o + ERASE_SIZE]]
        print(f"Updating {len(changed)} of {len(padded) // ERASE_SIZE} far sectors", flush=True)
        for o in changed:
            loader.flash_erase_sector(address + o)
            for page in range(o, o + ERASE_SIZE, TRANSFER_SIZE):
                loader.flash_write(address + page, padded[page:page + TRANSFER_SIZE])
            sector = b"".join(loader.flash_read(address + page, TRANSFER_SIZE) for page in range(o, o + ERASE_SIZE, TRANSFER_SIZE))
            if sector != padded[o:o + ERASE_SIZE]:
                raise FlashError(f"Far sector readback mismatch at 0x{address + o:X}")
            print(f"Verified far sector 0x{address + o:X}", flush=True)
        verified = b"".join(loader.flash_read(page, TRANSFER_SIZE) for page in range(address, end, TRANSFER_SIZE))
    if verified != padded:
        raise FlashError("Far region readback differs from the blob")
    print(f"Far region verified: 0x{address:X}-0x{end:X}, blob SHA-256 {sha256(blob).hexdigest()}")


def flash_image(loader, image: bytes, current: bytes) -> None:
    if len(image) != len(current) or len(image) % ERASE_SIZE:
        raise FlashError("Flash image/current contents have incompatible lengths")
    changed = [offset for offset in range(0, len(image), ERASE_SIZE)
               if image[offset:offset + ERASE_SIZE] != current[offset:offset + ERASE_SIZE]]
    print(f"Updating {len(changed)} of {len(image) // ERASE_SIZE} sectors", flush=True)
    for index, offset in enumerate(changed, 1):
        loader.flash_erase_sector(offset)
        for page in range(offset, offset + ERASE_SIZE, TRANSFER_SIZE):
            loader.flash_write(page, image[page:page + TRANSFER_SIZE])
        verified_sector = b"".join(
            loader.flash_read(page, TRANSFER_SIZE)
            for page in range(offset, offset + ERASE_SIZE, TRANSFER_SIZE)
        )
        if verified_sector != image[offset:offset + ERASE_SIZE]:
            raise FlashError(f"Sector readback mismatch at 0x{offset:X}")
        if index % 16 == 0 or index == len(changed):
            print(f"Verified sectors: {index}/{len(changed)}", flush=True)


def run_flash(device_path: str, firmware: Firmware, backup_path: Path, *,
              write: bool = True, recover: bool = False, restore: bool = False,
              custom: bool = False) -> None:
    if restore and not recover:
        raise FlashError("Restore requires recovery checks")
    if recover:
        saved = backup_path.read_bytes()
        if len(saved) != len(firmware.image):
            if custom:
                raise FlashError("Custom recovery backup has an unexpected length")
            raise FlashError("Recovery requires the exact backup captured from this radio")
        if custom:
            require_backup_manifest(backup_path, saved, firmware)
        elif sha256(saved).hexdigest() != BACKUP_SHA256:
            raise FlashError("Recovery requires the exact backup captured from this radio")
        saved_header = parse_flash_header(saved)
        if (saved_header.pid, saved_header.flash_size) != (firmware.header.pid, firmware.header.flash_size):
            raise FlashError("Recovery backup has a different product ID or flash capacity")
    target = saved if restore else firmware.image
    protocol = load_protocol()
    SCSIDev = protocol[0]
    backup_created = False
    erase_started = False
    try:
        with SCSIDev(device_path) as device:
            require_bootloader(device)
            loader = start_loader(device, protocol)
            chipkey = loader.chip_key()
            if chipkey != firmware.chipkey:
                raise FlashError(f"Chip key 0x{chipkey:04X} differs from firmware 0x{firmware.chipkey:04X}")
            print(f"Chip key: 0x{chipkey:04X}")

            current = read_flash(loader, len(firmware.image))
            if not recover:
                current_header = parse_flash_header(current)
                if (current_header.pid, current_header.flash_size) != (
                    firmware.header.pid, firmware.header.flash_size
                ):
                    raise FlashError("Current radio flash header differs in PID or capacity")
                if backup_path.exists():
                    if backup_path.read_bytes() != current:
                        raise FlashError(f"Existing backup does not match the radio: {backup_path}")
                    print(f"Existing backup matches the radio: {backup_path}")
                else:
                    with backup_path.open("xb") as backup:
                        backup_created = True
                        backup.write(current)
                        backup.flush()
                        os.fsync(backup.fileno())
                    print(f"Backup saved: {backup_path} ({sha256(current).hexdigest()})")

                if custom:
                    ensure_backup_manifest(backup_path, current, firmware)

                if not write:
                    print("Backup complete. No flash erase or write was performed.")
                    return

            if current == target:
                print("Radio already contains the requested image; no erase or write needed.")
                return
            erase_started = True
            flash_image(loader, target, current)
            verified = read_flash(loader, len(firmware.image))
            if verified != target:
                mismatch = next(i for i, (a, b) in enumerate(zip(verified, target)) if a != b)
                raise FlashError(f"Readback mismatch at flash offset 0x{mismatch:X}")
            print(f"Readback verified: {sha256(target).hexdigest()}")
            print("Restore complete." if restore else "Flash complete.",
                  "Power-cycle the radio to start the firmware.")
    except Exception:
        if erase_started:
            print(f"Flash may be incomplete. Keep the radio in update mode. Backup: {backup_path}", file=sys.stderr)
        elif backup_created and backup_path.exists() and backup_path.stat().st_size != len(firmware.image):
            backup_path.unlink()
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    inspect = sub.add_parser("inspect", help="Validate a TD-H9 .fw file without touching a radio")
    inspect.add_argument("firmware", type=Path)
    inspect.add_argument("--stock-base", type=Path,
                         help="Require an exact app-only rebuild of the supplied stock v1.0.38 package")
    probe = sub.add_parser("probe", help="Read SCSI INQUIRY only; no RAM or flash commands")
    probe.add_argument("--device", required=True)
    backup = sub.add_parser("backup", help="Load the RAM helper and back up the radio; no flash erase or write")
    backup.add_argument("firmware", type=Path)
    backup.add_argument("--device", required=True)
    backup.add_argument("--backup", required=True, type=Path)
    backup.add_argument("--stock-base", type=Path, help="Stock v1.0.38 .fw used to validate an app-only custom image")
    dump = sub.add_parser("dump", help="Read-only: read the whole SPI NOR (default 1 MiB) to a file")
    dump.add_argument("--device", required=True)
    dump.add_argument("--output", required=True, type=Path)
    dump.add_argument("--size", type=lambda v: int(v, 0), default=0x100000)
    far = sub.add_parser("write-far", help="Write a raw blob into the far region 0xEA000-0xFBFFF (backup + readback)")
    far.add_argument("blob", type=Path)
    far.add_argument("--device", required=True)
    far.add_argument("--address", type=lambda v: int(v, 0), default=FAR_START)
    far.add_argument("--backup", required=True, type=Path)
    far.add_argument("--yes", action="store_true", help="Acknowledge that this erases and writes far-region sectors")
    flash = sub.add_parser("flash", help="Back up, erase, write, and verify the supplied v1.0.38 firmware")
    flash.add_argument("firmware", type=Path)
    flash.add_argument("--device", required=True)
    flash.add_argument("--backup", required=True, type=Path)
    flash.add_argument("--stock-base", type=Path, help="Stock v1.0.38 .fw used to validate an app-only custom image")
    flash.add_argument("--yes", action="store_true", help="Acknowledge that this will erase and rewrite radio flash")
    resume = sub.add_parser("resume", help="Retry an interrupted write using the exact backup from this radio")
    resume.add_argument("firmware", type=Path)
    resume.add_argument("--device", required=True)
    resume.add_argument("--backup", required=True, type=Path)
    resume.add_argument("--stock-base", type=Path, help="Stock v1.0.38 .fw used to validate an app-only custom image")
    resume.add_argument("--yes", action="store_true", help="Acknowledge that this will erase and rewrite radio flash")
    restore = sub.add_parser("restore", help="Restore this radio's exact pre-update backup after a failed write")
    restore.add_argument("firmware", type=Path)
    restore.add_argument("--device", required=True)
    restore.add_argument("--backup", required=True, type=Path)
    restore.add_argument("--stock-base", type=Path, help="Stock v1.0.38 .fw used to validate an app-only custom image")
    restore.add_argument("--yes", action="store_true", help="Acknowledge that this will erase and rewrite radio flash")
    args = parser.parse_args()

    try:
        if args.command == "inspect":
            firmware = (checked_custom_firmware(args.firmware, args.stock_base)
                        if args.stock_base else parse_firmware(args.firmware))
            print(f"Package SHA-256: {firmware.package_sha256}")
            print(f"Flash SHA-256:   {firmware.image_sha256}")
            print(f"Flash bytes:     {len(firmware.image)}")
            print(f"Chip key:        0x{firmware.chipkey:04X}")
            print(f"VID/PID:         {firmware.header.vid!r} / {firmware.header.pid!r}")
            print(f"Known v1.0.38:   {firmware.package_sha256 == PACKAGE_SHA256}")
            if args.stock_base:
                print("Stock-derived app-only rebuild: verified")
        elif args.command == "probe":
            SCSIDev = load_protocol()[0]
            with SCSIDev(args.device) as device:
                require_bootloader(device)
        elif args.command == "backup":
            firmware = (checked_custom_firmware(args.firmware, args.stock_base)
                        if args.stock_base else checked_firmware(args.firmware))
            run_flash(args.device, firmware, args.backup, write=False,
                      custom=bool(args.stock_base))
        elif args.command == "write-far":
            if not args.yes:
                raise FlashError("Add --yes to write the far region")
            run_write_far(args.device, args.blob, args.address, args.backup)
        elif args.command == "dump":
            run_dump(args.device, args.output, args.size)
        elif args.command in ("flash", "resume", "restore"):
            if not args.yes:
                raise FlashError("Add --yes to perform the flash; use inspect or probe first")
            firmware = (checked_custom_firmware(args.firmware, args.stock_base)
                        if args.stock_base else checked_firmware(args.firmware))
            run_flash(args.device, firmware, args.backup,
                      recover=args.command in ("resume", "restore"),
                      restore=args.command == "restore", custom=bool(args.stock_base))
        return 0
    except (RuntimeError, OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
