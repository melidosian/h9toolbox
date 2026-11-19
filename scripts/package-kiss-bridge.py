#!/usr/bin/env python3
"""Build the downloadable KISS bridge archive from the repository sources."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

root = Path(__file__).resolve().parents[1]
host = root / "firmware-research/kiss-tnc/host"
archive = root / "dist/firmware-update/h9-kiss-bridge.zip"
files = ("README.md", "bridge.py", "agwpe.py", "ax25.py", "ax25_link.py",
         "requirements.txt", "run_pat.py")
with ZipFile(archive, "w") as output:
    for name in files:
        entry = ZipInfo(f"H9-KISS-Bridge/{name}")
        entry.external_attr = 0o100644 << 16
        output.writestr(entry, (host / name).read_bytes(), compress_type=ZIP_DEFLATED)
