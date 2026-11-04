#!/usr/bin/env python3
"""Extract and rebuild the exact TD-H9 v1.0.38 firmware layout offline.

This is a format and integrity tool, not a disassembler or an unrestricted patcher.
The input must be the supplied stock package. Output is never sent to a radio.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from hashlib import md5, sha256
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "h9-flasher"))
from h9_flasher import (BACKUP_SHA256, checked_firmware, crc16, parse_firmware,
                        parse_flash_header, xor_jieli)  # noqa: E402


APP_AREA = 0x5000
TOP_LIST = 0x20
UFW_KEY = 0xF181
CHIP_KEY = 0xF181
HEADER_SIZE = 32


class FormatError(RuntimeError):
    pass


@dataclass(frozen=True)
class Entry:
    header_offset: int
    data_offset: int
    size: int
    flags: int
    last: bool
    name: str
    header_crc: int
    data_crc: int

    def as_dict(self, data: bytes) -> dict:
        return {
            "name": self.name,
            "header_offset": f"0x{self.header_offset:X}",
            "data_offset": f"0x{self.data_offset:X}",
            "size": self.size,
            "flags": f"0x{self.flags:02X}",
            "sha256": sha256(data).hexdigest(),
        }


def decode_app_area(image: bytes) -> bytearray:
    decoded = bytearray(image)
    for offset in range(APP_AREA, len(decoded), 32):
        key = CHIP_KEY ^ ((offset - APP_AREA) >> 2)
        decoded[offset:offset + 32] = xor_jieli(decoded[offset:offset + 32], key)
    return decoded


def entry_at(decoded: bytes, offset: int, *, base: int, inline: bool) -> Entry:
    if offset < 0 or offset + HEADER_SIZE > len(decoded):
        raise FormatError(f"JLFS entry outside image at 0x{offset:X}")
    raw = decoded[offset:offset + HEADER_SIZE]
    header_crc, data_crc, relative, size, flags, _, last, name = struct.unpack(
        "<HHIIBBH16s", raw
    )
    if crc16(raw[2:]) != header_crc:
        raise FormatError(f"JLFS header CRC mismatch at 0x{offset:X}")
    data_offset = offset + HEADER_SIZE if inline else base + relative
    data_size = size - HEADER_SIZE if inline else size
    if data_size < 0 or data_offset + data_size > len(decoded):
        raise FormatError(f"JLFS data outside image at 0x{offset:X}")
    filename = name.split(b"\0", 1)[0].decode("ascii")
    data = decoded[data_offset:data_offset + data_size]
    if crc16(data) != data_crc:
        raise FormatError(f"JLFS data CRC mismatch for {filename} at 0x{offset:X}")
    return Entry(offset, data_offset, data_size, flags, last != 0, filename,
                 header_crc, data_crc)


def app_layout(decoded: bytes) -> tuple[Entry, Entry, list[Entry]]:
    area = entry_at(decoded, APP_AREA, base=APP_AREA, inline=True)
    if area.name != "app_area_head":
        raise FormatError("Unexpected application area header")
    app = entry_at(decoded, APP_AREA + HEADER_SIZE, base=APP_AREA, inline=False)
    if app.name != "app.bin" or app.data_offset != 0x5100:
        raise FormatError("Unexpected application payload layout")
    resources = []
    offset = APP_AREA + HEADER_SIZE + area.size
    for _ in range(16):
        item = entry_at(decoded, offset, base=APP_AREA, inline=True)
        resources.append(item)
        offset = item.data_offset + item.size
        if item.last:
            break
    else:
        raise FormatError("Application resource list did not end")
    if [item.name for item in resources] != [
        "cfg_tool.bin", "eq_cfg_hw.bin", "config.dat", "md5.bin"
    ]:
        raise FormatError("Unexpected application resource list")
    digest = md5(decoded[app.data_offset:app.data_offset + app.size]).hexdigest().encode("ascii")
    md5_entry = resources[-1]
    if decoded[md5_entry.data_offset:md5_entry.data_offset + md5_entry.size] != digest:
        raise FormatError("app.bin MD5 record does not match")
    return area, app, resources


def top_entries(image: bytes) -> list[Entry]:
    result = []
    for offset in range(TOP_LIST, TOP_LIST + 16 * HEADER_SIZE, HEADER_SIZE):
        raw = xor_jieli(image[offset:offset + HEADER_SIZE], 0xFFFF)
        if crc16(raw[2:]) != int.from_bytes(raw[:2], "little"):
            raise FormatError(f"Top-level JLFS header CRC mismatch at 0x{offset:X}")
        _, data_crc, data_offset, size, flags, _, last, name = struct.unpack(
            "<HHIIBBH16s", raw
        )
        filename = name.split(b"\0", 1)[0].decode("ascii")
        if filename not in {"uboot.boot", "isd_config.ini", "app_dir_head", "key_mac"}:
            raise FormatError(f"Unexpected top-level entry {filename}")
        result.append(Entry(offset, data_offset, size, flags, last != 0, filename,
                            int.from_bytes(raw[:2], "little"), data_crc))
        if last:
            break
    return result


def outer_flash_record(package: bytes) -> tuple[int, int, int]:
    header = xor_jieli(package[:0x40], UFW_KEY)
    _, _, _, count, _, _, _ = struct.unpack("<HHIHHI48s", header)
    for offset in range(0x40, 0x40 + count * 0x50, 0x50):
        record = xor_jieli(package[offset:offset + 0x50], UFW_KEY)
        kind, _, _, _, data_offset, size, _, _, name = struct.unpack(
            "<HHHHIII44s16s", record
        )
        if kind == 0 and name.split(b"\0", 1)[0] == b"flash.bin":
            return offset, data_offset, size
    raise FormatError("Outer flash.bin entry missing")


def rebuild(stock_package: bytes, replacement_app: bytes) -> bytes:
    package = bytearray(stock_package)
    firmware = checked_firmware_from_bytes(stock_package)
    image = firmware.image
    decoded = decode_app_area(image)
    original_decoded = bytes(decoded)
    area, app, resources = app_layout(decoded)
    if len(replacement_app) != app.size:
        raise FormatError(f"Replacement app must be exactly {app.size} bytes")

    decoded[app.data_offset:app.data_offset + app.size] = replacement_app
    struct.pack_into("<H", decoded, app.header_offset + 2, crc16(replacement_app))
    struct.pack_into("<H", decoded, app.header_offset,
                     crc16(decoded[app.header_offset + 2:app.header_offset + 32]))

    md5_entry = resources[-1]
    digest = md5(replacement_app).hexdigest().encode("ascii")
    decoded[md5_entry.data_offset:md5_entry.data_offset + md5_entry.size] = digest
    struct.pack_into("<H", decoded, md5_entry.header_offset + 2, crc16(digest))
    struct.pack_into("<H", decoded, md5_entry.header_offset,
                     crc16(decoded[md5_entry.header_offset + 2:md5_entry.header_offset + 32]))

    area_data = decoded[area.data_offset:area.data_offset + area.size]
    struct.pack_into("<H", decoded, area.header_offset + 2, crc16(area_data))
    struct.pack_into("<H", decoded, area.header_offset,
                     crc16(decoded[area.header_offset + 2:area.header_offset + 32]))

    encrypted = bytearray(image)
    for index in range(APP_AREA, len(image)):
        if decoded[index] != original_decoded[index]:
            encrypted[index] ^= decoded[index] ^ original_decoded[index]

    record_offset, image_offset, size = outer_flash_record(package)
    if size != len(encrypted):
        raise FormatError("Outer and inner image lengths differ")
    package[image_offset:image_offset + size] = encrypted
    outer_record = bytearray(xor_jieli(package[record_offset:record_offset + 0x50], UFW_KEY))
    struct.pack_into("<H", outer_record, 4, crc16(encrypted))
    package[record_offset:record_offset + 0x50] = xor_jieli(outer_record, UFW_KEY)

    count = struct.unpack_from("<H", xor_jieli(package[:0x40], UFW_KEY), 8)[0]
    header = bytearray(xor_jieli(package[:0x40], UFW_KEY))
    entries_end = 0x40 + count * 0x50
    struct.pack_into("<H", header, 2, crc16(package[0x40:entries_end]))
    struct.pack_into("<H", header, 0, crc16(header[2:]))
    package[:0x40] = xor_jieli(header, UFW_KEY)

    result = bytes(package)
    parsed = parse_firmware_bytes(result)
    if parsed.image != bytes(encrypted):
        raise FormatError("Repacked flash.bin failed round-trip validation")
    check_decoded = decode_app_area(parsed.image)
    app_layout(check_decoded)
    if parsed.image[:APP_AREA] != image[:APP_AREA]:
        raise FormatError("Repack changed the boot area")
    return result


def validate_repacked(stock_path: Path, candidate_path: Path):
    """Accept only an exact stock-derived rebuild with a same-size app replacement.

    Rebuilding from the candidate's decoded app and comparing every package byte
    rules out changes to the bootloader, chip config, VM layout, or extra records.
    """
    stock_package = stock_path.read_bytes()
    checked_firmware(stock_path)
    candidate_package = candidate_path.read_bytes()
    candidate = parse_firmware(candidate_path)
    decoded = decode_app_area(candidate.image)
    _, app, _ = app_layout(decoded)
    replacement_app = bytes(decoded[app.data_offset:app.data_offset + app.size])
    if candidate_package != rebuild(stock_package, replacement_app):
        raise FormatError("Candidate is not an exact app-only rebuild of the stock package")
    return candidate


def checked_firmware_from_bytes(package: bytes):
    from tempfile import TemporaryDirectory
    with TemporaryDirectory() as temp:
        path = Path(temp) / "stock.fw"
        path.write_bytes(package)
        return checked_firmware(path)


def parse_firmware_bytes(package: bytes):
    from tempfile import TemporaryDirectory
    with TemporaryDirectory() as temp:
        path = Path(temp) / "rebuilt.fw"
        path.write_bytes(package)
        return parse_firmware(path)


def extract_image(image: bytes, output: Path, metadata: dict) -> None:
    decoded = decode_app_area(image)
    area, app, resources = app_layout(decoded)
    top = top_entries(image)
    output.mkdir(parents=True, exist_ok=False)
    (output / "app.bin").write_bytes(decoded[app.data_offset:app.data_offset + app.size])
    (output / "flash.bin").write_bytes(image)
    for item in resources:
        (output / item.name).write_bytes(decoded[item.data_offset:item.data_offset + item.size])
    for item in top:
        if item.name in ("uboot.boot", "isd_config.ini"):
            (output / item.name).write_bytes(image[item.data_offset:item.data_offset + item.size])
    manifest = {
        **metadata,
        "flash_sha256": sha256(image).hexdigest(),
        "app_area": area.as_dict(decoded[area.data_offset:area.data_offset + area.size]),
        "app": app.as_dict(decoded[app.data_offset:app.data_offset + app.size]),
        "resources": [item.as_dict(decoded[item.data_offset:item.data_offset + item.size])
                      for item in resources],
        "top_level": [item.as_dict(image[item.data_offset:item.data_offset + item.size])
                      for item in top if item.data_offset + item.size <= len(image)],
        "entry_point_metadata": "0x1E00100",
        "notes": ["app.bin is decrypted machine code and data, not original C source"],
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Extracted to {output}; app.bin SHA-256 {manifest['app']['sha256']}")


def extract(stock_path: Path, output: Path) -> None:
    firmware = checked_firmware(stock_path)
    extract_image(firmware.image, output, {
        "source_kind": "official-v1.0.38-ufw",
        "package_sha256": firmware.package_sha256,
        "chip_key": f"0x{firmware.chipkey:04X}",
        "reserved_vm_start": "0xDA000",
    })


def extract_backup(backup_path: Path, output: Path) -> None:
    image = backup_path.read_bytes()
    if sha256(image).hexdigest() != BACKUP_SHA256 or len(image) != 0xDA000:
        raise FormatError("Pre-update backup hash or length differs from the captured radio image")
    header = parse_flash_header(image)
    if header.pid != b"X12345678" or header.flash_size != 0xFF000:
        raise FormatError("Pre-update backup has a different product ID or capacity")
    extract_image(image, output, {
        "source_kind": "pre-update-radio-backup",
        "source_sha256": BACKUP_SHA256,
        "chip_key": "0xF181",
        "reserved_vm_start": "0xC8000",
        "limitation": "Only flash offsets 0x00000..0xD9FFF were captured",
    })
    (output / "old-vm-overlap.bin").write_bytes(image[0xC8000:0xDA000])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    unpack = sub.add_parser("extract")
    unpack.add_argument("stock", type=Path)
    unpack.add_argument("--output", required=True, type=Path)
    backup = sub.add_parser("extract-backup")
    backup.add_argument("backup", type=Path)
    backup.add_argument("--output", required=True, type=Path)
    repack = sub.add_parser("repack")
    repack.add_argument("stock", type=Path)
    repack.add_argument("--app", required=True, type=Path)
    repack.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.command == "extract":
            extract(args.stock, args.output)
        elif args.command == "extract-backup":
            extract_backup(args.backup, args.output)
        else:
            if args.output.exists():
                raise FormatError(f"Refusing to overwrite {args.output}")
            result = rebuild(args.stock.read_bytes(), args.app.read_bytes())
            with args.output.open("xb") as out:
                out.write(result)
            print(f"Rebuilt {args.output}; SHA-256 {sha256(result).hexdigest()}")
        return 0
    except (FormatError, OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
