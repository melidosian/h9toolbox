#!/usr/bin/env python3
"""Whole-image call graph of the stock app and the code only `my_app` needs.

For the Doom firmware (doom-firmware/README.md), stock `my_app` (the radio
UI and radio loop) never runs. Code reachable only through it can hold Doom.

Method:

- **Function starts:** the call index's callees, `my_app`, every 2-aligned
  word that points at an app-code function prologue (as reachability.py
  does), and every direct call target found. The last is iterated until no
  new targets appear.
- **Extents:** a function's extent runs from its start to the next start.
  Each extent is decoded linearly with the repo's Pi32v2 decoder and every
  direct `call` is collected.
- **Roots:** the reset entry and every pointer-like start. The radio-app run
  removes `my_app` from the roots and does not follow it.
- **Result:** "my_app only" = reachable with `my_app` minus reachable
  without it.

Conservative for reclaiming:

- Linear decoding may read data as calls, which adds edges and keeps more
  code live.
- Indirect calls through pointers are covered by the pointer roots. A
  callback stored only by `my_app` still counts as a root, so its code stays
  live.
- What remains uncertain is code reached only through jump tables or
  computed addresses outside these patterns. Trap tests on hardware are the
  proof.

    python3 -I callgraph.py <stock app.bin> [--json out.json] [--needed 0x01e4c2da,...]

--needed lists stock functions (CPU addresses) the replacement program
calls. Everything they reach is subtracted from the free set, because the
replacement still runs it.
"""

from __future__ import annotations

import bisect
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ISA = ROOT.parent / "brick-breaker" / "tools" / "pi32v2-isa"
INDEX = ROOT.parent / "h9-firmware-research" / "evidence" / "stock" / "pi32-call-index-v2.json"
BASE = 0x01E00100
MY_APP = 0x088180
RAMCODE = (0xC618C, 0xC618C + 0xDA78)
CALL = re.compile(r"^call (-?\d+)$")


def prologue(app: bytes, v: int) -> bool:
    return app[v] == 0xE2 or app[v + 1] == 0x04


def main() -> int:
    app = Path(sys.argv[1]).read_bytes()
    out_json = Path(sys.argv[sys.argv.index("--json") + 1]) if "--json" in sys.argv else None
    needed = ([int(a, 16) - BASE for a in sys.argv[sys.argv.index("--needed") + 1].split(",")]
              if "--needed" in sys.argv else [])
    sys.path.insert(0, str(ISA))
    from pi32v2isa import objdump, table  # noqa: E402
    decoder = objdump.decoder_for(table.load(), mcpu="r3")
    code_end = RAMCODE[0]

    index = {int(k, 16) for k in json.loads(INDEX.read_text())["calls"]}
    pointers = set()
    for i in range(0, len(app) - 3, 2):
        v = struct.unpack_from("<I", app, i)[0] - BASE
        if 0 <= v < code_end and v % 2 == 0 and (v in index or prologue(app, v)):
            pointers.add(v)
    starts = {s for s in index if s < code_end} | {0, MY_APP} | pointers | {n for n in needed if 0 <= n < code_end}

    def sweep(start: int, end: int) -> set[int]:
        found, pos = set(), start
        while pos < end:
            hit = decoder.decode(app, pos)
            if not hit:
                pos += 2
                continue
            _, _, length, text = hit
            m = CALL.match(text)
            if m:
                target = pos + length + int(m.group(1))
                if 0 <= target < code_end and target % 2 == 0:
                    found.add(target)
            pos += length
        return found

    calls: dict[int, set[int]] = {}
    while True:
        ordered = sorted(starts)
        for i, s in enumerate(ordered):
            end = ordered[i + 1] if i + 1 < len(ordered) else code_end
            if s not in calls or calls[s][1] != end:
                calls[s] = (end, sweep(s, end))
        new = {t for _, (e, ts) in calls.items() for t in ts} - starts
        if not new:
            break
        starts |= new
        print(f"  {len(new)} new call targets; {len(starts)} functions", file=sys.stderr, flush=True)
    ordered = sorted(starts)
    extent = {s: calls[s][0] - s for s in ordered}
    edges = {s: calls[s][1] for s in ordered}

    def reach(roots, skip=frozenset()):
        seen, stack = set(), list(roots)
        while stack:
            f = stack.pop()
            if f in seen or f in skip:
                continue
            seen.add(f)
            stack.extend(edges.get(f, ()))
        return seen

    roots = {0} | pointers
    with_app = reach(roots | {MY_APP})
    without_app = reach(roots - {MY_APP}, skip={MY_APP})
    kept = reach({n for n in needed if n in edges})
    only_app = with_app - without_app - kept
    dead = set(ordered) - with_app
    size = lambda fs: sum(extent[f] for f in fs)

    def runs(fs):
        out, cur = [], None
        for s in ordered:
            if s in fs:
                if cur and cur[1] == s:
                    cur[1] = s + extent[s]
                else:
                    cur = [s, s + extent[s]]
                    out.append(cur)
            else:
                cur = None
        return sorted(out, key=lambda r: r[0] - r[1])

    # Merge my_app-only and unreached: both are free in the Doom firmware.
    free = only_app | dead
    report = {
        "functions": len(ordered), "pointer_roots": len(pointers), "direct_call_edges": sum(map(len, edges.values())),
        "my_app_extent": extent[MY_APP],
        "reachable_with_my_app_bytes": size(with_app),
        "needed_by_replacement": {"functions": len(kept), "bytes": size(kept),
                                  "inside_my_app_only": sorted(hex(f) for f in kept & (with_app - without_app))},
        "my_app_only": {"functions": len(only_app), "bytes": size(only_app)},
        "unreached": {"functions": len(dead), "bytes": size(dead)},
        "free_in_doom_firmware_bytes": size(free),
        "largest_free_runs": [{"start": hex(a), "end": hex(b), "bytes": b - a} for a, b in runs(free)[:12]],
        "largest_my_app_only_runs": [{"start": hex(a), "end": hex(b), "bytes": b - a} for a, b in runs(only_app)[:8]],
    }
    print(json.dumps(report, indent=2))
    if out_json:
        out_json.write_text(json.dumps({**report, "my_app_only_functions": [hex(f) for f in sorted(only_app)],
                                        "unreached_functions": [hex(f) for f in sorted(dead)]}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
