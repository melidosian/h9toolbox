#!/usr/bin/env python3
"""Read the TD-H9's full 1 MiB SPI flash into a new local file; no flash writes."""

from __future__ import annotations

import argparse
from hashlib import sha256
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "h9-flasher"))
from h9_flasher import (FlashError, checked_firmware, load_protocol, parse_flash_header,
                        read_flash, require_bootloader, start_loader)  # noqa: E402


FLASH_BYTES = 0x100000  # 0x856014 SPI NOR flash observed on this radio


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--device", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise FlashError(f"Refusing to overwrite {args.output}")
        firmware = checked_firmware(args.firmware)
        protocol = load_protocol()
        with protocol[0](args.device) as device:
            require_bootloader(device)
            loader = start_loader(device, protocol)
            key = loader.chip_key()
            if key != firmware.chipkey:
                raise FlashError(f"Chip key mismatch: 0x{key:04X}")
            image = read_flash(loader, FLASH_BYTES)
            header = parse_flash_header(image)
            if (header.pid, header.flash_size) != (firmware.header.pid,
                                                    firmware.header.flash_size):
                raise FlashError("Radio flash header differs in PID or capacity")
        with args.output.open("xb") as out:
            out.write(image)
            out.flush()
            os.fsync(out.fileno())
        print(f"Full flash saved: {args.output} ({len(image)} bytes)")
        print(f"SHA-256: {sha256(image).hexdigest()}")
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
