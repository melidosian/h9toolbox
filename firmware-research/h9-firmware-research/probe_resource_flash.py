#!/usr/bin/env python3
"""Read-only probe of a TD-H9 secondary SPI NOR in USB Update mode.

Uploads the already-used BR23 helper into RAM, then selects loader target 4
or 5 (the two alternate SPI-NOR selectors). No erase or flash-write command
is issued. If a large NOR is found, read the 40 KiB candidate startup-logo
region to a new file.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "h9-flasher"))
from h9_flasher import (FlashError, LOADER_ADDRESS, LOADER_FILE, LOADER_SHA256,
                        load_protocol, require_bootloader)  # noqa: E402

LOGO_ADDRESS = 0xA00000
LOGO_SIZE = 0xA000


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", required=True)
    parser.add_argument("--target", required=True, type=int, choices=(4, 5))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise FlashError(f"Refusing to overwrite {args.output}")
        data = LOADER_FILE.read_bytes()
        if len(data) != 11264 or sha256(data).hexdigest() != LOADER_SHA256:
            raise FlashError("BR23 RAM helper mismatch")
        SCSIDev, JL_UBOOT, JL_LoaderV2 = load_protocol()
        with SCSIDev(args.device) as device:
            require_bootloader(device)
            boot = JL_UBOOT(device)
            for offset in range(0, len(data), 512):
                boot.mem_write(LOADER_ADDRESS + offset, data[offset:offset + 512])
            print(f"Starting RAM helper for alternate SPI NOR target {args.target}", flush=True)
            boot.mem_jump(LOADER_ADDRESS, args.target)
            loader = JL_LoaderV2(device)
            online = loader.online_device()
            print(f"Device type: {online['type']}; JEDEC ID: 0x{online['id']:06X}")
            if online["type"] not in (JL_LoaderV2.DevType.SPI0_NOR,
                                      JL_LoaderV2.DevType.SPI1_NOR):
                raise FlashError("Secondary target is not SPI NOR")
            density = online["id"] & 0xFF
            if not 20 <= density <= 26:
                raise FlashError("Unrecognized NOR density; no flash read attempted")
            capacity = 1 << density
            print(f"Capacity from JEDEC density byte: {capacity:,} bytes")
            if capacity < LOGO_ADDRESS + LOGO_SIZE:
                raise FlashError("Secondary NOR cannot contain logical logo address")
            backup = bytearray()
            for offset in range(0, LOGO_SIZE, 256):
                backup.extend(loader.flash_read(LOGO_ADDRESS + offset, 256))
            if len(backup) != LOGO_SIZE:
                raise FlashError("Short logo-region read")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("xb") as output:
            output.write(backup)
            output.flush()
            os.fsync(output.fileno())
        print(f"Logo-region backup: {args.output} ({len(backup):,} bytes)")
        print(f"SHA-256: {sha256(backup).hexdigest()}")
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
