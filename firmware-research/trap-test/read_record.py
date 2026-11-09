#!/usr/bin/env python3
"""Read the trap record (RAM 0x1E4F0) through the mask-ROM USB loader.

Read-only apart from the 16-byte cipher check the ROM session always does in
its program window (0x12000). The record is 16 bytes: "TRAP" (0x50415254)
and the stub address that fired, if a trap ran since RAM last lost power.
The resource-usb tools limit reads to their program window; this reads one
16-byte block outside it directly.

    sudo ~/h9usb/bin/python read_record.py --device 4C4A:2342
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

USB = Path(__file__).resolve().parent.parent / "resource-usb"
sys.path.insert(0, str(USB))
import rom  # noqa: E402
import usb_msc  # noqa: E402

RECORD = 0x1E4F0
MAGIC = b"TRAP"[::-1]  # stored as the little-endian word 0x50415254


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--device", default="4C4A:2342")
    args = parser.parse_args()
    vid, pid = usb_msc.parse_device(args.device)
    with usb_msc.BulkOnlyDevice(vid, pid) as transport:
        session = rom.RomSession(transport)
        print("ROM:", session.require_rom())
        print("cipher:", session.check_read_cipher(0x12F00, b"H9 trap record!!"))
        raw = session.boot.mem_read(RECORD, 16)
        data = rom.encrypt(raw) if session.reads_encrypted else raw
        print(f"RAM 0x{RECORD:X}: {data.hex(' ')}")
        if data[:4] == MAGIC:
            stub = int.from_bytes(data[4:8], "little")
            print(f"TRAP fired at stub 0x{stub:08X} (app+0x{stub - 0x01E00100:X})")
        else:
            print("No trap record (or RAM lost power since).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
