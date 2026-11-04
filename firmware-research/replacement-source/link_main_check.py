#!/usr/bin/env python3
"""Whole-archive Pi32v2 link check with first-pass stock entry reconstruction.

The board entry remains inert. This emits an unbootable diagnostic ELF only.
"""

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
RAM_START = 0xE2E0
RAM_END = 0x1E4D4
STOCK_BOOT_ARGS = bytes.fromhex(
    "50 ec 04 20 c1 ff a0 e2 00 00 50 ec 11 21 02 60 "
    "23 62 93 66 2a 4d 9a 40 0a 66 9a 6e 0a 67 9a 6f "
    "50 ec 00 21 50 ec 11 22 80 00"
)
STOCK_ICFG_AFTER_RAM_COPY = bytes.fromhex("64 e0 00 2b 72 e1 40 2f 64 e0 80 2b")


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
    if data[4:16] != bytes.fromhex("ee ff 90 e2 00 00 ed ff 90 e2 00 00") or \
            data[0x2D82E:0x2D858] != STOCK_BOOT_ARGS:
        raise ValueError("Stock Pi32 entry or boot-argument copy differs")
    if data[0x7E:0x8A] != STOCK_ICFG_AFTER_RAM_COPY:
        raise ValueError("Stock post-RAM-copy interrupt configuration differs")


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
    startup = output / "h9_main_startup.o"
    run([compiler, "-target", "pi32v2", "-mcpu=r3", "-c",
         str(source_dir / "h9_main_startup.S"), "-o", str(startup)])
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
         "-Map", str(map_file), "-o", str(elf), str(startup), str(runtime),
         "--whole-archive", str(archive), "--no-whole-archive"])
    entry = verify_linked_elf(elf)
    text_section = output / "main-text.bin"
    run([sibling_tool(compiler, "llvm-objcopy"), "-O", "binary", "-j=.text",
         str(elf), str(text_section)])
    text_data = text_section.read_bytes()
    first = text_data[:14]
    if len(first) != 14 or first[2:8] != bytes.fromhex("ee ff 90 e2 00 00") or \
            first[8:14] != bytes.fromhex("ed ff 90 e2 00 00"):
        raise ValueError("Linked main entry lacks the stock stack setup bytes")
    symbols = run([sibling_tool(compiler, "llvm-nm"), str(elf)])
    match = re.search(r"^([0-9a-fA-F]+) [tT] h9_main_copy_boot_args$",
                      symbols, re.M)
    if match is None:
        raise ValueError("Linked main entry lacks boot-argument copy symbol")
    boot_offset = int(match.group(1), 16) - IMAGE_BASE
    if text_data[boot_offset:boot_offset + len(STOCK_BOOT_ARGS)] != STOCK_BOOT_ARGS:
        raise ValueError("Linked boot-argument copy differs from stock bytes")
    def symbol_address(name: str) -> int:
        symbol = re.search(rf"^([0-9a-fA-F]+) [A-Za-z] {re.escape(name)}$",
                           symbols, re.M)
        if symbol is None:
            raise ValueError(f"Linked main ELF lacks {name}")
        return int(symbol.group(1), 16)

    icfg_offset = symbol_address("h9_main_interrupt_config") - IMAGE_BASE
    if text_data[icfg_offset:icfg_offset + len(STOCK_ICFG_AFTER_RAM_COPY)] != \
            STOCK_ICFG_AFTER_RAM_COPY:
        raise ValueError("Linked interrupt configuration differs from stock bytes")

    data_start = symbol_address("__data_start")
    data_end = symbol_address("__data_end")
    bss_start = symbol_address("__bss_start")
    bss_end = symbol_address("__bss_end")
    data_load = symbol_address("__data_load_start")
    ram_code_start = symbol_address("__ram_code_start")
    ram_code_end = symbol_address("__ram_code_end")
    ram_code_load = symbol_address("__ram_code_load_start")
    serial_bus_entry = symbol_address("h9_main_serial_bus_transfer")
    serial_read_entry = symbol_address("h9_main_serial_command_read")
    serial_write_entry = symbol_address("h9_main_serial_command_write")
    if symbol_address("__stock_clear_start") != RAM_START or \
            symbol_address("__stock_clear_end") != RAM_END:
        raise ValueError("Linked RAM-clear interval differs from stock startup")
    if not (RAM_START <= data_start <= data_end <= bss_start <= bss_end <= RAM_END):
        raise ValueError("Linked C data/BSS exceed observed stock-cleared RAM interval")
    if not (0 <= ram_code_start <= ram_code_end <= 0xDA78):
        raise ValueError("Linked RAM code exceeds stock relocation interval")
    if ram_code_start != 0 or serial_bus_entry != 0x40 or ram_code_end <= 0x40:
        raise ValueError("Linked RAM serial-bus routine is not at stock RAM address 0x40")
    if not (serial_bus_entry < serial_read_entry < ram_code_end and
            serial_bus_entry < serial_write_entry < ram_code_end):
        raise ValueError("Linked serial command handlers are not relocated RAM code")
    ram_code_section = output / "main-ram-code.bin"
    run([sibling_tool(compiler, "llvm-objcopy"), "-O", "binary",
         "-j=.ram_code", str(elf), str(ram_code_section)])
    ram_code_data = ram_code_section.read_bytes()
    if len(ram_code_data) != ram_code_end - ram_code_start or \
            ram_code_data[:0x40] != bytes(0x40):
        raise ValueError("Linked RAM code has unexpected length or nonzero prefix")
    if not (IMAGE_BASE <= data_load and data_load + data_end - data_start <=
            IMAGE_BASE + STOCK_IMAGE_SIZE):
        raise ValueError("Linked C data load exceeds stock app address envelope")
    if not (IMAGE_BASE <= ram_code_load and
            ram_code_load + ram_code_end - ram_code_start <=
            IMAGE_BASE + STOCK_IMAGE_SIZE):
        raise ValueError("Linked RAM-code load exceeds stock app address envelope")
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
        "startup_object": startup.name,
        "stock_stack_pointer": 58000,
        "entry_stack_bytes_verified": True,
        "boot_argument_copy_bytes_verified": True,
        "post_ram_copy_icfg_bytes_verified": True,
        "c_ram_start": data_start,
        "c_ram_end": bss_end,
        "ram_code_bytes": ram_code_end - ram_code_start,
        "ram_serial_bus_entry": serial_bus_entry,
        "ram_serial_read_entry": serial_read_entry,
        "ram_serial_write_entry": serial_write_entry,
        "ram_code_zero_prefix_verified": True,
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
