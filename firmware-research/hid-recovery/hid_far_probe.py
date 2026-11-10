#!/usr/bin/env python3
"""Does the PTT+2 HID updater erase and write the Doom code region?

The /doom/ web installer writes the Doom code at 0xEA000 over the vendor HID
recovery mode. CRC reads there are proven (the web flasher scans 1 MiB);
erase and write are not. This probe uses one sector at 0xFB000, past the end
of the Doom code (0xEA000 + 66,798 bytes ends at 0xFA4EE) and still inside
the region (0xEA000-0xFBFFF), so nothing that runs is ever touched:

  1. CRC 0xFB000 (must read erased, this probe's own pattern, or the known
     leftover of Doom builds 1-3)
  2. erase, CRC (expect erased)
  3. write a 4 KiB pattern in 40-byte packets, CRC (expect the pattern;
     a different CRC means the updater changes far-region bytes, for
     example by encrypting them)
  4. erase again, CRC (expect erased): the sector is left as found

    hid_far_probe.py            # step 1 only (read only)
    hid_far_probe.py --yes      # all steps
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import restore_stock as hid  # noqa: E402  (the proven HID packets)

SECTOR = 0xFB000
PATTERN = bytes((i * 37 + (i >> 8) * 11 + 0x5A) & 0xFF for i in range(hid.BLOCK_SIZE))
BLANK_CRC = hid.crc(hid.BLANK)
# What builds 1-3 of the Doom firmware left there: their far blobs filled 18
# sectors (0xEA000-0xFBFFF; later builds fit in 17). 208 bytes up to
# 0xFB109, copied from the Doom code's data (USB dump 2026-11-09,
# h9-main-flash-full-20261109-doom32.bin); nothing reads it.
LEFTOVER_CRC = 0xEFA9


def sector_crc(device) -> int:
    return hid.hid_crcs(device, SECTOR, 1)[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--yes", action="store_true", help="erase and write the probe sector (then erase it again)")
    args = parser.parse_args()

    device = hid.open_radio()
    try:
        first = sector_crc(device)
        state = ("erased" if first == BLANK_CRC else "probe pattern" if first == hid.crc(PATTERN)
                 else "early Doom build leftover" if first == LEFTOVER_CRC else "other data")
        print(f"0x{SECTOR:X} CRC 0x{first:04X} ({state}); erased = 0x{BLANK_CRC:04X}, pattern = 0x{hid.crc(PATTERN):04X}")
        if state == "other data":
            print("Refusing: the probe sector holds data this probe did not write.")
            return 1
        if not args.yes:
            print("Read only. Run with --yes to erase, write and erase the probe sector.")
            return 0

        hid.hid_erase(device, SECTOR)
        after_erase = sector_crc(device)
        print(f"erase:  CRC 0x{after_erase:04X} -> {'OK (erased)' if after_erase == BLANK_CRC else 'NOT ERASED'}")
        if after_erase != BLANK_CRC:
            return 2

        for offset in range(0, hid.BLOCK_SIZE, hid.WRITE_CHUNK):
            hid.hid_write(device, SECTOR + offset, PATTERN[offset:offset + hid.WRITE_CHUNK])
        after_write = sector_crc(device)
        print(f"write:  CRC 0x{after_write:04X} -> {'OK (bytes stored as sent)' if after_write == hid.crc(PATTERN) else 'DIFFERENT (stored bytes differ from those sent)'}")

        hid.hid_erase(device, SECTOR)
        restored = sector_crc(device)
        print(f"erase:  CRC 0x{restored:04X} -> {'OK (left erased, as found)' if restored == BLANK_CRC else 'NOT ERASED'}")
        ok = after_write == hid.crc(PATTERN) and restored == BLANK_CRC
        print("RESULT:", "the HID updater erases and writes the Doom code region." if ok else "see above.")
        return 0 if ok else 3
    finally:
        try:
            device.close()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
