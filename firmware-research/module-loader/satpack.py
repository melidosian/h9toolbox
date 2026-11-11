#!/usr/bin/env python3
"""TLEs -> modules/satpass/sats.h, the satellite table embedded in SAT PASS.

    python3 -I satpack.py <tle file> [-o modules/satpass/sats.h]

Takes the FM repeater satellites listed in SATELLITES (by their CelesTrak
name) from a TLE file, e.g. CelesTrak's amateur group:
https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle

For each it computes what orbit.c needs in fixed point: the SGP4 "un-Kozai"
mean motion and semi-major axis, the secular J2 rates of RAAN, argument of
perigee and mean anomaly, and the ndot/2 decay term, with every satellite moved
to one shared epoch (the oldest elements, to the second). It also writes a Greenwich sidereal time reference (IAU 1982,
UT1 = UTC). Elements age: rebuild with fresh TLEs every week or two.
"""

from __future__ import annotations

import argparse
import calendar
import math
import time
from pathlib import Path

# (CelesTrak name, label on the radio, at most 7 characters)
SATELLITES = [
    ("SAUDISAT 1C (SO-50)", "SO-50"),
    ("ISS (ZARYA)", "ISS"),
    ("RADFXSAT (FOX-1B)", "AO-91"),
    ("EYESAT A (AO-27)", "AO-27"),
    ("DIWATA-2B", "PO-101"),
    ("ASRTU-1 (AO-123)", "AO-123"),
    ("LILACSAT-2", "LILAC2"),
]

XKE = 60.0 / math.sqrt(6378.135 ** 3 / 398600.8)   # SGP4 (WGS72) constants
J2 = 1.082616e-3
EARTH_KM = 6378.135
TURN = 2 ** 32


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
    m_rate = m_dot * per_min + 2 * m_ddot * shift     # the rate at the shared epoch
    return dict(
        label=label,
        m0=round(m0 * TURN / (2 * math.pi) + m_dot * per_min * shift + m_ddot * shift * shift) % TURN,
        argp0=round((argp + argp_dot * shift / 60) * TURN / (2 * math.pi)) % TURN,
        raan0=round((raan + raan_dot * shift / 60) * TURN / (2 * math.pi)) % TURN,
        cos_i=round(cosi * 2 ** 30), sin_i=round(math.sin(inc) * 2 ** 30), ecc=round(ecc * 2 ** 30),
        a=round(a * EARTH_KM * 256),
        argp_dot=round(argp_dot * per_min * 65536), raan_dot=round(raan_dot * per_min * 65536),
        m_dot=round(m_rate * 256), m_ddot=round(m_ddot * 2 ** 40),
        age_from=epoch,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("tle", type=Path)
    parser.add_argument("-o", "--output", type=Path, default=Path(__file__).parent / "modules" / "satpass" / "sats.h")
    args = parser.parse_args()
    tles = parse_tles(args.tle.read_text(errors="replace"))
    for name, _ in SATELLITES:
        if name not in tles:
            raise SystemExit(f"{name} is not in {args.tle}")
    # One shared epoch (the oldest elements, to the second): each satellite's
    # angles are moved to it, so the table needs no per-satellite epoch.
    ref = round(min(epoch_unix(tles[name][0]) for name, _ in SATELLITES))
    sats = [element(label, *tles[name], ref) for name, label in SATELLITES]
    gmst = round(gmst_turns(ref) * TURN) % TURN
    newest = max(s["age_from"] for s in sats)
    date = time.strftime("%Y-%m-%d", time.gmtime(newest))
    rows = []
    for s in sats:
        if not (abs(s["m_dot"]) < 2 ** 31 and abs(s["m_ddot"]) < 2 ** 31 and s["ecc"] < 0.02 * 2 ** 30):
            raise SystemExit(f'{s["label"]}: outside the fixed-point ranges (eccentricity must be < 0.02)')
        rows.append(f'    {{"{s["label"]}", {s["m0"]}u, {s["argp0"]}u, {s["raan0"]}u, '
                    f'{s["cos_i"]}, {s["sin_i"]}, {s["ecc"]}, {s["a"]}, '
                    f'{s["argp_dot"]}, {s["raan_dot"]}, {s["m_dot"]}, {s["m_ddot"]}}},')
    args.output.write_text(
        "/* Generated by satpack.py from " + args.tle.name + ": do not edit.\n"
        " * FM repeater satellites for SAT PASS; elements from " + date + ". */\n"
        "#ifndef SATPASS_SATS_H\n#define SATPASS_SATS_H\n\n#include \"orbit.h\"\n\n"
        f'#define SATS_DATE "{date}"\n#define SATS_NEWEST {round(newest)}u\n\n'
        "static const struct sat SATS[] = {\n" + "\n".join(rows) + "\n};\n"
        f"#define SATS_COUNT {len(sats)}u\n\n"
        f"static const struct gmst_ref GMST_REF = {{{ref}u, {gmst}u}};\n\n#endif\n")
    print(f"{args.output}: {len(sats)} satellites, elements up to {date}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
