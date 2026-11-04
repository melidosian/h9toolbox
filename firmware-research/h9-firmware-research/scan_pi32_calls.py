#!/usr/bin/env python3
"""Find direct Pi32 call encodings that a seeded call graph may miss.

Scans every even app offset, including data. Matches are candidates until the
surrounding code is decoded and independently checked. This reads only the
exact stock TD-H9 v1.0.38 app.bin; it does not connect to a radio.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import importlib
from pathlib import Path
import re
import sys


STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
BASE = 0x01E00100
DIRECT_CALL = re.compile(r"^call (-?\d+)$")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("--isa-repo", type=Path, required=True)
    parser.add_argument("--target", type=lambda s: int(s, 0), action="append", required=True,
                        help="App offset of a direct-call target; repeat to search several")
    args = parser.parse_args()
    app = args.app.read_bytes()
    if sha256(app).hexdigest() != STOCK_SHA256:
        parser.error("app.bin does not match stock TD-H9 v1.0.38")
    sys.path.insert(0, str(args.isa_repo))
    table = importlib.import_module("pi32v2isa.table")
    objdump = importlib.import_module("pi32v2isa.objdump")
    decoder = objdump.decoder_for(table.load(), mcpu="r3")
    targets = set(args.target)
    found = 0
    for offset in range(0, len(app) - 1, 2):
        decoded = decoder.decode(app, offset)
        if decoded is None:
            continue
        _, _, length, mnemonic = decoded
        match = DIRECT_CALL.fullmatch(mnemonic)
        if match is None or length not in (2, 4, 6, 8):
            continue
        target = offset + length + int(match.group(1))
        if target in targets:
            print(f"app+0x{offset:06X} (0x{BASE + offset:08X}) -> "
                  f"app+0x{target:06X} (0x{BASE + target:08X}) "
                  f"bytes={app[offset:offset + length].hex()}")
            found += 1
    print(f"Candidate call sites: {found}; linear scan includes data")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
