#!/usr/bin/env python3
"""First-pass estimate of stock main-app code that is dead once stock my_app
is replaced (the hybrid takeover plan, see DESIGN.md).

Inputs: the stock app.bin and evidence/stock/pi32-call-index-v2.json
(callee -> call sites, direct calls only). Function starts are the callees;
a call site belongs to the nearest start at or below it. Live roots: the
reset entry and every function whose absolute address appears anywhere in
the image (function pointers, task and handler tables) - a conservative
stand-in for the 1,477 indirect jumps the index could not resolve. Every
2-aligned 32-bit value in the image that points at an even app-code address
holding a function prologue (a register push: first byte 0xE2, or a
halfword whose high byte is 0x04; 1,126 of 1,558 indexed callees match) is
also treated as a function start and a root. Task entries such as app_core
(app+0x066648) are passed as immediates and never called directly, so the
first pass, which missed them, merged them into "dead" runs. Accepting every
pointer-like value instead leaves only 77 KB dead, since stray data words
then keep large spans alive. Edges out of
my_app (app+0x088180) are cut unless --keep-my-app is given (for programs
launched from the stock main loop, like Brick Breaker); the call index
misses most of my_app's body, so that mode undercounts live code. The RAM-code image (app 0xC618C, 0xDA78
bytes, copied to RAM 0 at boot) is always kept. The result is a lower bound
on reclaimable flash; nothing here is proven dead.

    python3 -I reachability.py <stock app.bin> [--keep-my-app]
"""

from __future__ import annotations

import bisect
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
INDEX = ROOT.parent / "h9-firmware-research" / "evidence" / "stock" / "pi32-call-index-v2.json"
BASE = 0x01E00100
MY_APP = 0x088180
RAMCODE = (0xC618C, 0xC618C + 0xDA78)


def main() -> int:
    app = Path(sys.argv[1]).read_bytes()
    keep_my_app = "--keep-my-app" in sys.argv[2:]
    calls = {int(k, 16): [int(s, 16) for s in v] for k, v in json.loads(INDEX.read_text())["calls"].items()}
    # pointer-like values: any 2-aligned word naming an even address in app code
    pointers = set()
    for i in range(0, len(app) - 3, 2):
        v = struct.unpack_from("<I", app, i)[0] - BASE
        if 0 <= v < RAMCODE[0] and v % 2 == 0 and (v in calls or app[v] == 0xE2 or app[v + 1] == 0x04):
            pointers.add(v)
    starts = sorted(set(calls) | {0, MY_APP} | pointers)
    def owner(site: int) -> int:
        return starts[bisect.bisect_right(starts, site) - 1]
    # caller function -> callees
    edges: dict[int, set[int]] = {}
    for callee, sites in calls.items():
        for site in sites:
            edges.setdefault(owner(site), set()).add(callee)
    taken = pointers
    live, stack = set(), [0, *taken]
    while stack:
        f = stack.pop()
        if f in live:
            continue
        live.add(f)
        if f == MY_APP and not keep_my_app:
            continue  # replaced: do not follow the radio task
        stack.extend(edges.get(f, ()))
    # function extents: start to next start (code only, below the RAM-code image)
    def size(i: int) -> int:
        return (starts[i + 1] if i + 1 < len(starts) else RAMCODE[0]) - starts[i]
    dead = [(s, size(i)) for i, s in enumerate(starts) if s not in live and s < RAMCODE[0]]
    dead_bytes = sum(n for _, n in dead)
    # contiguous dead runs
    runs, cur = [], None
    for i, s in enumerate(starts):
        if s >= RAMCODE[0]:
            break
        if s not in live:
            cur = [s, s + size(i)] if cur is None or cur[1] != s else [cur[0], s + size(i)]
            if not runs or runs[-1][0] != cur[0]:
                runs.append(cur)
            else:
                runs[-1] = cur
        else:
            cur = None
    runs.sort(key=lambda r: r[0] - r[1])
    code_total = RAMCODE[0]
    report = {
        "mode": "keep my_app" if keep_my_app else "my_app replaced",
        "functions": len(starts), "address_taken": len(taken), "live": len(live), "dead": len(dead),
        "dead_bytes": dead_bytes, "flash_code_bytes_below_ramcode": code_total,
        "largest_dead_runs": [{"start": hex(a), "end": hex(b), "bytes": b - a} for a, b in runs[:10]],
        "my_app_reached_from_address_tables": MY_APP in taken,
    }
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
