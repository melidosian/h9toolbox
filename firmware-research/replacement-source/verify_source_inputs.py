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
           tx_frequency_source: str, tx_defaults_source: str) -> None:
    if sha256(main_app).hexdigest() != MAIN_SHA256:
        raise ValueError("main app is not the analyzed v1.0.38 image")
    if sha256(aprs_image).hexdigest() != APRS_SHA256:
        raise ValueError("APRS image is not the analyzed v1.0.15 image")
    if sha256(main_app[0x060E16:0x060E96]).hexdigest() != (
        "647f7f73b4d96ab6a6e83a5d52fb55404994b9146993ff8ca077dfc66914a4fb"
    ):
        raise ValueError("main app_core post-sample sequence differs")
    for start, end, expected in (
        (0x024E92, 0x024E9C,
         "d0251b671345f9bcedb13a5b701224ca4dfab1c451d92effd6869a9f36d7e2f8"),
        (0x04A574, 0x04A578,
         "8d2dc18219c4cd47323a1b57a5cb80b25581565311dfee263ee9cc6cf37b9277"),
    ):
        if sha256(main_app[start:end]).hexdigest() != expected:
            raise ValueError(f"main app_core wrapper differs at 0x{start:X}")
    if main_app[0x0B16DF:0x0B16E6] != b"dev_mg\x00" or \
            sha256(main_app[0x02CF22:0x02CF66]).hexdigest() != (
                "d020b759cb79c94e2a92f7cf1fe1208deb8d47064e4dae615f3fb3fa3610426e"
            ):
        raise ValueError("main dev_mg task-launch input differs")
    if sha256(main_app[0x04A870:0x04A8AA]).hexdigest() != (
        "366d6c1012faa3cdbd84fcb8570833b93edf8479bf60e56d6f3433c5ca2a5c1e"
    ):
        raise ValueError("main dev_mg task entry differs")
    for offset, name in ((0x0B17D4, b"sdfile\x00"),
                         (0x0B1813, b"fat_nor\x00"),
                         (0x0B181B, b"res_nor\x00")):
        if main_app[offset:offset + len(name)] != name:
            raise ValueError(f"main dev_mg volume name differs at 0x{offset:X}")
    if sha256(main_app[0x04A7B2:0x04A870]).hexdigest() != (
        "00a9a912ae03cde279eb5df6db63f3c17b278cd54188d96d7a1b28dacb284b4c"
    ):
        raise ValueError("main volume registration helper differs")
    if sha256(main_app[0x052102:0x052228]).hexdigest() != (
        "dcdeae3eef2c26ed4c9a250da76ef6c205a1590bfb14fd0a7a4c14dace9135c1"
    ):
        raise ValueError("main speaker audio policy differs")
    if sha256(main_app[0x016728:0x01683C]).hexdigest() != (
        "333d84d1d2d8088a8b7005799e3b4dd9d652c5a042c0f622c87e63eff3e2bdb6"
    ):
        raise ValueError("main audio status selector differs")
    if sha256(main_app[0x0A27E0:0x0A2844]).hexdigest() != (
        "98284a1d9d211728ec88948455d2b7947ef56c605e42df0c83855a21cd069f37"
    ):
        raise ValueError("main volume driver table differs")
    for offset, expected in (
        (0x0C5EEE, "BE EA 47 31"),
        (0x0C5F2A, "BE EA 29 31"),
        (0x0C5F66, "BE EA 0B 31"),
        (0x08C180, "CB EA 4F B2"),
    ):
        if main_app[offset:offset + 4] != bytes.fromhex(expected):
            raise ValueError(f"main UART1 DMA ACK call differs at 0x{offset:X}")
    for offset, expected in (
        (0x0C5A44, "D0 EC 40 E1"),
        (0x0C5A90, "D0 EC 71 E2"),
        (0x0C5A0A, "BD EA C4 96"),
        (0x0C5A64, "BD EA 95 96"),
        (0x0C5A6C, "BD EA 91 96"),
        (0x078792, "CC EA 59 50"),
        (0x078798, "C1 FF 00 51 1E 00"),
        (0x0787E6, "48 20 BC EA 67 3C"),
        (0x0787FA, "48 21 04 8B"),
        (0x07881A, "21 60 02 EF 02 00"),
        (0x00055C, "80 50 1E 00"),
    ):
        if main_app[offset:offset + len(bytes.fromhex(expected))] != bytes.fromhex(expected):
            raise ValueError(f"main UART1 pin setup differs at 0x{offset:X}")
    ram_code = main_app[0x0C618C:0x0C618C + 0x58]
    if ram_code[:0x40] != bytes(0x40) or ram_code[0x40:] != bytes.fromhex(
        "c1 ff 08 3e 1e 00 90 61 00 ef 10 10 10 60 "
        "50 e8 fd 0b 10 61 00 17 80 00"
    ):
        raise ValueError("main relocated RAM serial-bus routine differs")
    if main_app[0x7E:0x8A] != bytes.fromhex(
        "64 e0 00 2b 72 e1 40 2f 64 e0 80 2b"
    ):
        raise ValueError("main post-RAM-copy interrupt configuration differs")
    for literal, address in ((0x080026C8, 0x20000110),
                             (0x080026D0, 0x20000114),
                             (0x080057F4, 0x2000011C),
                             (0x08005DD4, 0x20000110),
                             (0x08005DD8, 0x20000114),
                             (0x08005DDC, 0x20000118),
                             (0x08005DE0, 0x2000011C),
                             (0x08005DE4, 0x2000010E)):
        if struct.unpack_from("<I", aprs_image, literal - APRS_BASE)[0] != address:
            raise ValueError(f"APRS TX setting pointer differs at 0x{literal:08X}")
    for name, expected in (("LEAD_MS", 500), ("TAIL_MS", 100),
                           ("PARAMETER3_MS", 200), ("PERSISTENCE", 63)):
        match = re.search(rf"H9_APRS_STOCK_TX_{name}\s*=\s*(\d+)",
                          tx_defaults_source)
        if match is None or int(match.group(1)) != expected:
            raise ValueError(f"APRS stock TX {name.lower()} default differs")
    if main_app[0x05C324:0x05C329] != bytes.fromhex("03 05 09 08 18"):
        raise ValueError("main APRS TX mode table differs")
    if main_app[0x05C2D8:0x05C30C] != bytes.fromhex(
        "C0 FF F0 C0 00 00 50 EE 0A 1D C5 FF D0 1A 01 00 "
        "81 F8 10 02 41 E0 80 0A D8 EE 50 11 44 20 31 53 "
        "41 E0 A2 02 D8 EC 5A 11 42 E0 A4 02 D8 EC 5A 22 "
        "21 19 31 49"
    ):
        raise ValueError("main APRS GPS-position TX preflight differs")
    if main_app[0x082F0E:0x082F12] != bytes.fromhex("BE EA E1 C9"):
        raise ValueError("timed beacon no longer calls the known selector")
    if main_app[0x08747C:0x087498] != bytes.fromhex(
        "00 FC 0E 02 81 F8 0C 06 50 EE 7B 02 50 EE 7E 12 "
        "10 19 61 FF FF 00 18 00 BF EA 0D DD"
    ):
        raise ValueError("timed beacon caller gate differs")
    for offset, expected in ((0x088912, "BF EA 8C F5"),
                             (0x088C3C, "BF EA F7 F3"),
                             (0x089086, "BF EA D2 F1")):
        if main_app[offset:offset + 4] != bytes.fromhex(expected):
            raise ValueError(f"timed beacon top-level call differs at 0x{offset:X}")
    if main_app[0x087130:0x087138] != bytes.fromhex(
        "01 F8 57 0E 81 F8 53 0C"
    ):
        raise ValueError("main APRS control group 07/06 dispatch differs")
    if main_app[0x062C58:0x062CA0] != bytes.fromhex(
        "74 04 40 E0 A4 0B C4 FF D0 1A 01 00 D8 EE 40 10 "
        "71 E1 C0 10 D8 EE 41 10 40 E0 83 11 D8 EE 40 00 "
        "80 53 40 21 11 E1 80 41 98 43 40 20 98 44 10 64 "
        "80 4B C1 FF DC A1 E8 01 40 20 4A 32 BE EA 27 0E "
        "41 E0 64 04 D8 EC 4B 01"
    ):
        raise ValueError("main APRS control 07 03 state/timer path differs")
    if main_app[0x08A0DC:0x08A10E] != bytes.fromhex(
        "76 04 45 E0 64 04 C6 FF D0 1A 01 00 D8 EC 6A 15 "
        "04 16 90 17 BC EA F8 D3 40 20 D8 EC 6B 05 04 F8 "
        "05 02 84 46 40 22 36 04 FE EA 92 BA 8B EA 7F AF "
        "56 04"
    ):
        raise ValueError("main APRS control 07 timer callback differs")
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
    if main_app[0x078D96:0x078DB8] != bytes.fromhex(
        "77 04 C7 FF F0 C0 00 00 04 D5 7A 49 04 E8 50 20 "
        "FC 49 C6 FF D0 1A 01 00 52 EE 66 44 40 20 BE EA 29 BB"
    ):
        raise ValueError("main volume saved/live setting path differs")
    if main_app[0x078E16:0x078E46] != bytes.fromhex(
        "50 EE 6E 02 00 43 40 21 BF EA 42 68 40 E0 68 0B D8 "
        "EE 60 00 49 17 BF EA D5 3F 50 EE 6E 02 00 43 40 20 "
        "BF EA 35 68 40 16 BF EA 0B C6 BF EA 09 BC"
    ):
        raise ValueError("main volume audio/UI dispatch differs")
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
    tx_defaults = Path(__file__).resolve().parent / "aprs" / "h9_aprs_tx_defaults.h"
    verify(args.main_app.read_bytes(), args.aprs_image.read_bytes(),
           source.read_text(encoding="utf-8"),
           tx_source.read_text(encoding="utf-8"),
           tx_defaults.read_text(encoding="utf-8"))
    print("Exact stock hashes, relocated RAM byte-transfer routine, TX call/tables/limits and Dual Watch side and frequency copies, control group 07 timer/volume paths, speaker writer/GPIO bank, RF AF selector, ADC setup/IRQ/GPIO/timer bytes, APRS TX setting pointers/defaults, waveform bytes, and CRC table verified")


if __name__ == "__main__":
    main()
