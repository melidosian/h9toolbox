#!/usr/bin/env python3
"""Conservative Pi32v2 call index for the exact TD-H9 application image.

It follows direct relative calls/branches from supplied function seeds. An
indirect jump, invalid decode, or unsupported flow terminates that path. The
result is an index of observed instructions, not a complete control-flow graph.
"""

from __future__ import annotations

import argparse
from collections import defaultdict, deque
from hashlib import sha256
import importlib
import json
from pathlib import Path
import re
import sys


STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
BASE = 0x01E00100
DIRECT_CALL = re.compile(r"^call (-?\d+)$")
DIRECT_GOTO = re.compile(r"^goto (-?\d+)$")
CONDITIONAL_GOTO = re.compile(r"^if .* goto (-?\d+)$")


def build_index(app: bytes, isa_repo: Path, seeds: list[int]) -> dict:
    if sha256(app).hexdigest() != STOCK_SHA256:
        raise ValueError("This index is locked to stock TD-H9 v1.0.38 app.bin")
    sys.path.insert(0, str(isa_repo))
    table = importlib.import_module("pi32v2isa.table")
    objdump = importlib.import_module("pi32v2isa.objdump")
    decoder = objdump.decoder_for(table.load(), mcpu="r3")

    queue = deque(seeds)
    seen: dict[int, tuple[int, str]] = {}
    calls: dict[int, set[int]] = defaultdict(set)
    invalid: set[int] = set()
    indirect: set[int] = set()

    def add(offset: int) -> None:
        if 0 <= offset < len(app) and offset % 2 == 0 and offset not in seen:
            queue.append(offset)

    while queue:
        offset = queue.popleft()
        if offset in seen or offset in invalid or offset >= len(app):
            continue
        decoded = decoder.decode(app, offset)
        if decoded is None:
            invalid.add(offset)
            continue
        _, _, length, mnemonic = decoded
        if length not in (2, 4, 6, 8) or offset + length > len(app):
            invalid.add(offset)
            continue
        seen[offset] = (length, mnemonic)
        next_offset = offset + length
        if call := DIRECT_CALL.fullmatch(mnemonic):
            target = next_offset + int(call.group(1))
            if 0 <= target < len(app) and target % 2 == 0:
                calls[target].add(offset)
                add(target)
            add(next_offset)
        elif branch := DIRECT_GOTO.fullmatch(mnemonic):
            add(next_offset + int(branch.group(1)))
        elif branch := CONDITIONAL_GOTO.fullmatch(mnemonic):
            add(next_offset + int(branch.group(1)))
            add(next_offset)
        elif mnemonic.startswith("goto ") or mnemonic.startswith("pc =") or "{pc," in mnemonic:
            indirect.add(offset)
        else:
            add(next_offset)

    immediate_71 = []
    for offset, (_, mnemonic) in sorted(seen.items()):
        if re.search(r"= (?:71|0x47)$", mnemonic):
            immediate_71.append({"offset": f"0x{offset:06X}", "instruction": mnemonic})

    return {
        "app_sha256": STOCK_SHA256,
        "address_base_inferred": f"0x{BASE:08X}",
        "seed_count": len(seeds),
        "decoded_instruction_count": len(seen),
        "invalid_seed_or_target_count": len(invalid),
        "indirect_control_flow_count": len(indirect),
        "immediate_71_candidates": immediate_71,
        "calls": {f"0x{target:06X}": [f"0x{site:06X}" for site in sorted(sites)]
                  for target, sites in sorted(calls.items())},
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("app", type=Path)
    parser.add_argument("--isa-repo", required=True, type=Path)
    parser.add_argument("--seeds", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--focus", action="append", type=lambda s: int(s, 0), default=[])
    args = parser.parse_args()
    try:
        if args.output.exists():
            raise ValueError(f"Refusing to overwrite {args.output}")
        seeds = [int(line.strip(), 0) for line in args.seeds.read_text().splitlines() if line.strip()]
        result = build_index(args.app.read_bytes(), args.isa_repo, seeds)
        with args.output.open("x", encoding="utf-8") as output:
            json.dump(result, output, indent=2)
            output.write("\n")
        print(f"Decoded {result['decoded_instruction_count']:,} instructions from {len(seeds):,} seeds")
        print(f"Direct call targets: {len(result['calls']):,}")
        for focus in args.focus:
            print(f"app+0x{focus:06X} callers: {', '.join(result['calls'].get(f'0x{focus:06X}', [])) or 'none observed'}")
        return 0
    except (OSError, ValueError, ImportError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
