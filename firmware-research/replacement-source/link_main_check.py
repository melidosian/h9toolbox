#!/usr/bin/env python3
"""Whole-archive Pi32v2 link check; emits an unbootable diagnostic ELF only."""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import re
import shutil
import struct
import sys

from build_target_objects import ROOT, build, find_compiler, run


IMAGE_BASE = 0x01E00100
STOCK_IMAGE_SIZE = 0xD3C2C
STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"


def sibling_tool(compiler: str, name: str) -> str:
    suffix = ".exe" if sys.platform == "win32" else ""
    candidate = Path(compiler).parent / (name + suffix)
    found = shutil.which(str(candidate)) or shutil.which(name)
    if found is None:
        raise ValueError(f"Tool not found: {name}")
    return found


def verify_stock_image(path: Path) -> None:
    data = path.read_bytes()
    if len(data) != STOCK_IMAGE_SIZE or sha256(data).hexdigest() != STOCK_SHA256:
        raise ValueError("Stock main app hash or size differs from pinned evidence")


def verify_linked_elf(path: Path) -> int:
    data = path.read_bytes()
    if len(data) < 52 or data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Linker did not emit little-endian ELF32")
    file_type, machine = struct.unpack_from("<HH", data, 16)
    entry = struct.unpack_from("<I", data, 24)[0]
    if file_type != 2 or machine != 0xF1:
        raise ValueError("Linker did not emit an executable Pi32v2 ELF")
    if entry != IMAGE_BASE:
        raise ValueError(f"Diagnostic entry differs from main app base: 0x{entry:08X}")
    return entry


def link(compiler: str, output: Path, stock_image: Path | None = None) -> dict:
    if stock_image is not None:
        verify_stock_image(stock_image)
    output.mkdir(parents=True, exist_ok=True)
    object_manifest = build("main", compiler, output / "objects", no_lto=True)
    source_dir = ROOT / "main"
    runtime = output / "link_check_runtime.o"
    run([compiler, "-target", "pi32v2", "-mcpu=r3", "-Oz", "-std=c11",
         "-ffreestanding", "-fno-builtin", "-Wall", "-Wextra", "-Werror",
         "-c", str(source_dir / "link_check_runtime.c"), "-o", str(runtime)])
    linker = sibling_tool(compiler, "pi32v2-ld")
    if "elf32pi32v2" not in run([linker, "-V"]):
        raise ValueError("Linker lacks elf32pi32v2 emulation")
    archive = output / "objects" / object_manifest["archive"]
    elf = output / "h9_main_link_check.elf"
    map_file = output / "h9_main_link_check.map"
    script = source_dir / "link_check.ld"
    run([linker, "-m", "elf32pi32v2", "-T", str(script),
         "-Map", str(map_file), "-o", str(elf), str(runtime),
         "--whole-archive", str(archive), "--no-whole-archive"])
    entry = verify_linked_elf(elf)
    undefined = run([sibling_tool(compiler, "llvm-nm"), "-u", str(elf)])
    if undefined:
        raise ValueError(f"Linked ELF has undefined symbols:\n{undefined}")
    map_data = map_file.read_text(encoding="utf-8", errors="replace")
    archive_section = map_data.split("Memory Configuration", 1)[0]
    included = re.findall(r"libh9_main_replacement\.a\(([^)]+)\)",
                          archive_section)
    expected = {item["object"] for item in object_manifest["objects"]}
    if len(included) != len(expected) or set(included) != expected:
        raise ValueError("Link map does not include every main source object")
    manifest = {
        "kind": "main_whole_archive_link_check",
        "stock_envelope": {
            "image_base": IMAGE_BASE,
            "image_bytes": STOCK_IMAGE_SIZE,
            "stock_sha256": STOCK_SHA256,
            "private_stock_image_checked": stock_image is not None,
            "ram_layout_verified": False,
        },
        "compiler": compiler,
        "linker": linker,
        "source_objects": len(included),
        "linker_script_sha256": sha256(script.read_bytes()).hexdigest(),
        "elf": elf.name,
        "elf_sha256": sha256(elf.read_bytes()).hexdigest(),
        "map": map_file.name,
        "map_sha256": sha256(map_file.read_bytes()).hexdigest(),
        "entry": entry,
        "firmware_image_created": False,
        "flashable": False,
    }
    (output / "link-check-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", help="JieLi Pi32v2 clang executable path")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build" / "link-check-main")
    parser.add_argument("--stock-image", type=Path,
                        help="optionally recheck the private pinned app.bin")
    args = parser.parse_args()
    try:
        compiler = find_compiler("main", args.compiler)
        manifest = link(compiler, args.output, args.stock_image)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Link check failed: {error}", file=sys.stderr)
        return 1
    print(f"Linked {manifest['source_objects']} main source objects: "
          f"{args.output / manifest['elf']}")
    print(f"Entry: 0x{manifest['entry']:08X}")
    print("Diagnostic ELF only; no bootable or flashable update image was produced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
