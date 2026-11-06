#!/usr/bin/env python3
"""Download the public Brick Breaker source package and launch its guided CLI."""

from __future__ import annotations

from io import BytesIO
import json
from pathlib import Path
import subprocess
import sys
from urllib.request import Request, urlopen
import venv
from zipfile import ZipFile


OWNER = "melidosian"
REPO = "h9toolbox"
PREFIX = "firmware-research/brick-breaker/"
DEST = Path.home() / "TD-H9-Brick-Breaker"


def fetch(url: str) -> bytes:
    request = Request(url, headers={"User-Agent": "h9-brick-breaker-bootstrap/1"})
    with urlopen(request, timeout=90) as response:
        return response.read()


def main() -> None:
    if sys.version_info < (3, 11):
        raise RuntimeError("Python 3.11 or newer is required")
    restoring = len(sys.argv) > 1 and sys.argv[1] == "restore"
    if restoring:
        if "--backup" in sys.argv:
            backup = Path(sys.argv[sys.argv.index("--backup") + 1]).expanduser().resolve()
            destination = backup.parent.parent
        else:
            backups = sorted(DEST.glob("*/backups/h9-before-brick-breaker-*.bin"),
                             key=lambda path: path.stat().st_mtime)
            if not backups:
                raise RuntimeError("No local Brick Breaker backup found; pass --backup PATH")
            destination = backups[-1].parent.parent
        source = destination / "cli.py"
        if not source.exists():
            raise RuntimeError(f"Original Brick Breaker source is missing: {source}")
    else:
        commit = json.loads(fetch(f"https://api.github.com/repos/{OWNER}/{REPO}/commits/main"))["sha"]
        if len(commit) != 40 or any(c not in "0123456789abcdef" for c in commit):
            raise RuntimeError("GitHub returned an invalid commit ID")
        destination = DEST / commit[:12]
        source = destination / "cli.py"
    if not source.exists():
        print(f"Downloading {OWNER}/{REPO} at {commit}...", flush=True)
        archive = ZipFile(BytesIO(fetch(f"https://codeload.github.com/{OWNER}/{REPO}/zip/{commit}")))
        names = archive.namelist()
        root = names[0].split("/", 1)[0] + "/"
        selected = [(name, name[len(root + PREFIX):]) for name in names
                    if name.startswith(root + PREFIX) and not name.endswith("/")]
        if not selected or not any(relative == "cli.py" for _, relative in selected):
            raise RuntimeError("Brick Breaker package is missing from the GitHub archive")
        destination.mkdir(parents=True, exist_ok=True)
        for name, relative in selected:
            path = Path(relative)
            if path.is_absolute() or ".." in path.parts:
                raise RuntimeError("Unsafe archive member")
            output = destination / path
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(archive.read(name))
    environment = destination / ".venv"
    if not environment.exists():
        venv.create(environment, with_pip=True)
    python = environment / ("Scripts/python.exe" if sys.platform == "win32" else "bin/python")
    if len(sys.argv) == 1 or sys.argv[1] != "build":
        result = subprocess.run([str(python), "-c", "import hid"], capture_output=True)
        if result.returncode:
            print("Installing pinned HID recovery dependency...", flush=True)
            subprocess.run([str(python), "-m", "pip", "install", "hidapi==0.15.0"], check=True)
    print(f"Source, tools and local backups: {destination}", flush=True)
    subprocess.run([str(python), str(source), *sys.argv[1:]], check=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Stopped: {error}", file=sys.stderr)
        raise SystemExit(1)
