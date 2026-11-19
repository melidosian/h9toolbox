#!/usr/bin/env python3
"""Check or stage a built APRS bundle over the H9's PTT+2 USB HID interface."""
from __future__ import annotations

import argparse
from binascii import crc32
from hashlib import sha256
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "hid-recovery"))
import restore_stock as hid  # noqa: E402
from probe_hid_recovery import fetch_crcs  # noqa: E402

ADDRESS = 0xF5000
SIZE = 0x7000
IMAGE_OFFSET = 0x2000
IMAGE_SIZE = 17304
SHIFTS = (1, 37, 1024, 2048, 3072, 4095)


def load_stage(directory: Path) -> bytes:
    manifest = json.loads((directory / "manifest.json").read_text())
    stage = (directory / "aprs-stage-0xf5000.bin").read_bytes()
    if len(stage) != SIZE or sha256(stage).hexdigest() != manifest["stage_sha256"]:
        raise ValueError("Stage bundle differs from the build manifest")
    package = (directory / "h9-kiss-aprs-stage.fw").read_bytes()
    if sha256(package).hexdigest() != manifest["main_package_sha256"]:
        raise ValueError("Main firmware differs from the matching build manifest")
    modem = ROOT.parent / "kiss-tnc" / "APRS-1.0.15-rx15-rxff.bin"
    code_size = manifest["stage_code_size"]
    tail = IMAGE_OFFSET + IMAGE_SIZE
    if (not 0 < code_size <= IMAGE_OFFSET
            or crc32(stage[:code_size]) != int(manifest["stage_code_crc32"], 16)
            or stage[code_size:IMAGE_OFFSET] != b"\xff" * (IMAGE_OFFSET - code_size)
            or stage[IMAGE_OFFSET:tail] != modem.read_bytes()
            or stage[tail:] != b"\xff" * (SIZE - tail)):
        raise ValueError("Unexpected code, modem image, or padding in the stage")
    return stage


def verify_shifted(device, data: bytes) -> None:
    sector = hid.BLOCK_SIZE
    count = len(data) // sector - 1
    for shift in SHIFTS:
        actual = fetch_crcs(device, ADDRESS - hid.APP_START + shift, count)
        expected = [hid.crc(data[pos:pos + sector]) for pos in range(shift, count * sector + shift, sector)]
        if actual != expected:
            raise RuntimeError(f"Staging window differs at shift {shift}; use a flash backup to check its contents")


def inspect(device, stage: bytes) -> list[int]:
    sector = hid.BLOCK_SIZE
    count = SIZE // sector
    present = hid.hid_crcs(device, ADDRESS, count)
    if len(present) != count:
        raise RuntimeError("Incomplete stage CRC scan")
    current = bytearray()
    todo = []
    for i, actual in enumerate(present):
        offset = i * sector
        wanted = stage[offset:offset + sector]
        if actual == hid.crc(hid.BLANK):
            current.extend(hid.BLANK)
            if wanted != hid.BLANK:
                todo.append(offset)
        elif actual == hid.crc(wanted):
            current.extend(wanted)
        else:
            raise RuntimeError(f"Staging sector 0x{ADDRESS + offset:X} contains other data")
    verify_shifted(device, current)
    return todo


def write_stage(device, stage: bytes, *, write: bool) -> None:
    todo = inspect(device, stage)
    label = " ".join(f"0x{ADDRESS + offset:X}" for offset in todo)
    if todo:
        print(f"Erased sectors available for staging: {label}", flush=True)
    if not write:
        print("Read-only inspection complete" if todo else "Matching bundle is already staged")
        return

    sector = hid.BLOCK_SIZE
    # Stage the executable last. Flash programming only consumes erased bytes.
    # Leave a partially written sector for inspection if the transfer fails.
    for offset in reversed(todo):
        for pos in range(offset, offset + sector, hid.WRITE_CHUNK):
            payload = stage[pos:min(pos + hid.WRITE_CHUNK, offset + sector)]
            if payload != b"\xff" * len(payload):
                hid.hid_write(device, ADDRESS + pos, payload)
        expected = hid.crc(stage[offset:offset + sector])
        if hid.hid_crcs(device, ADDRESS + offset, 1) != [expected]:
            raise RuntimeError(f"Stage write verification failed at 0x{ADDRESS + offset:X}")
        print(f"Verified staged sector 0x{ADDRESS + offset:X}", flush=True)
    if inspect(device, stage):
        raise RuntimeError("The final stage checksum scan found missing sectors")
    print("Stage verified by aligned and shifted device checksums. Install its matching main package.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build")
    parser.add_argument("--write", action="store_true", help="program the bundle into erased staging sectors")
    args = parser.parse_args()
    try:
        stage = load_stage(args.build_dir)
        device = hid.open_radio()
        try:
            write_stage(device, stage, write=args.write)
        finally:
            device.close()
    except (ImportError, OSError, RuntimeError, ValueError, KeyError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
