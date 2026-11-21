#!/usr/bin/env python3
"""Publish the App Manager catalog: copy the built modules listed in
modules/catalog.json to dist/apps/catalog/ as .h9mod files (one bundle per
app, holding all of its modules, main first) and write dist/apps/catalog/catalog.json with their sizes, RAM,
CRC-16 and SHA-256. Run build.py first.

    python3 -I publish_catalog.py
    python3 -I publish_catalog.py --smartbeacon --bundle build/<dev-build>/h9_modules.bin
"""

from __future__ import annotations

import argparse
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


def smartbeacon_catalog(meta: dict, bundle_path: Path, data: bytes, built: dict) -> dict:
    manifest_path = bundle_path.with_name("h9_toolbox_modules-manifest.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    firmware = manifest["firmware"]
    resident = firmware["resident_beacon"] or {}
    if (firmware["loader_api"] != 5 or not resident.get("auto_tx")
            or manifest["bundle"]["sha256"] != sha256(data).hexdigest()
            or resident["screen_crc16"] != f"{build.crc16(built['APRS BEACON']['body']):04x}"):
        raise SystemExit("The SmartBeacon catalog needs the matching API 5 automatic beacon build.")

    menu = {
        **meta["menu"], "title": "Toolbox menu", "version": "2.0-dev",
        "description": "Grouped Toolbox menu with keypad shortcuts and APRS Settings.",
        "summary": "Hold Back to open Toolbox. Type a row number to jump to it.",
        "controls": ["Up and Down move or adjust. Menu opens or saves. Back returns one level."],
        "notes": ["Use the dev build of H9 Toolbox Firmware v0.1.5."],
        "replaces": ["ac00", *meta["menu"].get("replaces", [])],
    }
    if firmware.get("unified_menu"):
        menu.update(
            version="2.1-dev",
            description="Unified Menu-key interface with settings, channels and APRS.",
            summary="Press Menu to open Toolbox from the channel screen.",
            notes=["Use matching H9 Toolbox Firmware v0.1.6 preview."],
        )
        version = firmware["firmware_version"]
        if (build.firmware_version_word(version) & 0xffffff) >= 0x000107:
            menu.update(
                version="2.2",
                description="Unified Menu-key interface with settings, channels, APRS and Quick Change.",
                controls=[*menu["controls"], "Hold Back on the channel screen to open Quick Change."],
                notes=[f"Use matching H9 Toolbox Firmware v{version}."],
            )
    apps = []
    for classic in meta["apps"]:
        app = {key: value for key, value in classic.items() if key != "screenshots"}
        if app["id"] != "aprs":
            app["version"] += "-dev"
        if app["id"] == "aprs":
            app.update(
                description="Browse stations and map, send APRS messages, and keep a six-message inbox/outbox across power cycles. Beacon Status shows the resident scheduler.",
                summary="Stations, Map, persistent Messages, Send and Beacon Status.",
                modules=[name for name in app["modules"] if name != ".APRS BG"],
                retiredModules=[".APRS BG"],
                controls=[
                    "Open APRS → Stations and use the keypad tabs for List, Map, Messages and Send.",
                    "In Messages, Menu reads the highlighted item; Menu again opens a reply with its station filled in. A Send draft survives APRS screen changes until reboot.",
                    "SENT means a packet was handed to the radio. ACK or REJ appears only after that reply is heard from the recipient. The six most recent messages and their reply state survive power cycles when mailbox storage is available.",
                    "Use APRS Settings in Toolbox to arm the schedule or change the mode, interval, symbol and mute.",
                    "Beacon Status shows the configured APRS VFO and the number of packets submitted to the modem.",
                ],
                notes=["Automatic beacons need a fresh GPS fix and send from the normal channel screen.",
                       "Persistent messages need the matching firmware guard and unclaimed sectors at 0x3FA000–0x3FBFFF. The radio reports when storage is unavailable."],
            )
        apps.append(app)
    apps.append({
        "id": "about", "name": "ABOUT", "title": "About", "version": "1.0-dev",
        "category": "Setup", "description": "Show the Toolbox firmware and app loader version.",
    })
    return {"profile": "smartbeacon", "menu": menu, "apps": apps}


def publish(bundle_path: Path = BUNDLE, out: Path = OUT, *, smartbeacon: bool = False) -> dict:
    meta = json.loads(SOURCE.read_text(encoding="utf-8"))
    data = bundle_path.read_bytes()
    built = {m["name"]: m for m in build.parse_bundle(data)}
    if smartbeacon:
        meta = smartbeacon_catalog(meta, bundle_path, data, built)
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
    if smartbeacon:
        catalog["profile"] = meta["profile"]
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, default=BUNDLE)
    parser.add_argument("--smartbeacon", action="store_true", help="Publish the matching API 5 bench catalog")
    parser.add_argument("--output", type=Path, help="Override the catalog output directory")
    args = parser.parse_args()
    out = args.output or (OUT / "smartbeacon" if args.smartbeacon else OUT)
    c = publish(args.bundle, out, smartbeacon=args.smartbeacon)
    print(json.dumps({"menu": c["menu"]["file"], "apps": [a["file"] for a in c["apps"]]}, indent=1))
