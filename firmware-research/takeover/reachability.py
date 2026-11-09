#!/usr/bin/env python3
"""First-pass estimate of stock main-app code that is dead once stock my_app
is replaced (the hybrid takeover plan, see DESIGN.md).

Inputs: the stock app.bin and evidence/stock/pi32-call-index-v2.json
(callee -> call sites, direct calls only). Function starts are the callees;
a call site belongs to the nearest start at or below it. Live roots: the
reset entry and every function whose absolute address appears anywhere in
the image (function pointers, task and handler tables) - a conservative
stand-in for the 1,477 indirect jumps the index could not resolve. Edges out
of my_app (app+0x088180) are cut. The RAM-code image (app 0xC618C, 0xDA78
bytes, copied to RAM 0 at boot) is always kept. The result is a lower bound
on reclaimable flash; nothing here is proven dead.

    python3 -I reachability.py <stock app.bin>
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
    calls = {int(k, 16): [int(s, 16) for s in v] for k, v in json.loads(INDEX.read_text())["calls"].items()}
    starts = sorted(set(calls) | {0, MY_APP})
    def owner(site: int) -> int:
        return starts[bisect.bisect_right(starts, site) - 1]
    # caller function -> callees
    edges: dict[int, set[int]] = {}
    for callee, sites in calls.items():
        for site in sites:
            edges.setdefault(owner(site), set()).add(callee)
    # address-taken functions: any aligned 32-bit word equal to BASE + start
    start_set = set(starts)
    taken = set()
    for i in range(0, len(app) - 3, 2):
        v = struct.unpack_from("<I", app, i)[0] - BASE
        if v in start_set:
            taken.add(v)
    live, stack = set(), [0, *taken]
    while stack:
        f = stack.pop()
        if f in live:
            continue
        live.add(f)
        if f == MY_APP:
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
        "functions": len(starts), "address_taken": len(taken), "live": len(live), "dead": len(dead),
        "dead_bytes": dead_bytes, "flash_code_bytes_below_ramcode": code_total,
        "largest_dead_runs": [{"start": hex(a), "end": hex(b), "bytes": b - a} for a, b in runs[:10]],
        "my_app_reached_from_address_tables": MY_APP in taken,
    }
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
