#!/usr/bin/env python3
"""Build the stock-derived TD-H9 Brick Breaker firmware package offline."""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
RESEARCH = ROOT / "tools" / "h9-firmware-research"
FLASHER = ROOT / "tools" / "h9-flasher"
ISA = ROOT / "tools" / "pi32v2-isa"
IMAGE_BASE = 0x01E00100
STOCK_IMAGE_SIZE = 0xD3C2C
STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
HOOK_OFFSET = 0x88906
GAME_OFFSET = 0xA77F0
GAME_SIZE = 1704
LAUNCHER_OFFSET = 0xA9D9E
LAUNCHER_SIZE = 1004
ORIGINAL_HOOK = bytes.fromhex("be ea 6d 82")
PROVEN_GAME_SHA256 = "19ab1b31a386634d04c964d748ceb838b773265e06ba842de0c3df8c25d0b536"


def verify_stock_image(path: Path) -> None:
    data = path.read_bytes()
    if len(data) != STOCK_IMAGE_SIZE or sha256(data).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected the exact official v1.0.38 main application")


def build(stock_image: Path, stock_package: Path, output: Path,
          source: Path | None = None) -> dict:
    verify_stock_image(stock_image)
    stock = stock_image.read_bytes()
    if stock[HOOK_OFFSET:HOOK_OFFSET + 4] != ORIGINAL_HOOK:
        raise ValueError("Stock my_app entry bytes differ from the pinned hook")
    for offset, size in ((GAME_OFFSET, GAME_SIZE),
                         (LAUNCHER_OFFSET, LAUNCHER_SIZE)):
        if stock[offset:offset + size] != b"\xff" * size:
            raise ValueError(f"Code cave at 0x{offset:X} is not erased in stock app")
    if not (ISA / "pi32v2isa" / "asm.py").is_file():
        raise ValueError("Local pi32v2-isa assembler is missing")

    output.mkdir(parents=True, exist_ok=True)
    source = source or ROOT / "src" / "game.s"
    launcher_source = ROOT / "src" / "launcher.s"
    linker = ROOT / "src" / "brick_breaker.ld"
    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm, elf, ld  # noqa: E402

    obj = output / "brick_breaker_game.o"
    obj.write_bytes(asm.assemble_source(source.read_text(encoding="utf-8"),
                                        filename=str(output / source.name),
                                        include=[str(output)]))
    launcher_obj = output / "brick_breaker_launcher.o"
    launcher_obj.write_bytes(asm.assemble_source(
        launcher_source.read_text(encoding="utf-8"),
        filename=str(output / launcher_source.name), include=[str(output)]))
    linked = output / "brick_breaker_stock_first.elf"
    link_options = ld.parse_args(["-T", str(linker), "-o", str(linked),
                                  str(obj), str(launcher_obj)])
    link_state, elf_bytes = ld.link(link_options)
    if elf_bytes is None or link_state.errors:
        raise RuntimeError(f"Pi32 linker failed: {link_state.errors}")
    linked.write_bytes(elf_bytes)
    program = elf.read(linked.read_bytes())
    game = next(s for s in program.sections if s.name == ".text")
    launcher = next(s for s in program.sections if s.name == ".launcher")
    if (program.entry != IMAGE_BASE + LAUNCHER_OFFSET or
            game.addr != IMAGE_BASE + GAME_OFFSET or
            launcher.addr != program.entry):
        raise ValueError("Linked sections are at unexpected flash addresses")
    game_code = bytes(game.data)
    launcher_code = bytes(launcher.data)
    if len(game_code) > GAME_SIZE or len(launcher_code) > LAUNCHER_SIZE:
        raise ValueError("Game or launcher exceeds its pinned erased cave")
    if sha256(game_code).hexdigest() != PROVEN_GAME_SHA256:
        raise ValueError("Game bytes differ from the hardware-tested stage 26")
    resume = [s.value for s in program.symbols
              if s.name == "brick_breaker_after_stock_setup"]
    if len(resume) != 1 or not (game.addr < resume[0] < game.addr + len(game_code)):
        raise ValueError("Game resume symbol is outside the game section")

    # One stock my_app loop call is redirected through the hotkey wrapper.
    def call_bytes(displacement: int) -> bytes:
        object_bytes = asm.assemble_source(f"call {displacement}\n")
        return bytes(next(s for s in elf.read(object_bytes).sections
                          if s.name == ".text").data)

    hook = call_bytes(LAUNCHER_OFFSET - HOOK_OFFSET - 4)
    if len(hook) != 4:
        raise ValueError("my_app entry hook did not fit four bytes")

    patched = bytearray(stock)
    patched[HOOK_OFFSET:HOOK_OFFSET + 4] = hook
    patched[GAME_OFFSET:GAME_OFFSET + len(game_code)] = game_code
    patched[LAUNCHER_OFFSET:LAUNCHER_OFFSET + len(launcher_code)] = launcher_code
    raw_path = output / "h9_brick_breaker_hotkey_NOT_FOR_FLASH.raw"
    raw_path.write_bytes(patched)

    package = output / "h9_brick_breaker_hotkey_OFFLINE_EXPERIMENT.fw"
    rebuilt = output / "h9_brick_breaker_hotkey_REBUILD_CHECK.fw"
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
    sys.path.insert(0, str(FLASHER))
    from h9_flasher import parse_firmware  # noqa: E402

    original_flash = parse_firmware(stock_package).image
    target_flash = parse_firmware(package).image
    changed = tuple(offset for offset in range(0, len(original_flash), 4096)
                    if original_flash[offset:offset + 4096] !=
                    target_flash[offset:offset + 4096])
    if changed != (0x5000, 0x8D000, 0xAC000, 0xAE000, 0xD9000):
        raise ValueError(f"Unexpected display-image flash sectors: {changed!r}")
    manifest = {
        "kind": "stock_derived_brick_breaker_hotkey",
        "source": source.name,
        "marker": "stock_ui_back_hold_without_decoder_call",
        "stock_app_sha256": STOCK_SHA256,
        "hook_offset": hex(HOOK_OFFSET),
        "hook_original_bytes": ORIGINAL_HOOK.hex(),
        "hook_new_bytes": hook.hex(),
        "game_offset": hex(GAME_OFFSET),
        "game_bytes": len(game_code),
        "game_sha256": sha256(game_code).hexdigest(),
        "launcher_offset": hex(LAUNCHER_OFFSET),
        "launcher_bytes": len(launcher_code),
        "game_resume_address": hex(resume[0]),
        "app_sha256": sha256(patched).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target_flash).hexdigest(),
        "changed_flash_sectors": [hex(offset) for offset in changed],
        "hardware_observation": "One TD-H9 owner reported normal UI, working short Back taps, and game launch on long Back hold",
    }
    (output / "brick-breaker-hotkey-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock-image", type=Path, required=True)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--source", type=Path,
        default=ROOT / "src" / "game.s")
    parser.add_argument("--output", type=Path,
        default=ROOT / "build")
    args = parser.parse_args()
    manifest = build(args.stock_image.resolve(), args.stock_package.resolve(),
                     args.output.resolve(), args.source.resolve())
    print(json.dumps(manifest, indent=2))
    print("Offline build only; reproduced the package hash tested on one TD-H9")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
