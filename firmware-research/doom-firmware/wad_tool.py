#!/usr/bin/env python3
"""Measure a Doom IWAD and what a trimmed copy for the TD-H9 would weigh.

    wad_tool.py stats <iwad>            size by category
    wad_tool.py map <iwad> E1M1 [...]   bytes needed for these maps

The per-map figure counts the lumps the engine needs to start and play
those maps: their level lumps; the wall textures they use (plus switch and
animation partners and the sky); the patches those textures are built
from; the flats they use (plus animation ranges); the sprites the map's
things can show (monsters with their projectiles and drops, pickups with the
weapon sprites they give, the player, fist, pistol and hit effects); and the
in-game graphics (status bar, menus, fonts, palette, colormap). Title, help,
intermission and ending pictures, sound and music are left out, so the port
must start straight into a map and skip the demo loop and intermission.
Lump directory reads only; nothing is written.
"""

from __future__ import annotations

from collections import Counter
from pathlib import Path
import re
import struct
import sys

MAP_LUMPS = ("THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS", "SSECTORS",
             "NODES", "SECTORS", "REJECT", "BLOCKMAP")
# Always-needed non-level graphics and tables (prefixes), sound/music excluded.
CORE_PREFIXES = ("PLAYPAL", "COLORMAP", "ST", "M_", "WI", "AMMNUM", "BRDR_", "END", "PFUB",
                 "TITLEPIC", "INTERPIC", "HELP", "CREDIT", "VICTORY", "BOSSBACK", "DMXGUS", "GENMIDI")
SOUND_PREFIXES = ("DS", "DP", "D_")
# Vanilla episode-1 switch pairs and animation ranges (p_switch.c, p_spec.c).
SWITCHES = [("SW1BRCOM", "SW2BRCOM"), ("SW1BRN1", "SW2BRN1"), ("SW1BRN2", "SW2BRN2"),
            ("SW1BRNGN", "SW2BRNGN"), ("SW1BROWN", "SW2BROWN"), ("SW1COMM", "SW2COMM"),
            ("SW1COMP", "SW2COMP"), ("SW1DIRT", "SW2DIRT"), ("SW1EXIT", "SW2EXIT"),
            ("SW1GRAY", "SW2GRAY"), ("SW1GRAY1", "SW2GRAY1"), ("SW1METAL", "SW2METAL"),
            ("SW1PIPE", "SW2PIPE"), ("SW1SLAD", "SW2SLAD"), ("SW1STARG", "SW2STARG"),
            ("SW1STON1", "SW2STON1"), ("SW1STON2", "SW2STON2"), ("SW1STONE", "SW2STONE"),
            ("SW1STRTN", "SW2STRTN")]
ANIM_TEXTURES = [("BLODGR1", "BLODGR4"), ("SLADRIP1", "SLADRIP3"), ("BLODRIP1", "BLODRIP4"),
                 ("FIREWALA", "FIREWALL"), ("GSTFONT1", "GSTFONT3"), ("FIRELAVA", "FIRELAV3"),
                 ("FIREMAG1", "FIREMAG3"), ("FIREBLU1", "FIREBLU2"), ("ROCKRED1", "ROCKRED3"),
                 ("BFALL1", "BFALL4"), ("SFALL1", "SFALL4"), ("WFALL1", "WFALL4"), ("DBRAIN1", "DBRAIN4")]
ANIM_FLATS = [("NUKAGE1", "NUKAGE3"), ("FWATER1", "FWATER4"), ("SWATER1", "SWATER4"),
              ("LAVA1", "LAVA4"), ("BLOOD1", "BLOOD3"), ("RROCK05", "RROCK08"),
              ("SLIME01", "SLIME04"), ("SLIME05", "SLIME08"), ("SLIME09", "SLIME12")]


