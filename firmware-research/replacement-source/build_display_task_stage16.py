#!/usr/bin/env python3
"""Build an offline cleared-panel display-task entry probe for the TD-H9."""

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
HOOK_OFFSET = 0x88180
CAVE_OFFSET = 0xA77F0
CAVE_SIZE = 1704
ORIGINAL_HOOK = bytes.fromhex("e2 81 40 e0")


def pixels() -> bytes:
    result = bytearray()
    for row in range(16):
        for column in range(16):
            # White border, red center: distinct from a powered black panel.
            result.extend(b"\xff\xff" if row in (0, 15) or column in (0, 15)
                          else b"\xf8\x00")
    assert len(result) == 16 * 16 * 2
    return bytes(result)


def build(stock_image: Path, stock_package: Path, output: Path,
          source: Path | None = None) -> dict:
    verify_stock_image(stock_image)
    stock = stock_image.read_bytes()
    if stock[HOOK_OFFSET:HOOK_OFFSET + 4] != ORIGINAL_HOOK:
        raise ValueError("Stock my_app entry bytes differ from the pinned hook")
    if stock[CAVE_OFFSET:CAVE_OFFSET + CAVE_SIZE] != b"\xff" * CAVE_SIZE:
        raise ValueError("Proposed code cave is not completely erased in stock app")
    if not (ISA / "pi32v2isa" / "asm.py").is_file():
        raise ValueError("Local pi32v2-isa assembler is missing")

    output.mkdir(parents=True, exist_ok=True)
    pixel_path = output / "hello_display_pixels.rgb565"
    pixel_path.write_bytes(pixels())
    source = source or ROOT / "main" / "display_task_entry_stage7.s"
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

    # The stock my_app task entry is replaced by a nonreturning relative call.
    def call_bytes(displacement: int) -> bytes:
        object_bytes = asm.assemble_source(f"call {displacement}\n")
        return bytes(next(s for s in elf.read(object_bytes).sections
                          if s.name == ".text").data)

    hook = call_bytes(CAVE_OFFSET - HOOK_OFFSET - 4)
    if len(hook) != 4:
        raise ValueError("my_app entry hook did not fit four bytes")

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
        "kind": "replacement_my_app_display_task_entry_experiment",
        "source": source.name,
        "marker": "16x16_white_border_red_center_big_endian_rgb565",
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
        "display_marker_verified": False,
    }
    (output / "hello-display-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock-image", type=Path, required=True)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--source", type=Path,
                        default=ROOT / "main" / "display_task_entry_stage16.s")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build" / "display-task-stage16")
    args = parser.parse_args()
    manifest = build(args.stock_image.resolve(), args.stock_package.resolve(),
                     args.output.resolve(), args.source.resolve())
    print(json.dumps(manifest, indent=2))
    print("Offline package only; hardware display behavior is unverified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
