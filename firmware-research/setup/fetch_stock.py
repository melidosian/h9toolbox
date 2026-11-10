#!/usr/bin/env python3
"""Fetch the stock TD-H9 v1.0.38 firmware and extract app.bin for the builds.

    python3 -I firmware-research/setup/fetch_stock.py              # download the pinned ZIP
    python3 -I firmware-research/setup/fetch_stock.py --zip <file>  # a ZIP you already have
    python3 -I firmware-research/setup/fetch_stock.py --fw <file>   # a TD-H9-V1.0.38.fw you already have

Writes firmware-research/setup/stock/TD-H9-V1.0.38.fw and app.bin (git-ignored:
this is TIDRADIO's firmware). Every step is pinned:

- the Type-C v1.0.38 update ZIP from the TDH9 mirror on GitHub (the same file
  the web flasher fetches, dist/firmware-update/stock-source.mjs),
- the .fw inside it (package SHA-256 c4d0beb5..., firmware.mjs STOCK_PACKAGE_SHA256),
- app.bin decoded from it (SHA-256 11a07bc0..., the image every build and
  analysis in this repo is pinned to).

Extraction: UFW header and entry list (CRC-16, XOR keystream 0xF181) ->
flash.bin -> app area from 0x5000 decrypted per 32-byte block (key
0xF181 ^ (offset >> 2)) -> JLFS directory -> app.bin, all CRC-checked. Format
references: mjwgeek/TIDRADIO_TD-H9 and kagaimiq/jl-misctools.
"""

from __future__ import annotations

import argparse
import binascii
import hashlib
import io
import struct
import sys
import urllib.request
import zipfile
from pathlib import Path

ZIP_URL = ("https://raw.githubusercontent.com/teedubyeah/TDH9/1ccb67c2684204b0097533486c545607caaf65b8/"
           "Firmware/1.0.38/H9_Update-Type-C_1.0.38.zip")
ZIP_SHA256 = "1d1b6f87705f026dc5c5b118786d133cbc7f0589f606c8572581df981492155a"
ZIP_SIZE = 7091957
FW_PATH = "H9_Update-Type-C/TD-H9-V1.0.38.fw"
FW_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
APP_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
OUT = Path(__file__).resolve().parent / "stock"


def crc(data: bytes) -> int:
    return binascii.crc_hqx(data, 0)


def xor_stream(data: bytes, key: int = 0xF181) -> bytes:
    out = bytearray()
    for byte in data:
        out.append(byte ^ (key & 0xFF))
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF
    return bytes(out)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"fetch_stock: {message}")


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def flash_from_fw(fw: bytes) -> bytes:
    head = xor_stream(fw[:64])
    hcrc, lcrc, total, count, _unknown, hsize, _chip = struct.unpack("<HHIHHI48s", head)
    require(crc(head[2:]) == hcrc, "UFW header CRC mismatch")
    require(total == len(fw) and hsize == 512 and 0 < count <= 5, "unexpected UFW layout")
    table = fw[64:64 + count * 80]
    require(crc(table) == lcrc, "UFW entry list CRC mismatch")
    for i in range(count):
        raw = xor_stream(table[i * 80:(i + 1) * 80])
        _kind, _index, checksum, _r, offset, size, padded, _extra, name = struct.unpack("<HHHHIII44s16s", raw)
        if name.split(b"\0")[0] == b"flash.bin":
            payload = fw[offset:offset + padded]
            require(len(payload) == padded and crc(payload) == checksum, "flash.bin CRC mismatch")
            return fw[offset:offset + size]
    raise SystemExit("fetch_stock: no flash.bin in the package")


def jlfs(data: bytes, offset: int) -> dict:
    raw = data[offset:offset + 32]
    hcrc, dcrc, ptr, size, flags, _r, index, name = struct.unpack("<HHIIBBH16s", raw)
    require(crc(raw[2:]) == hcrc, f"JLFS header CRC mismatch at 0x{offset:x}")
    return dict(crc=dcrc, pointer=ptr, size=size, flags=flags, index=index, name=name.split(b"\0")[0])


def app_from_flash(flash: bytes) -> bytes:
    base = 0x5000
    decoded = bytearray(flash)
    for off in range(base, len(flash), 32):
        decoded[off:off + 32] = xor_stream(flash[off:off + 32], 0xF181 ^ ((off - base) >> 2))
    area = jlfs(decoded, base)
    require(area["name"] == b"app_area_head" and area["flags"] == 0x83, "unexpected app area")
    require(crc(bytes(decoded[base + 32:base + area["size"]])) == area["crc"], "app area CRC mismatch")
    for off in range(base + 32, base + area["size"], 32):
        item = jlfs(decoded, off)
        if item["name"] == b"app.bin":
            app = bytes(decoded[base + item["pointer"]:base + item["pointer"] + item["size"]])
            require(crc(app) == item["crc"], "app.bin CRC mismatch")
            return app
        if item["index"]:
            break
    raise SystemExit("fetch_stock: no app.bin in the app directory")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--zip", type=Path, help="use this Type-C v1.0.38 ZIP instead of downloading it")
    source.add_argument("--fw", type=Path, help="use this TD-H9-V1.0.38.fw directly")
    parser.add_argument("-o", "--output", type=Path, default=OUT)
    args = parser.parse_args()

    if args.fw:
        fw = args.fw.read_bytes()
    else:
        if args.zip:
            archive = args.zip.read_bytes()
        else:
            print(f"Downloading {ZIP_URL} ({ZIP_SIZE:,} bytes)…", file=sys.stderr)
            with urllib.request.urlopen(ZIP_URL, timeout=120) as response:
                archive = response.read()
        require(len(archive) == ZIP_SIZE and sha(archive) == ZIP_SHA256, "the ZIP does not match its pinned SHA-256")
        with zipfile.ZipFile(io.BytesIO(archive)) as z:
            fw = z.read(FW_PATH)
    require(sha(fw) == FW_SHA256, "TD-H9-V1.0.38.fw does not match its pinned SHA-256")
    app = app_from_flash(flash_from_fw(fw))
    require(sha(app) == APP_SHA256, "app.bin does not match its pinned SHA-256")

    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "TD-H9-V1.0.38.fw").write_bytes(fw)
    (args.output / "app.bin").write_bytes(app)
    print(f"{args.output / 'TD-H9-V1.0.38.fw'}  sha256 {FW_SHA256}")
    print(f"{args.output / 'app.bin'}  {len(app):,} bytes  sha256 {APP_SHA256}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
