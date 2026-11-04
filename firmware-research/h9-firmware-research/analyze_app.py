#!/usr/bin/env python3
"""Read-only APRS string references and selected Pi32v2 disassembly."""

from __future__ import annotations

import argparse
from hashlib import sha256
import importlib
import json
from pathlib import Path
import re
import struct
import sys


STOCK_APP_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
OLD_APP_SHA256 = "31e6ef16c7ac3b0e3146ec062d994c7df2b5362c6d0948d5612490cc7499e0ec"
APP_LOAD_ADDRESS = 0x1E00100  # JLFS app_area_head entry field; mapping is an inference


def stock_app(path: Path) -> bytes:
    data = path.read_bytes()
    if sha256(data).hexdigest() not in (STOCK_APP_SHA256, OLD_APP_SHA256):
        raise ValueError("This analysis map is locked to the two captured TD-H9 app images")
    return data


def aprs_map(app: bytes) -> list[dict]:
    rows = []
    for match in re.finditer(rb"(?i)aprs", app):
        offset = match.start()
        beginning = app.rfind(b"\0", max(0, offset - 80), offset) + 1
        end = app.find(b"\0", offset, min(len(app), offset + 120))
        if end < 0:
            end = min(len(app), offset + 120)
        label_bytes = app[beginning:end]
        pointer = struct.pack("<I", APP_LOAD_ADDRESS + offset)
        refs = []
        start = 0
        while (found := app.find(pointer, start)) >= 0:
            refs.append(f"0x{found:X}")
            start = found + 1
        rows.append({
            "match_app_offset": f"0x{offset:X}",
            "inferred_virtual_address": f"0x{APP_LOAD_ADDRESS + offset:X}",
            "label_app_offset": f"0x{beginning:X}",
            "label": label_bytes.decode("utf-8", errors="replace"),
            "raw_label_hex": label_bytes.hex(),
            "literal_pointer_references": refs,
        })
    return rows


def disassemble(app: bytes, isa_repo: Path, offset: int, size: int) -> str:
    if offset < 0 or offset % 2 or size <= 0 or offset + size > len(app):
        raise ValueError("Disassembly range must be even aligned and within app.bin")
    sys.path.insert(0, str(isa_repo))
    table = importlib.import_module("pi32v2isa.table")
    objdump = importlib.import_module("pi32v2isa.objdump")
    decoder = objdump.decoder_for(table.load(), mcpu="r3")
    lines = [
        "; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.",
        "; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.",
        f"; app.bin SHA-256 {sha256(app).hexdigest()}",
    ]
    position = offset
    end = offset + size
    decoded = 0
    unknown = 0
    while position < end:
        hit = decoder.decode(app, position)
        if hit and position + hit[2] <= end:
            _, _, length, mnemonic = hit
            decoded += 1
        else:
            length = min(2, end - position)
            mnemonic = ".byte " + ", ".join(f"0x{byte:02X}" for byte in app[position:position + length])
            unknown += 1
        raw = app[position:position + length].hex()
        lines.append(f"0x{APP_LOAD_ADDRESS + position:08X}  app+0x{position:06X}  {raw:<12}  {mnemonic}")
        position += length
    lines.insert(3, f"; decoded instructions: {decoded}; undecoded chunks: {unknown}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    strings = sub.add_parser("aprs-map")
    strings.add_argument("app", type=Path)
    strings.add_argument("--output", required=True, type=Path)
    assembly = sub.add_parser("disasm")
    assembly.add_argument("app", type=Path)
    assembly.add_argument("--isa-repo", required=True, type=Path)
    assembly.add_argument("--offset", required=True, type=lambda x: int(x, 0))
    assembly.add_argument("--length", required=True, type=lambda x: int(x, 0))
    assembly.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise ValueError(f"Refusing to overwrite {args.output}")
        app = stock_app(args.app)
        if args.command == "aprs-map":
            result = aprs_map(app)
            payload = json.dumps({"app_sha256": sha256(app).hexdigest(),
                                  "address_mapping_status": "inferred from JLFS metadata",
                                  "aprs_strings": result}, indent=2) + "\n"
            print(f"Found {len(result)} APRS string occurrences")
        else:
            payload = disassemble(app, args.isa_repo, args.offset, args.length)
            print(f"Decoded app+0x{args.offset:X}..0x{args.offset + args.length:X}")
        with args.output.open("x", encoding="utf-8") as out:
            out.write(payload)
        return 0
    except (OSError, ValueError, ImportError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
