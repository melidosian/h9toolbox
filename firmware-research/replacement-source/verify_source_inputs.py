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


def verify(main_app: bytes, aprs_image: bytes, oscillator_source: str,
           tx_frequency_source: str) -> None:
    if sha256(main_app).hexdigest() != MAIN_SHA256:
        raise ValueError("main app is not the analyzed v1.0.38 image")
    if sha256(aprs_image).hexdigest() != APRS_SHA256:
        raise ValueError("APRS image is not the analyzed v1.0.15 image")
    if main_app[0x05C324:0x05C329] != bytes.fromhex("03 05 09 08 18"):
        raise ValueError("main APRS TX mode table differs")
    if main_app[0x082F0E:0x082F12] != bytes.fromhex("BE EA E1 C9"):
        raise ValueError("timed beacon no longer calls the known selector")
    if main_app[0x087130:0x087138] != bytes.fromhex(
        "01 F8 57 0E 81 F8 53 0C"
    ):
        raise ValueError("main APRS control group 07/06 dispatch differs")
    if main_app[0x052228:0x05224C] != bytes.fromhex(
        "74 04 04 16 40 23 41 16 BD EA 4F 6F C0 FF D0 1A 01 00 "
        "50 EE 0D 17 72 E1 FE 40 B9 20 21 19 52 EE 0D 17 54 04"
    ):
        raise ValueError("main speaker pin/state writer differs")
    if main_app[0x0592E4:0x0592F2] != bytes.fromhex(
        "00 A8 41 E0 40 60 01 19 50 27 FF EA 8C B9"
    ):
        raise ValueError("main RF AF selector differs")
    if main_app[0x058D4C:0x058D76] != bytes.fromhex(
        "41 E0 D7 10 C0 FF D0 1A 01 00 D8 EE 00 11 01 44 "
        "50 EE 0C 01 80 48 04 84 54 EE 00 08 80 FD 03 00 "
        "BF EA 29 ED 04 82 BF EA AE ED"
    ):
        raise ValueError("main Dual Watch RF-side selector differs")
    if main_app[0x0567E6:0x0567F6] != bytes.fromhex(
        "50 65 06 F1 FC B5 D0 60 43 F0 8C 07 50 66 D0 64"
    ):
        raise ValueError("main A-side working-frequency copy differs")
    if main_app[0x0568F6:0x056906] != bytes.fromhex(
        "50 67 06 F1 0C B6 D0 60 43 F0 8C 07 50 68 D0 64"
    ):
        raise ValueError("main B-side working-frequency copy differs")
    if main_app[0x05AC6C:0x05ACA6] != bytes.fromhex(
        "41 E0 EA 10 C9 FF D0 1A 01 00 D8 EE 90 21 04 16 "
        "04 46 49 48 32 E8 00 10 40 60 40 61 04 8E "
        "02 47 40 E0 89 07 D8 EE 90 10 D1 EC 9C 0B "
        "04 86 40 E0 89 07 D8 EE 90 10 D1 EC 9C 0C"
    ):
        raise ValueError("main working TX-frequency choice differs")
    if main_app[0x078E94:0x078EC2] != bytes.fromhex(
        "75 04 C2 FF D0 1A 01 00 50 EE 26 14 43 E0 4C 02 "
        "44 21 10 18 0D 17 D8 EE 21 43 B5 EE 00 00 40 20 "
        "02 17 32 E9 1E 00 40 3E 00 17 81 E8 08 00"
    ):
        raise ValueError("main volume adjustment/clamp differs")
    if struct.unpack_from("<4I", main_app, 0x554) != (
        0x001E5000, 0x001E5040, 0x001E5080, 0x001E50C0
    ):
        raise ValueError("main GPIO bank pointers differ")
    match = re.search(r"mode2_frequency_10hz\[30\]\s*=\s*\{([^}]+)\}",
                      tx_frequency_source, re.S)
    if match is None:
        raise ValueError("main mode-2 frequency table not found in source")
    source_frequencies = tuple(int(value) for value in
                               re.findall(r"\b(\d+)u\b", match.group(1)))
    stock_frequencies = struct.unpack_from("<30I", main_app, 0x0B4F78)
    if source_frequencies != stock_frequencies:
        raise ValueError("main mode-2 frequency table differs from stock")
    limits = re.search(r"h9_main_stock_tx_limits\s*=\s*\{([^}]+)\}",
                       tx_frequency_source, re.S)
    if limits is None:
        raise ValueError("main TX limits not found in source")
    source_limits = tuple(int(value) for value in
                          re.findall(r"\b(\d+)u\b", limits.group(1)))
    uhf_low, uhf_high, vhf_low, vhf_high = struct.unpack_from(
        "<4I", main_app, 0x0D22A4)
    stock_limits = (vhf_low, vhf_high, uhf_low, uhf_high)
    if source_limits != stock_limits:
        raise ValueError("main TX limits differ from stock")
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
    for address in (0x08002F80, 0x08003240):
        if aprs_image[address - APRS_BASE:address - APRS_BASE + 2] != b"\x70\x47":
            raise ValueError(f"APRS ADC IRQ branch callback differs at 0x{address:08X}")
    for address, expected in (
        (0x080027A0, 0x40021000),
        (0x08003238, 0xFFFEC219),
        (0x0800323C, 0xC33FFFE7),
        (0x08002EE8, 0x00001001),
        (0x08005754, 0x20000230),
        (0x080057FC, 0x2000010E),
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
    tx_source = Path(__file__).resolve().parent / "main" / "h9_main_tx_frequency.c"
    verify(args.main_app.read_bytes(), args.aprs_image.read_bytes(),
           source.read_text(encoding="utf-8"),
           tx_source.read_text(encoding="utf-8"))
    print("Exact stock hashes, TX call/tables/limits and Dual Watch side and frequency copies, control groups/volume clamp, speaker writer/GPIO bank, RF AF selector, ADC setup/IRQ/GPIO/timer bytes, waveform bytes, and CRC table verified")


if __name__ == "__main__":
    main()
