#!/usr/bin/env python3
"""Query JieLi HID update metadata and optional flash CRCs without writing.

The packet is reconstructed from Jieli-Tech/fw-Bootloader's
update_tools/win-usb_hid/main.cpp and checked against the supplied serial
updater's captured first request. Only DEVICE_INIT (0xC0) and FLASH_CRC
(0xC4) are implemented; no erase, write, or reboot command exists here.
"""

from __future__ import annotations

import argparse
import binascii
from hashlib import sha256
from pathlib import Path
import struct
import sys


VID, PID = 0x4C4A, 0x4155
REPORT_DESCRIPTOR = bytes.fromhex(
    "06 00 ff 09 01 a1 01 09 02 15 00 26 ff 00 75 08 95 40 "
    "81 02 09 03 15 00 26 ff 00 75 08 95 40 91 02 c0"
)
CAPTURED_PACKET = bytes.fromhex(
    "aa5513008e61dd703e3ed8693c3ed4652f05bc004e61bcb538"
)
KEY = (12345678).to_bytes(4, "little")
FAILED_IMAGE_SHA256 = "62b3af328050c340a3e1257f8966cc76fc48aea0d7f927f868abe49b19b346c7"
APP_START = 0x5000
FLASH_SIZE = 0x100000
BLOCK_SIZE = 4096


def make_packet(command: int, body: bytes) -> bytes:
    packet = bytearray(b"\xaa\x55" + (len(body) + 2).to_bytes(2, "little")
                       + bytes((command, 0)) + body)
    packet[4:] = bytes(byte ^ KEY[index % 4]
                       for index, byte in enumerate(packet[4:]))
    packet.extend(binascii.crc_hqx(packet, 0).to_bytes(2, "little"))
    if len(packet) > 64:
        raise ValueError(f"HID command is {len(packet)} bytes, maximum is 64")
    return b"\x00" + bytes(packet).ljust(64, b"\x00")  # HID report ID zero


def init_packet() -> bytes:
    body = b"app_dir_head".ljust(16, b"\x00") + b"\x00"  # mode zero
    report = make_packet(0xC0, body)
    if report[1:26] != CAPTURED_PACKET:
        raise RuntimeError("Generated identification packet differs from vendor log")
    return report


def exchange(device, report: bytes, expected_command: int,
             show_raw: bool = True) -> bytes:
    written = device.write(report)
    if written != len(report):
        raise RuntimeError(f"Short HID report write: {written}/{len(report)}")
    raw = bytes(device.read(64, timeout_ms=3000))
    if show_raw:
        print(f"Raw reply ({len(raw)} bytes): {raw.hex(' ') if raw else '<none>'}")
    if len(raw) != 64 or not raw.startswith(b"\xaa\x55"):
        raise RuntimeError("Missing or malformed HID reply")
    content_length = int.from_bytes(raw[2:4], "little")
    if not 2 <= content_length <= 58:
        raise RuntimeError(f"Invalid HID reply length {content_length}")
    end = 4 + content_length
    reported_crc = int.from_bytes(raw[end:end + 2], "little")
    if binascii.crc_hqx(raw[:end], 0) != reported_crc:
        raise RuntimeError("HID reply CRC does not match")
    content = bytes(byte ^ KEY[index % 4]
                    for index, byte in enumerate(raw[4:end]))
    if content[0] != expected_command or content[1] != 0:
        raise RuntimeError(f"HID reply command/status: {content[:2].hex(' ')}")
    return content[2:]


def fetch_crcs(device, offset: int, count: int) -> list[int]:
    request = make_packet(0xC4, struct.pack("<III", offset, count * BLOCK_SIZE,
                                           BLOCK_SIZE))
    body = exchange(device, request, 0xC4, show_raw=False)
    if len(body) != count * 2:
        raise RuntimeError(f"Expected {count * 2} CRC bytes, got {len(body)}")
    return list(struct.unpack("<" + "H" * count, body))


