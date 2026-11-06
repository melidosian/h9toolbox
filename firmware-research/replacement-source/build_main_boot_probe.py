#!/usr/bin/env python3
"""Build an offline Pi32v2 early-execution probe, not a radio-tested image."""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import re

from build_target_objects import ROOT, find_compiler, run
from link_main_check import (
    IMAGE_BASE, STOCK_BOOT_ARGS, STOCK_IMAGE_SIZE, STOCK_SHA256,
    sibling_tool, verify_stock_image,
)


def build(compiler: str, output: Path, stock_image: Path) -> dict:
    verify_stock_image(stock_image)
    stock = stock_image.read_bytes()
    output.mkdir(parents=True, exist_ok=True)
    source = ROOT / "main"
    startup = output / "boot_probe_startup.o"
    runtime = output / "boot_probe.o"
    elf = output / "h9_main_boot_probe.elf"
    raw_path = output / "h9_main_boot_probe_NOT_FOR_FLASH.raw"
    linker = sibling_tool(compiler, "pi32v2-ld")
    objcopy = sibling_tool(compiler, "llvm-objcopy")
    nm = sibling_tool(compiler, "llvm-nm")

    run([compiler, "-target", "pi32v2", "-mcpu=r3", "-c",
         str(source / "boot_probe_startup.S"), "-o", str(startup)])
    run([compiler, "-target", "pi32v2", "-mcpu=r3", "-Oz", "-std=c11",
         "-ffreestanding", "-fno-builtin", "-Wall", "-Wextra", "-Werror",
         "-c", str(source / "boot_probe.c"), "-o", str(runtime)])
    run([linker, "-m", "elf32pi32v2", "-T", str(source / "boot_probe.ld"),
         "-Map", str(output / "h9_main_boot_probe.map"), "-o", str(elf),
         str(startup), str(runtime)])

    symbols = run([nm, str(elf)])
    def address(name: str) -> int:
        match = re.search(rf"^([0-9a-fA-F]+) [A-Za-z] {re.escape(name)}$",
                          symbols, re.M)
        if match is None:
            raise ValueError(f"Missing linked symbol: {name}")
        return int(match.group(1), 16)

    if address("h9_main_reset") != IMAGE_BASE:
        raise ValueError("Reset entry differs from stock application base")
    led_address = address("h9_main_led9_blink")
    if not IMAGE_BASE <= led_address < IMAGE_BASE + STOCK_IMAGE_SIZE:
        raise ValueError("Breathing LED signal is outside the app image")
    if address("__stock_clear_start") != 0xE2E0 or \
            address("__stock_clear_end") != 0x1E4D4:
        raise ValueError("RAM clear bounds differ from stock")
    if run([nm, "-u", str(elf)]):
        raise ValueError("Probe contains undefined symbols")

    text_file = output / "boot-probe-text.bin"
    run([objcopy, "-O", "binary", "-j=.text", str(elf), str(text_file)])
    code = text_file.read_bytes()
    if code[2:8] != bytes.fromhex("ee ff 90 e2 00 00") or \
            code[8:14] != bytes.fromhex("ed ff 90 e2 00 00"):
        raise ValueError("Stock stack setup bytes changed")
    boot_offset = address("h9_main_copy_boot_args") - IMAGE_BASE
    if code[boot_offset:boot_offset + len(STOCK_BOOT_ARGS)] != STOCK_BOOT_ARGS:
        raise ValueError("Stock launch argument copy bytes changed")
    if bytes.fromhex("54 42 39 48") not in code or \
            bytes.fromhex("ab bd c6 b7") not in code:
        raise ValueError("Boot marker is missing")
    if len(code) > STOCK_IMAGE_SIZE:
        raise ValueError("Probe exceeds the stock app size")
    # Preserve the stock app outside the linked probe. A probe only needs to
    # replace its entry code; padding the remaining app with 0xFF would force
    # nearly the entire main firmware area through erase/write/recovery.
    raw = code + stock[len(code):]
    raw_path.write_bytes(raw)
    manifest = {
        "kind": "main_early_execution_probe",
        "entry": IMAGE_BASE,
        "app_bytes": len(raw),
        "code_bytes": len(code),
        "stock_app_sha256": STOCK_SHA256,
        "private_stock_image_checked": True,
        "stock_app_tail_preserved": True,
        "app_bytes_changed": sum(a != b for a, b in zip(raw, stock)),
        "stock_launch_abi_bytes_verified": True,
        "stock_ram_clear_bounds_verified": True,
        "ram_probe_address": 0xE2E0,
        "ram_probe_expected_words": [0x48394254, 0xB7C6BDAB],
        "visible_signal": "stock_breathing_led_gpio9_three_pulses",
        "visible_signal_address": led_address,
        "visible_signal_hardware_verified": False,
        "elf": elf.name,
        "elf_sha256": sha256(elf.read_bytes()).hexdigest(),
        "raw": raw_path.name,
        "raw_sha256": sha256(raw).hexdigest(),
        "hardware_boot_verified": False,
        "flashable": False,
    }
    (output / "boot-probe-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler")
    parser.add_argument("--stock-image", required=True, type=Path)
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build" / "boot-probe-main")
    args = parser.parse_args()
    manifest = build(find_compiler("main", args.compiler), args.output,
                     args.stock_image)
    print(f"Linked probe: {args.output / manifest['elf']}")
    print(f"Offline app layout: {args.output / manifest['raw']}")
    print(f"SHA-256: {manifest['raw_sha256']}")
    print("Hardware boot unverified; no flashable update image produced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
