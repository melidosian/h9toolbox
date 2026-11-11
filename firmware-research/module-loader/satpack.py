#!/usr/bin/env python3
"""TLEs -> the satellite data module of SAT PASS (".SAT DATA").

    python3 -I satpack.py <tle file> [-o satdata.bin] [--only so50,ao91,...]

Takes the satellites of modules/satpass/satellites.json (all of them, or the ids
given to --only) from a TLE file, e.g. CelesTrak's amateur group:
https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle
and packs what the radio's integer orbit code needs (modules/satpass/satdata.h):
a 32-byte header and 64 bytes per satellite. The App Manager does the same in the
browser (dist/apps/satdata.mjs, tested against this file's output) so the data
can be refreshed without a compiler.

For each satellite it computes the SGP4 "un-Kozai" mean motion and semi-major
axis, the secular J2 rates of RAAN, argument of perigee and mean anomaly, and the
ndot/2 decay term, with every satellite moved to one shared epoch (the oldest
elements, to the second), plus a Greenwich sidereal time reference (IAU 1982,
UT1 = UTC). Elements age: refresh them every week or two.
"""

from __future__ import annotations

import argparse
import calendar
import json
import math
import struct
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
SATELLITES_JSON = HERE / "modules" / "satpass" / "satellites.json"
MAX_SATELLITES = 16
XKE = 60.0 / math.sqrt(6378.135 ** 3 / 398600.8)   # SGP4 (WGS72) constants
J2 = 1.082616e-3
EARTH_KM = 6378.135
TURN = 2 ** 32
MAGIC = 0x44544153   # "SATD"


def load_satellites() -> list[dict]:
    return json.loads(SATELLITES_JSON.read_text())["satellites"]


def parse_tles(text: str) -> dict[str, tuple[str, str]]:
    lines = [l.rstrip() for l in text.splitlines() if l.strip()]
    out = {}
    for i in range(len(lines) - 2):
        if lines[i + 1].startswith("1 ") and lines[i + 2].startswith("2 "):
            out[lines[i].strip()] = (lines[i + 1], lines[i + 2])
    return out


def epoch_unix(line1: str) -> float:
    yy = int(line1[18:20])
    year = 2000 + yy if yy < 57 else 1900 + yy
    day = float(line1[20:32])
    return calendar.timegm((year, 1, 1, 0, 0, 0)) + (day - 1) * 86400.0


def gmst_turns(unix: float) -> float:
    t = (unix / 86400.0 + 2440587.5 - 2451545.0) / 36525.0
    seconds = 67310.54841 + (876600 * 3600 + 8640184.812866) * t + 0.093104 * t * t - 6.2e-6 * t ** 3
    return (seconds % 86400.0) / 86400.0