def main() -> int:
    import hid

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--crc-offset", type=lambda value: int(value, 0),
                        help="Optional 4 KiB-aligned app-file offset to checksum")
    parser.add_argument("--crc-count", type=int, default=1,
                        help="Number of consecutive 4 KiB blocks (1..16)")
    parser.add_argument("--scan-target", type=Path,
                        help="Compare all accessible sector CRCs with pinned failed image")
    args = parser.parse_args()
    if args.scan_target is not None and args.crc_offset is not None:
        parser.error("Choose --scan-target or --crc-offset")
    if args.crc_offset is not None:
        if (args.crc_offset < 0 or args.crc_offset % 4096
                or not 1 <= args.crc_count <= 16
                or args.crc_offset + args.crc_count * BLOCK_SIZE > FLASH_SIZE - APP_START):
            parser.error("CRC range must fit the aligned 1 MiB app region")

    target = None
    if args.scan_target is not None:
        target = args.scan_target.read_bytes()
        if len(target) != FLASH_SIZE or sha256(target).hexdigest() != FAILED_IMAGE_SHA256:
            parser.error("--scan-target must be the exact failed 1 MiB image")

    devices = [device for device in hid.enumerate(VID, PID)
               if device.get("usage_page") == 0xFF00
               and device.get("usage") == 1]
    if len(devices) != 1:
        raise RuntimeError(f"Expected one vendor HID interface, found {len(devices)}")
    device = hid.device()
    try:
        device.open_path(devices[0]["path"])
        descriptor = bytes(device.get_report_descriptor())
        if descriptor != REPORT_DESCRIPTOR:
            raise RuntimeError("Unexpected HID report descriptor; no request sent")
        print(f"Device: {VID:04X}:{PID:04X}; 64-byte HID input/output")
        print(f"Sending one app_dir_head identification request: "
              f"{CAPTURED_PACKET.hex(' ')}", flush=True)
        body = exchange(device, init_packet(), 0xC0)
        if len(body) != 16:
            raise RuntimeError(f"Unexpected init body length {len(body)}")
        app_address, app_length, erase_offset, block_size = struct.unpack("<IIII", body)
        print(f"App address: 0x{app_address:X}; declared length: 0x{app_length:X}; "
              f"erase offset: 0x{erase_offset:X}; block size: {block_size}")
        if args.crc_offset is not None:
            if app_address != APP_START or erase_offset != 0 or block_size != BLOCK_SIZE:
                raise RuntimeError("Unexpected update geometry; CRC query not sent")
            count = args.crc_count
            print(f"Querying {count} CRC(s) at app-file offset "
                  f"0x{args.crc_offset:X}", flush=True)
            for index, crc in enumerate(fetch_crcs(device, args.crc_offset, count)):
                print(f"CRC app+0x{args.crc_offset + index * BLOCK_SIZE:X}: 0x{crc:04X}")
        if target is not None:
            if app_address != APP_START or erase_offset != 0 or block_size != BLOCK_SIZE:
                raise RuntimeError("Unexpected update geometry; flash scan not sent")
            mismatches = []
            checked = 0
            for offset in range(0, FLASH_SIZE - APP_START, BLOCK_SIZE * 16):
                count = min(16, (FLASH_SIZE - APP_START - offset) // BLOCK_SIZE)
                actual = fetch_crcs(device, offset, count)
                for index, crc in enumerate(actual):
                    physical = APP_START + offset + index * BLOCK_SIZE
                    expected = binascii.crc_hqx(target[physical:physical + BLOCK_SIZE], 0)
                    if crc != expected:
                        mismatches.append((physical, crc, expected))
                    checked += 1
            print(f"Scanned {checked} accessible 4 KiB sectors against pinned failed image")
            if mismatches:
                for physical, actual, expected in mismatches:
                    print(f"Mismatch at 0x{physical:X}: device 0x{actual:04X}, "
                          f"failed image 0x{expected:04X}")
                return 3
            print("All accessible sector CRCs match the failed image")
        print("Stopped after read-only queries. No flash erase or image write sent.")
        return 0
    finally:
        device.close()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"Probe error: {exc}", file=sys.stderr)
        sys.exit(1)
