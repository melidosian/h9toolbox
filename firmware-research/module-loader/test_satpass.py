#!/usr/bin/env python3
"""SAT PASS accuracy: orbit.c (integer model) against SGP4.

    python3 test_satpass.py <tle file> [--days-after-epoch N]

Needs the `sgp4` package (pip install sgp4; test only, never on the radio)
and a C compiler. Builds modules/satpass/orbit_cli.c, predicts the next
passes for several places, finds the same passes with SGP4 (TEME rotated by
the same IAU 1982 sidereal time, WGS84 observer), and reports the AOS, LOS
and maximum-elevation differences. Fails if a pass is missing or AOS / LOS
are off by more than the limit (default 60 s).
"""

from __future__ import annotations

import argparse
import math
import subprocess
import sys
import tempfile
from pathlib import Path

from sgp4.api import Satrec, jday

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import satpack  # noqa: E402

PLACES = [("Chicago", 41.878, -87.630), ("Sydney", -33.869, 151.209), ("Reykjavik", 64.147, -21.942),
          ("Quito", -0.180, -78.468), ("Tokyo", 35.676, 139.650)]
WGS84_A, WGS84_E2 = 6378.137, 6.69437999014e-3


def observer(lat, lon):
    la, lo = math.radians(lat), math.radians(lon)
    n = WGS84_A / math.sqrt(1 - WGS84_E2 * math.sin(la) ** 2)
    pos = (n * math.cos(la) * math.cos(lo), n * math.cos(la) * math.sin(lo), n * (1 - WGS84_E2) * math.sin(la))
    return pos, la, lo


def elevation(sat, obs, unix):
    pos, la, lo = obs
    jd, fr = jday(1970, 1, 1, 0, 0, 0)
    fr += unix / 86400.0
    err, r, _ = sat.sgp4(jd, fr)
    if err:
        return None, None
    th = satpack.gmst_turns(unix) * 2 * math.pi
    x = math.cos(th) * r[0] + math.sin(th) * r[1]
    y = -math.sin(th) * r[0] + math.cos(th) * r[1]
    dx, dy, dz = x - pos[0], y - pos[1], r[2] - pos[2]
    e = -math.sin(lo) * dx + math.cos(lo) * dy
    nn = -math.sin(la) * math.cos(lo) * dx - math.sin(la) * math.sin(lo) * dy + math.cos(la) * dz
    u = math.cos(la) * math.cos(lo) * dx + math.cos(la) * math.sin(lo) * dy + math.sin(la) * dz
    return math.degrees(math.atan2(u, math.hypot(e, nn))), math.degrees(math.atan2(e, nn)) % 360


def sgp4_passes(sat, obs, start, end):
    out, t, up = [], start, (elevation(sat, obs, start)[0] or -1) >= 0
    aos = start if up else None
    best = -90
    while t < end:
        t2 = t + 10
        el, _ = elevation(sat, obs, t2)
        if el >= 0 and not up:
            lo_, hi = t, t2
            for _ in range(8):
                mid = (lo_ + hi) / 2
                (lo_, hi) = (mid, hi) if elevation(sat, obs, mid)[0] < 0 else (lo_, mid)
            aos, best = hi, el
        if el >= 0:
            best = max(best, el)
        if el < 0 and up:
            lo_, hi = t, t2
            for _ in range(8):
                mid = (lo_ + hi) / 2
                (lo_, hi) = (mid, hi) if elevation(sat, obs, mid)[0] >= 0 else (lo_, mid)
            out.append((aos, hi, best))
        up, t = el >= 0, t2
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tle", type=Path)
    parser.add_argument("--days-after-epoch", type=float, default=0.5)
    parser.add_argument("--hours", type=int, default=36)
    parser.add_argument("--limit", type=float, default=60.0)
    args = parser.parse_args()
    tles = satpack.parse_tles(args.tle.read_text(errors="replace"))
    sats = {label: Satrec.twoline2rv(*tles[name]) for name, label in satpack.SATELLITES}
    with tempfile.TemporaryDirectory() as tmp:
        header = ROOT / "modules" / "satpass" / "sats.h"
        exe = Path(tmp) / "orbit_cli"
        subprocess.run(["cc", "-O2", "-o", str(exe), str(ROOT / "modules/satpass/orbit_cli.c"),
                        str(ROOT / "modules/satpass/orbit.c")], check=True)
        start = int(max(satpack.epoch_unix(l1) for l1, _ in tles.values() if True) * 0 +
                    max(satpack.epoch_unix(tles[n][0]) for n, _ in satpack.SATELLITES) + args.days_after_epoch * 86400)
        worst = {"aos": 0.0, "los": 0.0, "el": 0.0}
        missing = 0
        for place, lat, lon in PLACES:
            obs = observer(lat, lon)
            run = subprocess.run([str(exe), str(round(lat * 1e6)), str(round(lon * 1e6)), str(start),
                                  str(args.hours), "64"], capture_output=True, text=True, check=True)
            ours = [line.split() for line in run.stdout.split("\n") if line]
            ref = {label: sgp4_passes(sat, obs, start, start + args.hours * 3600) for label, sat in sats.items()}
            for label, aos, los, _tca, el, *_ in ours:
                aos, los, el = int(aos), int(los), int(el)
                if aos == start:
                    continue  # already up at the start: no rise to compare
                match = min(ref[label], key=lambda p: abs(p[0] - aos), default=None)
                if match is None or abs(match[0] - aos) > 600:
                    missing += 1
                    print(f"  {place}: {label} AOS {aos} has no SGP4 pass")
                    continue
                worst["aos"] = max(worst["aos"], abs(match[0] - aos))
                worst["los"] = max(worst["los"], abs(match[1] - los))
                worst["el"] = max(worst["el"], abs(match[2] - el))
            n_ref = sum(len([p for p in v if p[2] >= 0]) for v in ref.values())
            print(f"{place:10s} ours {len(ours):3d} passes, SGP4 {n_ref:3d}")
        print(f"worst over all places: AOS {worst['aos']:.0f} s, LOS {worst['los']:.0f} s, max elevation {worst['el']:.1f} deg")
        ok = missing == 0 and worst["aos"] <= args.limit and worst["los"] <= args.limit
        print("PASS" if ok else "FAIL")
        return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