def element(label: str, line1: str, line2: str, ref: int) -> dict:
    ndot2 = float(line1[33:43])                       # rev/day^2 (already /2)
    inc = math.radians(float(line2[8:16]))
    raan = math.radians(float(line2[17:25]))
    ecc = float("0." + line2[26:33].strip())
    argp = math.radians(float(line2[34:42]))
    m0 = math.radians(float(line2[43:51]))
    n_rev = float(line2[52:63])
    no = n_rev * 2 * math.pi / 1440.0                 # rad/min
    # SGP4 recovery of the original mean motion and semi-major axis.
    cosi = math.cos(inc)
    beta0 = math.sqrt(1 - ecc * ecc)
    k2 = 0.5 * J2
    a1 = (XKE / no) ** (2 / 3)
    d1 = 1.5 * k2 * (3 * cosi * cosi - 1) / (a1 * a1 * beta0 ** 3)
    a0 = a1 * (1 - d1 / 3 - d1 * d1 - 134 / 81 * d1 ** 3)
    d0 = 1.5 * k2 * (3 * cosi * cosi - 1) / (a0 * a0 * beta0 ** 3)
    n = no / (1 + d0)                                 # rad/min
    a = a0 / (1 - d0)                                 # Earth radii
    p = a * (1 - ecc * ecc)
    raan_dot = -1.5 * J2 * n * cosi / (p * p)         # rad/min
    argp_dot = 0.75 * J2 * n * (5 * cosi * cosi - 1) / (p * p)
    m_dot = n * (1 + 0.75 * J2 * beta0 * (3 * cosi * cosi - 1) / (p * p))
    per_min = TURN / (2 * math.pi) / 60.0             # rad/min -> binary angle/s
    epoch = epoch_unix(line1)
    shift = ref - epoch                               # seconds to the shared epoch
    m_ddot = ndot2 * TURN / 86400.0 ** 2              # binary angle/s^2
    m_rate = m_dot * per_min                          # at the TLE's epoch
    m_rate_ref = m_rate + 2 * m_ddot * shift          # at the shared epoch (the decay term has moved it)
    return dict(
        label=label,
        m0=round(m0 * TURN / (2 * math.pi) + m_rate * shift + m_ddot * shift * shift) % TURN,
        argp0=round((argp + argp_dot * shift / 60) * TURN / (2 * math.pi)) % TURN,
        raan0=round((raan + raan_dot * shift / 60) * TURN / (2 * math.pi)) % TURN,
        cos_i=round(cosi * 2 ** 30), sin_i=round(math.sin(inc) * 2 ** 30), ecc=round(ecc * 2 ** 30),
        a=round(a * EARTH_KM * 256),
        argp_dot=round(argp_dot * per_min * 65536), raan_dot=round(raan_dot * per_min * 65536),
        m_dot=round(m_rate_ref * 256), m_ddot=round(m_ddot * 2 ** 40),
        age_from=epoch,
    )


def pack(tles: dict[str, tuple[str, str]], satellites: list[dict]) -> tuple[bytes, dict]:
    """The data module body for `satellites` (dicts from satellites.json) and a summary."""
    if not 1 <= len(satellites) <= MAX_SATELLITES:
        raise ValueError(f"choose 1 to {MAX_SATELLITES} satellites")
    for sat in satellites:
        if sat["name"] not in tles:
            raise ValueError(f"{sat['name']} is not in the TLE data")
    ref = round(min(epoch_unix(tles[s["name"]][0]) for s in satellites))
    elements = [element(s["label"], *tles[s["name"]], ref) for s in satellites]
    for s, e in zip(satellites, elements):
        if not (abs(e["m_dot"]) < 2 ** 31 and abs(e["m_ddot"]) < 2 ** 31 and e["ecc"] < 0.02 * 2 ** 30):
            raise ValueError(f"{s['label']}: outside the fixed-point ranges (eccentricity must be < 0.02)")
    newest = max(e["age_from"] for e in elements)
    date = time.strftime("%Y-%m-%d", time.gmtime(newest))
    gmst = round(gmst_turns(ref) * TURN) % TURN
    body = struct.pack("<IHHIII12s", MAGIC, 1, len(elements), ref, gmst, round(newest), date.encode())
    for s, e in zip(satellites, elements):
        body += struct.pack("<8sIII" + "i" * 8, s["label"].encode().ljust(8, b"\0"), e["m0"], e["argp0"], e["raan0"],
                            e["cos_i"], e["sin_i"], e["ecc"], e["a"], e["argp_dot"], e["raan_dot"], e["m_dot"], e["m_ddot"])
        body += struct.pack("<IIHH", round(s["down"] * 1e5), round(s["up"] * 1e5), round(s["tone"] * 10), 0)
    return body, {"count": len(elements), "date": date, "newest": newest, "ref": ref, "gmst": gmst}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tle", type=Path)
    parser.add_argument("-o", "--output", type=Path, default=HERE / "build" / "satdata.bin")
    parser.add_argument("--only", help="comma-separated satellite ids (default: all)")
    args = parser.parse_args()
    satellites = load_satellites()
    if args.only:
        wanted = args.only.split(",")
        satellites = [s for s in satellites if s["id"] in wanted]
    body, info = pack(parse_tles(args.tle.read_text(errors="replace")), satellites)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(body)
    print(f"{args.output}: {info['count']} satellites, elements up to {info['date']}, {len(body)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
