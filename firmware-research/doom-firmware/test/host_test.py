#!/usr/bin/env python3
"""Run the TD-H9 Doom firmware's source set natively (test/td_host.c stands in
for the radio) against a chip image holding the uploaded pack, and save the
panel as images: status screen, tic 35, tic 70, final.

    python3 -I host_test.py --dmc <DMC100-Doom checkout> --pack <DOOM.DAT> [--tics N]

Needs Docker image h9-jieli-dev (gcc) and node (for the directory sector).
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402  (sources(), INCLUDES, WRAPS)

OUT = ROOT / "build" / "host"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dmc", type=Path, required=True)
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--tics", type=int, default=140)
    parser.add_argument("--asan", action="store_true", help="AddressSanitizer build (stack traces on faults)")
    args = parser.parse_args()
    dmc, pack = args.dmc.resolve(), args.pack.read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)

    directory = OUT / "directory.bin"
    subprocess.run(["node", str(ROOT / "test" / "make_directory.mjs"), str(len(pack)), str(directory)], check=True)
    chip = bytearray(b"\xff" * 0x1000000)
    chip[0xA1F000:0xA20000] = directory.read_bytes()
    chip[0xA20000:0xA20000 + len(pack)] = pack
    (OUT / "chip.bin").write_bytes(chip)

    mount = Path(os.path.commonpath([dmc, ROOT]))
    rel = lambda p: f"/w/{Path(p).resolve().relative_to(mount)}"
    # glibc replaces DMC100's libc stubs on the host (they override malloc/free under glibc's stdio)
    sources = [rel(s) for s in build.sources(dmc) if s.name != "stub_libc.c"]
    includes = " ".join(f"-I{rel(dmc / i)}" for i in build.INCLUDES) + f" -I{rel(ROOT / 'port')}"
    wraps = ",".join(f"--wrap={w}" for w in build.WRAPS)
    san = "-fsanitize=address -fno-omit-frame-pointer " if args.asan else ""
    # The harness uses the system headers (the engine's stub headers would hide fopen).
    harness = (f"gcc -O1 -g -w -std=gnu11 -DTD_HOST {san}-I{rel(ROOT / 'port')} -c "
               f"{rel(ROOT / 'test' / 'td_host.c')} -o {rel(OUT / 'td_host_main.o')} && ")
    script = (harness + f"gcc -O1 -g -w -fwrapv -fsigned-char -std=gnu11 -DTD_HOST -no-pie {san}{includes} "
              f"-Wl,{wraps} -o {rel(OUT / 'td_host')} {' '.join(sources)} {rel(OUT / 'td_host_main.o')} && "
              f"{rel(OUT / 'td_host')} {rel(OUT / 'chip.bin')} {rel(OUT)} {args.tics}")
    r = subprocess.run(["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{mount}:/w", "-w", "/w",
                        "h9-jieli-dev", "sh", "-c", script], capture_output=True, text=True)
    print(r.stdout[-3000:], r.stderr[-3000:])
    for ppm in sorted(OUT.glob("*.ppm")):
        subprocess.run(["sips", "-s", "format", "png", str(ppm), "--out", str(ppm.with_suffix(".png"))],
                       capture_output=True)
    return r.returncode


if __name__ == "__main__":
    raise SystemExit(main())