# Doom 1 thing types -> sprite prefixes they can show (including projectiles,
# drops, and the HUD weapon sprites a pickup gives the player).
THING_SPRITES = {
    1: "PLAY", 2: "PLAY", 3: "PLAY", 4: "PLAY", 11: "PLAY", 14: "",  # 14 teleport target
    3004: "POSS CLIP", 9: "SPOS SHOT SHTG SHTF", 3001: "TROO BAL1", 3002: "SARG", 58: "SARG",
    3006: "SKUL", 3005: "HEAD BAL2", 3003: "BOSS BAL7", 16: "CYBR MISL", 7: "SPID",
    2001: "SHOT SHTG SHTF", 2002: "MGUN CHGG CHGF", 2003: "LAUN MISG MISF MISL",
    2004: "PLAS PLSG PLSF PLSS PLSE", 2005: "CSAW SAWG", 2006: "BFUG BFGG BFGF BFS1 BFE1 BFE2",
    2007: "CLIP", 2048: "AMMO", 2008: "SHEL", 2049: "SBOX", 2010: "ROCK", 2046: "BROK",
    2047: "CELL", 17: "CELP", 8: "BPAK",
    2011: "STIM", 2012: "MEDI", 2014: "BON1", 2015: "BON2", 2018: "ARM1", 2019: "ARM2",
    2013: "SOUL", 2022: "PINV", 2023: "PSTR", 2024: "PINS", 2025: "SUIT", 2026: "PMAP", 2045: "PVIS",
    5: "BKEY", 6: "YKEY", 13: "RKEY", 38: "RSKU", 39: "YSKU", 40: "BSKU",
    2028: "COLU", 30: "COL1", 31: "COL2", 32: "COL3", 33: "COL4", 36: "COL5", 37: "COL6",
    41: "CEYE", 42: "FSKU", 43: "TRE1", 44: "TBLU", 45: "TGRN", 46: "TRED", 47: "SMIT",
    48: "ELEC", 54: "TRE2", 55: "SMBT", 56: "SMGT", 57: "SMRT", 34: "CAND", 35: "CBRA",
    70: "FCAN", 2035: "BAR1 BEXP",
    10: "PLAY", 12: "PLAY", 15: "PLAY", 18: "POSS", 19: "SPOS", 20: "TROO", 21: "SARG",
    22: "HEAD", 23: "SKUL", 24: "POL5", 25: "POL1", 26: "POL6", 27: "POL4", 28: "POL2",
    29: "POL3", 49: "GOR1", 50: "GOR2", 51: "GOR3", 52: "GOR4", 53: "GOR5", 59: "GOR2",
    60: "GOR4", 61: "GOR3", 62: "GOR5", 63: "GOR1", 79: "POB1", 80: "POB2", 81: "BRS1",
}
# Always present: the player, fist and pistol, and hit/teleport effects.
ALWAYS_SPRITES = "PLAY PUNG PISG PISF PUFF BLUD TFOG IFOG"
# In-game graphics only (no title, help, credits, intermission or endings).
LEAN_PREFIXES = ("PLAYPAL", "COLORMAP", "ST", "M_", "AMMNUM", "BRDR_")


def read_wad(path: Path):
    data = path.read_bytes()
    kind, count, offset = struct.unpack_from("<4sII", data, 0)
    if kind not in (b"IWAD", b"PWAD"):
        raise ValueError("Not a WAD")
    lumps = []
    for i in range(count):
        pos, size, name = struct.unpack_from("<II8s", data, offset + 16 * i)
        lumps.append((name.rstrip(b"\0").decode("ascii", "replace").upper(), pos, size))
    return data, lumps


def between(lumps, start, end):
    names = [n for n, _, _ in lumps]
    out = []
    for s in [i for i, n in enumerate(names) if n == start]:
        e = names.index(end, s)
        out += [l for l in lumps[s + 1:e] if l[2]]
    return out


def textures(data, lumps):
    """Composite texture name -> list of patch names (TEXTURE1/2 via PNAMES)."""
    by = {n: (p, s) for n, p, s in lumps}
    p, _ = by["PNAMES"]
    (n,) = struct.unpack_from("<I", data, p)
    pnames = [data[p + 4 + 8 * i:p + 12 + 8 * i].rstrip(b"\0").decode().upper() for i in range(n)]
    out = {}
    for t in ("TEXTURE1", "TEXTURE2"):
        if t not in by:
            continue
        base, _ = by[t]
        (count,) = struct.unpack_from("<I", data, base)
        for i in range(count):
            (o,) = struct.unpack_from("<I", data, base + 4 + 4 * i)
            name = data[base + o:base + o + 8].rstrip(b"\0").decode().upper()
            (patches,) = struct.unpack_from("<H", data, base + o + 20)
            refs = [pnames[struct.unpack_from("<hhH", data, base + o + 22 + 10 * k)[2]] for k in range(patches)]
            out[name] = refs
    return out


def expand(names, ranges, ordered):
    """Add whole animation ranges touched by any used name."""
    names = set(names)
    for a, b in ranges:
        if a in ordered and b in ordered:
            seq = ordered[ordered.index(a):ordered.index(b) + 1]
            if names & set(seq):
                names |= set(seq)
    return names


