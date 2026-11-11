#!/usr/bin/env python3
"""Publish the App Manager catalog: copy the built modules listed in
modules/catalog.json to dist/apps/catalog/ as .h9mod files (one bundle per
app, holding all of its modules, main first) and write dist/apps/catalog/catalog.json with their sizes, RAM,
CRC-16 and SHA-256. Run build.py first.

    python3 -I publish_catalog.py
"""

from __future__ import annotations

from hashlib import sha256
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402

SOURCE = ROOT / "modules" / "catalog.json"
BUNDLE = ROOT / "build" / "h9_modules.bin"
OUT = ROOT.parents[1] / "dist" / "apps" / "catalog"


def publish(bundle_path: Path = BUNDLE, out: Path = OUT) -> dict:
    meta = json.loads(SOURCE.read_text(encoding="utf-8"))
    built = {m["name"]: m for m in build.parse_bundle(bundle_path.read_bytes())}
    out.mkdir(parents=True, exist_ok=True)

    def entry(item: dict) -> dict:
        names = item.get("modules", [item["name"]])
        if names[0] != item["name"]:
            raise SystemExit(f"{item['id']}: the main module must come first in `modules`")
        missing = [n for n in names if n not in built]
        if missing:
            raise SystemExit(f"{missing} in catalog.json but not in {bundle_path.name}; run build.py")
        mods = [built[n] for n in names]
        data = build.bundle(mods)
        for shot in item.get("screenshots", []):          # made by make_screenshots.py
            if not (out / shot["file"]).is_file():
                raise SystemExit(f"{item['id']}: screenshot {shot['file']} is missing; run make_screenshots.py")
        file = f"{item['id']}.h9mod"
        (out / file).write_bytes(data)
        parts = [{"name": m["name"], "bytes": len(m["body"]), "ram": len(m["body"]) + m["bss"],
                  "crc16": f"{build.crc16(m['body']):04x}"} for m in mods]
        return {**item, "modules": parts, "file": file, "bytes": sum(p["bytes"] for p in parts),
                "ram": max(p["ram"] for p in parts), "crc16": parts[0]["crc16"], "sha256": sha256(data).hexdigest()}

    catalog = {"format": 1, "menu": entry(meta["menu"]), "apps": [entry(a) for a in meta["apps"]]}
    ids = [catalog["menu"]["id"], *(a["id"] for a in catalog["apps"])]
    names = [p["name"] for a in catalog["apps"] for p in a["modules"]]
    if len(set(ids)) != len(ids) or len(set(names)) != len(names):
        raise SystemExit("catalog ids and module names must be unique")
    for stale in out.glob("*.h9mod"):
        if stale.name not in {f"{i}.h9mod" for i in ids}:
            stale.unlink()
    (out / "catalog.json").write_text(json.dumps(catalog, indent=1) + "\n", encoding="utf-8")
    # The satellites SAT PASS can track: the App Manager's settings list and the data it builds from them.
    (out / "satellites.json").write_bytes((ROOT / "modules" / "satpass" / "satellites.json").read_bytes())
    return catalog


if __name__ == "__main__":
    c = publish()
    print(json.dumps({"menu": c["menu"]["file"], "apps": [a["file"] for a in c["apps"]]}, indent=1))
