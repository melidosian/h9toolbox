#!/usr/bin/env python3
"""Link the resident scheduler and settings engine into one far sector, offline."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import build  # noqa: E402

BACKUP_START = 0xEA000
SECTOR = 0x1000
FLASH_START = 0xFB000
CPU_START = 0x01EF6000


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--far-backup", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    args = parser.parse_args()
    backup = args.far_backup.read_bytes()
    offset = FLASH_START - BACKUP_START
    if len(backup) != offset + SECTOR or backup[offset:] != b"\xff" * SECTOR:
        raise SystemExit("Expected a complete 72 KiB far backup with erased sector 0xFB000")

    aprs = build.ROOT / "modules" / "aprs"
    common = build.ROOT / "modules" / "common"
    out = build.ROOT / "build" / "resident-probe"
    out.mkdir(parents=True, exist_ok=True)
    sources = (HERE / "dispatch.c", HERE / "beacon_service.c", aprs / "beacon_core.c",
               aprs / "third_party" / "smartbeacon" / "smartbeacon.c", common / "toolbox_settings_pair.c")
    objects = [build.assemble(HERE / "probe.s", out)]
    objects += [build.compile_c(source, out, args.toolchain.resolve()) for source in sources]
    image = build.link(objects, CPU_START, "resident_probe", out, "resident_NOT_FOR_FLASH")
    body = build.text_of(image, CPU_START)
    if len(body) > SECTOR or image.entry != CPU_START:
        raise ValueError(f"{len(body)} bytes at {image.entry:#x} do not fit the candidate sector")
    unexpected = [s.name for s in image.sections if s.length and (s.flags & 2) and s.name != ".text"]
    if unexpected:
        raise ValueError(f"Unexpected loaded sections: {unexpected}")
    result = {
        "candidate_flash_sector": hex(FLASH_START),
        "candidate_cpu_address": hex(CPU_START),
        "far_backup_sha256": sha256(backup).hexdigest(),
        "text_bytes": len(body),
        "text_sha256": sha256(body).hexdigest(),
        "exports": {s.name: hex(s.value) for s in image.symbols
                    if s.name.startswith(("h9_bs_", "h9_rs_", "tsp_"))},
    }
    (out / "placement.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
