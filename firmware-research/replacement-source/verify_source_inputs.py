#!/usr/bin/env python3
"""Check reconstructed constants against the exact captured firmware images.

Read-only. The private main and APRS images remain outside this source tree.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
from pathlib import Path
import re
import struct


MAIN_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
APRS_SHA256 = "1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33"
APRS_BASE = 0x08002000
TABLE_OFFSET = 0x40D8
TABLE_SIZE = 128
CRC_TABLE_OFFSET = 0x4158


def verify(main_app: bytes, aprs_image: bytes, oscillator_source: str) -> None:
    if sha256(main_app).hexdigest() != MAIN_SHA256:
        raise ValueError("main app is not the analyzed v1.0.38 image")
    if sha256(aprs_image).hexdigest() != APRS_SHA256:
        raise ValueError("APRS image is not the analyzed v1.0.15 image")
    if main_app[0x05C324:0x05C329] != bytes.fromhex("03 05 09 08 18"):
        raise ValueError("main APRS TX mode table differs")
    if main_app[0x082F0E:0x082F12] != bytes.fromhex("BE EA E1 C9"):
        raise ValueError("timed beacon no longer calls the known selector")
    pointer = struct.unpack_from("<I", aprs_image, 0x5E0)[0]
    if pointer != APRS_BASE + TABLE_OFFSET:
        raise ValueError("APRS waveform table pointer differs")
    match = re.search(r"quarter_wave\[128\]\s*=\s*\{([^}]+)\}",
                      oscillator_source, re.S)
    if match is None:
        raise ValueError("source waveform table not found")
    source_table = bytes(int(value, 16) for value in
                         re.findall(r"0x([0-9A-Fa-f]{2})\b", match.group(1)))
    stock_table = aprs_image[TABLE_OFFSET:TABLE_OFFSET + TABLE_SIZE]
    if source_table != stock_table:
        raise ValueError("source waveform table differs from APRS image")
    if struct.unpack_from("<H", aprs_image, 0x31F8)[0] != 0xFFFF:
        raise ValueError("APRS frame CRC initial value differs")
    if struct.unpack_from("<H", aprs_image, 0x3304)[0] != 0xF0B8:
        raise ValueError("APRS frame CRC residue differs")
    if struct.unpack_from("<I", aprs_image, 0x4040)[0] != APRS_BASE + CRC_TABLE_OFFSET:
        raise ValueError("APRS frame CRC table pointer differs")
    for address in (0x08002970, 0x080029E0, 0x080029F4):
        if struct.unpack_from("<I", aprs_image, address - APRS_BASE)[0] != 0x50000400:
            raise ValueError(f"APRS GPIOB base differs at 0x{address:08X}")
    if struct.unpack_from("<I", aprs_image, 0x080027A4 - APRS_BASE)[0] != 0x40012400:
        raise ValueError("APRS sample ADC base differs")
    if struct.unpack_from("<I", aprs_image, 0x08002304 - APRS_BASE)[0] != 0x40021000:
        raise ValueError("APRS waveform GPIO clock base differs")
    if struct.unpack_from("<I", aprs_image, 0x08002894 - APRS_BASE)[0] != 0x40012C00:
        raise ValueError("APRS sampling timer base differs")
    for address, expected in (
        (0x080027A0, 0x40021000),
        (0x08003238, 0xFFFEC219),
        (0x0800323C, 0xC33FFFE7),
        (0x08002EE8, 0x00001001),
        (0x08005754, 0x20000230),
        (0x080057F0, 0x20000100),
        (0x08005820, 0x200000FC),
        (0x08005828, 0x20000100),
    ):
        if struct.unpack_from("<I", aprs_image, address - APRS_BASE)[0] != expected:
            raise ValueError(f"APRS ADC setup literal differs at 0x{address:08X}")
    for index in range(256):
        crc = index
        for _ in range(8):
            crc = (crc >> 1) ^ (0x8408 if crc & 1 else 0)
        stock_crc = struct.unpack_from("<H", aprs_image,
                                       CRC_TABLE_OFFSET + index * 2)[0]
        if crc != stock_crc:
            raise ValueError(f"APRS frame CRC table differs at {index}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("main_app", type=Path)
    parser.add_argument("aprs_image", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent / "aprs" / "h9_aprs_oscillator.c"
    verify(args.main_app.read_bytes(), args.aprs_image.read_bytes(),
           source.read_text(encoding="utf-8"))
    print("Exact stock hashes, TX call/table, ADC setup/GPIO/timer literals, waveform bytes, and CRC table verified")


if __name__ == "__main__":
    main()
