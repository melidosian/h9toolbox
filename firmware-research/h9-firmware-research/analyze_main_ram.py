#!/usr/bin/env python3
"""Inspect the stock main application's RAM-resident Pi32 code offline.

The first startup copy places app+0x0C618C..0x0D3C04 at RAM address zero.
Input is pinned to the captured v1.0.38 application. Disassembly is linear;
data and unreachable bytes can look like instructions.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import importlib
from pathlib import Path
import sys


STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
RAM_SOURCE_OFFSET = 0x0C618C
RAM_CODE_BYTES = 55_928


def ram_code(path: Path) -> bytes:
    app = path.read_bytes()
    if sha256(app).hexdigest() != STOCK_SHA256:
        raise ValueError("Main application differs from pinned TD-H9 v1.0.38")
    data = app[RAM_SOURCE_OFFSET:RAM_SOURCE_OFFSET + RAM_CODE_BYTES]
    if len(data) != RAM_CODE_BYTES:
        raise ValueError("Stock RAM-code copy is truncated")
    return data


def disassemble(data: bytes, isa_repo: Path, offset: int, length: int) -> str:
    if offset < 0 or offset % 2 or length <= 0 or length % 2 or \
            offset + length > len(data):
        raise ValueError("Range must be even aligned and within RAM code")
    sys.path.insert(0, str(isa_repo))
    table = importlib.import_module("pi32v2isa.table")
    objdump = importlib.import_module("pi32v2isa.objdump")
    decoder = objdump.decoder_for(table.load(), mcpu="r3")
    lines = [
        "; TD-H9 stock main v1.0.38 RAM-code copy; RAM address = offset",
        f"; source app+0x{RAM_SOURCE_OFFSET:06X}; SHA-256 {sha256(data).hexdigest()}",
        "; Linear decode includes possible data and unreachable bytes.",
    ]
    position = offset
    end = offset + length
    while position < end:
        hit = decoder.decode(data, position)
        if hit is None or hit[2] not in (2, 4, 6, 8) or position + hit[2] > end:
            size = 2
            mnemonic = ".byte " + data[position:position + size].hex(" ")
        else:
            size = hit[2]
            mnemonic = hit[3]
        lines.append(f"0x{position:06X}  {data[position:position + size].hex():<16} {mnemonic}")
        position += size
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("--isa-repo", type=Path, required=True)
    parser.add_argument("--offset", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--length", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise ValueError(f"Refusing to overwrite {args.output}")
        data = ram_code(args.app)
        report = disassemble(data, args.isa_repo, args.offset, args.length)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    except (ImportError, OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Decoded RAM 0x{args.offset:X}..0x{args.offset + args.length:X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