def map_needs(data, lumps, maps):
    by = {n: (p, s) for n, p, s in lumps}
    names = [n for n, _, _ in lumps]
    tex = textures(data, lumps)
    tex_order = list(tex)
    flat_lumps = between(lumps, "F_START", "F_END") or between(lumps, "FF_START", "FF_END")
    flat_order = [n for n, _, _ in flat_lumps]
    used_tex, used_flats, level = {"SKY1"}, set(), 0
    for m in maps:
        i = names.index(m)
        for j, part in enumerate(MAP_LUMPS, 1):
            if lumps[i + j][0] != part:
                raise ValueError(f"{m}: unexpected lump order")
            level += lumps[i + j][2]
        _, p, s = lumps[i + 3]  # SIDEDEFS, 30 bytes each
        for k in range(s // 30):
            for f in (4, 12, 20):
                used_tex.add(data[p + 30 * k + f:p + 30 * k + f + 8].rstrip(b"\0").decode().upper())
        _, p, s = lumps[i + 8]  # SECTORS, 26 bytes each
        for k in range(s // 26):
            for f in (4, 12):
                used_flats.add(data[p + 26 * k + f:p + 26 * k + f + 8].rstrip(b"\0").decode().upper())
    used_tex.discard("-")
    for a, b in SWITCHES:
        if a in used_tex or b in used_tex:
            used_tex |= {a, b}
    used_tex = expand(used_tex, ANIM_TEXTURES, tex_order) & set(tex)
    used_flats = expand(used_flats, ANIM_FLATS, flat_order) | {"F_SKY1"}
    patches = {p for t in used_tex for p in tex[t]}
    wanted = set(ALWAYS_SPRITES.split())
    unknown = set()
    for m in maps:
        _, p, s = lumps[names.index(m) + 1]  # THINGS, 10 bytes each
        for k in range(s // 10):
            kind = struct.unpack_from("<h", data, p + 10 * k + 6)[0]
            if kind in THING_SPRITES:
                wanted |= set(THING_SPRITES[kind].split())
            else:
                unknown.add(kind)
    size = lambda n: by[n][1] if n in by else 0
    sprite_lumps = between(lumps, "S_START", "S_END")
    sprites = sum(s for n, _, s in sprite_lumps if n[:4] in wanted)
    core = sum(s for n, _, s in lumps if n.startswith(LEAN_PREFIXES) and not n.startswith(SOUND_PREFIXES))
    tables = size("PNAMES") + size("TEXTURE1") + size("TEXTURE2")
    parts = {
        "level lumps": level,
        f"wall patches ({len(patches)} for {len(used_tex)} textures)": sum(size(p) for p in patches),
        f"flats ({len(used_flats & set(flat_order))})": sum(size(f) for f in used_flats),
        f"sprites ({len(wanted & {n[:4] for n, _, _ in sprite_lumps})} groups for the map's things)": sprites,
        "status bar, menus, fonts, palette, colormap": core,
        "PNAMES/TEXTURE1 (full; trimmed would be smaller)": tables,
    }
    if unknown:
        print(f"note: thing types without a sprite mapping (not counted): {sorted(unknown)}")
    return parts


def stats(lumps):
    c = Counter()
    names = [n for n, _, _ in lumps]
    inside = {}
    for start, end, label in (("S_START", "S_END", "sprites"), ("P_START", "P_END", "wall patches"),
                              ("F_START", "F_END", "flats")):
        for l in between(lumps, start, end):
            inside[l] = label
    for i, l in enumerate(lumps):
        n, _, s = l
        if l in inside:
            c[inside[l]] += s
        elif re.match(r"^E\dM\d$", n) or (i and any(re.match(r"^E\dM\d$", names[k]) for k in range(max(0, i - 10), i))
                                          and n in MAP_LUMPS):
            c["levels (36 maps)"] += s
        elif n.startswith(SOUND_PREFIXES) or n in ("GENMIDI", "DMXGUS"):
            c["sound and music"] += s
        else:
            c["other graphics and tables"] += s
    return c


def main() -> int:
    if len(sys.argv) < 3 or sys.argv[1] not in ("stats", "map"):
        print(__doc__)
        return 2
    data, lumps = read_wad(Path(sys.argv[2]))
    if sys.argv[1] == "stats":
        c = stats(lumps)
        for k, v in c.most_common():
            print(f"{k:32s} {v:>12,}")
        print(f"{'total':32s} {len(data):>12,}  ({len(lumps)} lumps)")
    else:
        parts = map_needs(data, lumps, [m.upper() for m in sys.argv[3:]])
        for k, v in parts.items():
            print(f"{k:58s} {v:>10,}")
        print(f"{'total for ' + ', '.join(sys.argv[3:]):58s} {sum(parts.values()):>10,}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
