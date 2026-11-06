#!/usr/bin/env python3
"""Build an offline stock-derived LCD transport experiment for the TD-H9."""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys

from link_main_check import IMAGE_BASE, STOCK_SHA256, verify_stock_image


ROOT = Path(__file__).resolve().parent
RESEARCH = ROOT.parent / "h9-firmware-research"
ISA = RESEARCH / "output" / "pi32v2-isa"
HOOK_OFFSET = 0x8841E
CAVE_OFFSET = 0xA77F0
CAVE_SIZE = 1704
ORIGINAL_HOOK = bytes.fromhex("bf ea 52 8c")
MESSAGE = "HELLO WORLD"

GLYPHS = {
    " ": (0, 0, 0, 0, 0, 0, 0),
    "D": (0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110),
    "E": (0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111),
    "H": (0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001),
    "L": (0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111),
    "O": (0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110),
    "R": (0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001),
    "W": (0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010),
}


def pixels() -> bytes:
    result = bytearray()
    for row in range(8):
        for letter in MESSAGE:
            bits = GLYPHS[letter][row] if row < 7 else 0
            for column in range(6):
                lit = column < 5 and bool(bits & (1 << (4 - column)))
                result.extend(b"\xff\xff" if lit else b"\0\0")
    assert len(result) == 66 * 8 * 2
    return bytes(result)


def build(stock_image: Path, stock_package: Path, output: Path,
          source: Path | None = None) -> dict:
    verify_stock_image(stock_image)
    stock = stock_image.read_bytes()
    if stock[HOOK_OFFSET:HOOK_OFFSET + 4] != ORIGINAL_HOOK:
        raise ValueError("Stock LCD backlight call bytes differ from the pinned hook")
    if stock[CAVE_OFFSET:CAVE_OFFSET + CAVE_SIZE] != b"\xff" * CAVE_SIZE:
        raise ValueError("Proposed code cave is not completely erased in stock app")
    if not (ISA / "pi32v2isa" / "asm.py").is_file():
        raise ValueError("Local pi32v2-isa assembler is missing")

    output.mkdir(parents=True, exist_ok=True)
    pixel_path = output / "hello_display_pixels.rgb565"
    pixel_path.write_bytes(pixels())
    source = source or ROOT / "main" / "hello_display_hook.s"
    linker = ROOT / "main" / "hello_display_hook.ld"
    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm, elf, ld  # noqa: E402

    obj = output / "hello_display_hook.o"
    obj.write_bytes(asm.assemble_source(source.read_text(encoding="utf-8"),
                                        filename=str(output / source.name),
                                        include=[str(output)]))
    linked = output / "hello_display_hook.elf"
    link_options = ld.parse_args(["-T", str(linker), "-o", str(linked), str(obj)])
    link_state, elf_bytes = ld.link(link_options)
    if elf_bytes is None or link_state.errors:
        raise RuntimeError(f"Pi32 linker failed: {link_state.errors}")
    linked.write_bytes(elf_bytes)
    program = elf.read(linked.read_bytes())
    section = next(s for s in program.sections if s.name == ".text")
    if program.entry != IMAGE_BASE + CAVE_OFFSET or section.addr != program.entry:
        raise ValueError("Linked display hook is at the wrong flash address")
    code = bytes(section.data)
    if len(code) > CAVE_SIZE or code[-len(pixels()):] != pixels():
        raise ValueError("Display hook exceeds code cave or loses its pixel data")

    # The stock call at this site is a 4-byte relative Pi32 instruction. The
    # independent assembler must reproduce it before making a new branch.
    def call_bytes(displacement: int) -> bytes:
        object_bytes = asm.assemble_source(f"call {displacement}\n")
        return bytes(next(s for s in elf.read(object_bytes).sections
                          if s.name == ".text").data)

    stock_displacement = (IMAGE_BASE + 0x79CC6) - (IMAGE_BASE + HOOK_OFFSET + 4)
    if call_bytes(stock_displacement) != ORIGINAL_HOOK:
        raise ValueError("Open assembler does not reproduce the stock call encoding")
    hook = call_bytes(CAVE_OFFSET - HOOK_OFFSET - 4)
    if len(hook) != 4:
        raise ValueError("LCD hook call did not fit the original instruction")

    patched = bytearray(stock)
    patched[HOOK_OFFSET:HOOK_OFFSET + 4] = hook
    patched[CAVE_OFFSET:CAVE_OFFSET + len(code)] = code
    raw_path = output / "h9_hello_display_NOT_FOR_FLASH.raw"
    raw_path.write_bytes(patched)

    package = output / "h9_hello_display_OFFLINE_EXPERIMENT.fw"
    rebuilt = output / "h9_hello_display_REBUILD_CHECK.fw"
    destination = rebuilt if package.exists() else package
    if destination.exists():
        raise ValueError(f"Refusing to overwrite package check: {destination}")
    repack = RESEARCH / "firmware.py"
    result = subprocess.run(
        [sys.executable, str(repack), "repack", str(stock_package), "--app",
         str(raw_path), "--output", str(destination)], capture_output=True,
        text=True)
    if result.returncode:
        raise RuntimeError(f"Stock-derived package rebuild failed:\n{result.stdout}{result.stderr}")
    if destination == rebuilt:
        if rebuilt.read_bytes() != package.read_bytes():
            raise ValueError("Rebuilt display package differs from existing package")
        rebuilt.unlink()
    sys.path.insert(0, str(ROOT.parent / "h9-flasher"))
    from h9_flasher import parse_firmware  # noqa: E402

    original_flash = parse_firmware(stock_package).image
    target_flash = parse_firmware(package).image
    changed = tuple(offset for offset in range(0, len(original_flash), 4096)
                    if original_flash[offset:offset + 4096] !=
                    target_flash[offset:offset + 4096])
    if changed != (0x5000, 0x8D000, 0xAC000, 0xD9000):
        raise ValueError(f"Unexpected display-image flash sectors: {changed!r}")
    manifest = {
        "kind": "stock_derived_main_hello_display_transport_experiment",
        "source": source.name,
        "message": MESSAGE,
        "stock_app_sha256": STOCK_SHA256,
        "hook_offset": hex(HOOK_OFFSET),
        "hook_original_bytes": ORIGINAL_HOOK.hex(),
        "hook_new_bytes": hook.hex(),
        "cave_offset": hex(CAVE_OFFSET),
        "cave_bytes": len(code),
        "app_sha256": sha256(patched).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target_flash).hexdigest(),
        "changed_flash_sectors": [hex(offset) for offset in changed],
        "hardware_boot_verified": False,
        "display_text_verified": False,
    }
    (output / "hello-display-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock-image", type=Path, required=True)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--source", type=Path,
                        default=ROOT / "main" / "hello_display_hook.s")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build" / "hello-display-main")
    args = parser.parse_args()
    manifest = build(args.stock_image.resolve(), args.stock_package.resolve(),
                     args.output.resolve(), args.source.resolve())
    print(json.dumps(manifest, indent=2))
    print("Offline package only; hardware display behavior is unverified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
