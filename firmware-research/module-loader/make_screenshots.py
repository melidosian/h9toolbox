#!/usr/bin/env python3
"""Screenshots for the App Manager's info cards, rendered from the apps' own drawing code.

Each app screen is compiled for the host with a fake LCD, clock and keys (the render_*.c harnesses next to the
sources), fed made-up data (stations around Chicago, NMEA from a sample fix, satellite passes from the bundled
TLE snapshot), and written as a 3x PNG to dist/apps/catalog/shots/. They are simulations of the screen, not
photographs of a radio.

    python3 -I make_screenshots.py          (needs cc; run build.py first for build/satdata.bin)
"""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parent
MODULES = ROOT / "modules"
OUT = ROOT.parents[1] / "dist" / "apps" / "catalog" / "shots"
SCALE = 3
CFLAGS = ["-std=c11", "-O1", "-Wno-incompatible-library-redeclaration", "-Wno-unused-function", "-Wno-macro-redefined",
          "-Wno-deprecated-declarations", "-Wno-int-to-pointer-cast", "-Wno-pointer-to-int-cast", "-I", str(MODULES / "common")]


def png(ppm: bytes, scale: int = SCALE) -> bytes:
    width, height = (int(x) for x in ppm.split(b"\n", 3)[1].split())
    pixels = ppm.split(b"\n", 3)[3]
    rows = bytearray()
    for y in range(height):
        line = b"".join(pixels[(y * width + x) * 3:(y * width + x) * 3 + 3] * scale for x in range(width))
        rows += (b"\x00" + line) * scale
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width * scale, height * scale, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))


def cc(tmp: Path, name: str, sources: list[Path], includes: list[Path], defines: list[str] = ()) -> Path:
    exe = tmp / name
    cmd = ["cc", *CFLAGS, *[f"-I{i}" for i in includes], *defines, "-o", str(exe), *map(str, sources)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"{name}: compile failed\n{r.stderr}")
    return exe


def run(exe: Path, *args: str, env: dict | None = None) -> None:
    r = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, env={**os.environ, **(env or {})})
    if r.returncode:
        raise SystemExit(f"{exe.name} {args}: exit {r.returncode}\n{r.stdout}{r.stderr}")


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    tmp = Path(tempfile.mkdtemp(prefix="h9shots-"))
    made = []

    def save(name: str, ppm_path: Path) -> None:
        (OUT / f"{name}.png").write_bytes(png(ppm_path.read_bytes()))
        made.append(name)

    try:
        gfx = MODULES / "common" / "gfx.c"
        # APRS screens
        aprs = MODULES / "aprs"
        for screen, name in (("list", "aprs-list"), ("info", "aprs-details"), ("map", "aprs-map"), ("msg", "aprs-messages")):
            exe = cc(tmp, f"render_aprs_{screen}", [aprs / "render_aprs.c", gfx], [aprs], [f'-DSCREEN="aprs_{screen}.c"'])
            run(exe, tmp / f"{name}.ppm")
            save(name, tmp / f"{name}.ppm")
        beacon_src = [aprs / "render_aprs.c", aprs / "beacon_core.c", aprs / "aprs_tx.c", aprs / "third_party" / "smartbeacon" / "smartbeacon.c",
                      MODULES / "common" / "fix_store.c", gfx]
        exe = cc(tmp, "render_aprs_beacon", beacon_src, [aprs], ['-DSCREEN="aprs_beacon.c"', "-Wno-missing-braces"])
        run(exe, tmp / "aprs-beacon.ppm")
        save("aprs-beacon", tmp / "aprs-beacon.ppm")
        # GPS pages
        gps = MODULES / "gps"
        exe = cc(tmp, "render_gps", [gps / "render_gps.c", gfx], [gps])
        for page, name in ((0, "gps-position"), (1, "gps-sky"), (2, "gps-signals")):
            run(exe, tmp / f"{name}.ppm", page)
            save(name, tmp / f"{name}.ppm")
        # Maze Chase: two moments of a random-walk game
        maze = MODULES / "maze"
        exe = cc(tmp, "render_maze", [maze / "render_maze.c", gfx], [maze])
        run(exe, tmp / "mz", 60, 2, 500)
        save("maze-chase-1", tmp / "mz.2.ppm")
        save("maze-chase-2", tmp / "mz.5.ppm")
        # SAT PASS: the pass list, the sky plot and the live track page, for passes over Chicago
        sat = MODULES / "satpass"
        satdata = ROOT / "build" / "satdata.bin"
        if not satdata.exists():
            raise SystemExit("run build.py first (build/satdata.bin)")
        passes = ["1:1794302584:1794302983", "0:1794304648:1794305119", "4:1794309009:1794309606"]
        env = {"SATDATA": str(satdata)}
        exe = cc(tmp, "render_satlist", [sat / "render_satlist.c", sat / "orbit.c"], [sat])
        run(exe, tmp / "satlist.ppm", 7, env=env)
        save("sat-list", tmp / "satlist.ppm")
        exe = cc(tmp, "render_plot", [sat / "render_plot.c", sat / "orbit.c"], [sat])
        run(exe, 41878000, -87630000, tmp / "plot.ppm", 7, "", *passes, env=env)
        save("sat-plot", tmp / "plot.ppm")
        exe = cc(tmp, "render_track", [sat / "render_track.c", sat / "orbit.c"], [sat])
        run(exe, 41878000, -87630000, tmp / "track.ppm", 7, 400, "4:1794309009:1794309606", "0:1794304648:1794305119", env=env)
        save("sat-track", tmp / "track.ppm")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print("wrote", ", ".join(f"{n}.png" for n in made), "to", OUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
