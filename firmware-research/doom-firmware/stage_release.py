#!/usr/bin/env python3
"""Stage a Doom release folder for the /doom/ installer's "Download from GitHub".

    python3 -I stage_release.py --pack <DOOM.DAT> --music <E1M1.h9mu> [--version 32] [-o build/doom-release]

Copies the current build (build/h9_doom_OFFLINE_EXPERIMENT.fw and
build/h9_doom_far_0xEA000.bin), the game pack and the song into one folder
and writes doom-manifest.json with each file's size and SHA-256 (and the
firmware image's SHA-256). Publishing that folder (for example as the
`doom-files` branch read through raw.githubusercontent.com) is a separate,
deliberate step; this tool only stages it.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "h9-flasher"))


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--music", type=Path)
    parser.add_argument("--version", default="dev")
    parser.add_argument("-o", "--output", type=Path, default=ROOT / "build" / "doom-release")
    args = parser.parse_args()

    from h9_flasher import parse_firmware  # the flasher's own .fw parser and checks

    build = ROOT / "build"
    out = args.output
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    entries = {}

    def stage(key, source: Path, name: str, label: str, **extra):
        data = source.read_bytes()
        (out / name).write_bytes(data)
        entries[key] = {"file": name, "name": label, "bytes": len(data), "sha256": sha(data), **extra}

    fw = build / "h9_doom_OFFLINE_EXPERIMENT.fw"
    image = parse_firmware(fw).image
    stage("firmware", fw, f"h9-doom-{args.version}.fw", "Doom firmware", imageSha256=sha(image))
    stage("code", build / "h9_doom_far_0xEA000.bin", f"h9-doom-{args.version}-code-0xEA000.bin", "Doom code")
    if args.pack.read_bytes()[:8] != b"DMCDOOM\0":
        raise SystemExit(f"{args.pack} is not a DMC100 pack")
    stage("pack", args.pack, "freedoom-e1m1-pack.dat", "Freedoom E1M1")
    if args.music:
        if args.music.read_bytes()[:4] != b"H9MU":
            raise SystemExit(f"{args.music} is not an H9MU song")
        stage("music", args.music, "freedoom-e1m1.h9mu", "E1M1 music")
    manifest = {
        "kind": "td-h9-doom-release",
        "version": args.version,
        "firmware": entries["firmware"],
        "code": entries["code"],
        "pack": entries["pack"],
        "music": [entries["music"]] if "music" in entries else [],
        "licenses": {
            "engine": "DMC100-Doom (GPL-2.0-or-later) + TD-H9 port (GPL-2.0-or-later)",
            "data": "Freedoom (BSD-3-Clause)",
            "firmware_base": "TIDRADIO TD-H9 v1.0.38 with H9 Toolbox changes, as on h9toolbox.com",
        },
    }
    (out / "doom-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
