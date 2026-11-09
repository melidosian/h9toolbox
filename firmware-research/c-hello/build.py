#!/usr/bin/env python3
"""Build the H9 C hello firmware: the stage-23 startup (entry.s) plus C code
(main.c) compiled with JieLi's Pi32v2 clang in Docker (../toolchain/h9cc),
linked into the stage-23 code cave in place of stock my_app.

    export JIELI_TOOLCHAIN=~/path/to/jieli-linux-toolchains-20260730.1
    python3 -I build.py --stock-image <app.bin> --stock-package <TD-H9-V1.0.38.fw>
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
RESEARCH = ROOT.parent
TOOLS = RESEARCH / "brick-breaker" / "tools"
ISA = TOOLS / "pi32v2-isa"
H9CC = RESEARCH / "toolchain" / "h9cc"
IMAGE_BASE = 0x01E00100
STOCK_IMAGE_SIZE = 0xD3C2C
STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
STOCK_PACKAGE_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
# Same hook and cave as the hardware-tested stage-23 Snake task.
HOOK_OFFSET = 0x88180
ORIGINAL_HOOK = bytes.fromhex("e2 81 40 e0")
CAVE_OFFSET = 0xA77F0
CAVE_SIZE = 1704
SECTORS = (0x5000, 0x8D000, 0xAC000, 0xD9000)
C_FLAGS = ["-target", "pi32v2", "-mcpu=r3", "-Oz", "-integrated-as",
           "-ffreestanding", "-fno-builtin", "-fno-common", "-Wall", "-Werror"]
API = {"h9_lcd_window", "h9_lcd_write", "h9_key", "h9_delay", "h9_led"}


# --source picks the C file (main.c is C-hello; probe.c is the far-region
# probe); outputs are named h9_c_<tag>.
SOURCE = "main.c"
TAG = "hello"


def compile_c(source: Path, output: Path) -> Path:
    """Run JieLi clang in the container; paths must be inside ROOT (mounted)."""
    obj = output / f"{source.stem}.o"
    result = subprocess.run([str(H9CC), "clang", *C_FLAGS, "-c", source.name,
                             "-o", str(obj.relative_to(ROOT))],
                            cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"JieLi clang failed:\n{result.stdout}{result.stderr}")
    return obj


def build(stock_image: Path, stock_package: Path, output: Path) -> dict:
    stock = stock_image.read_bytes()
    if len(stock) != STOCK_IMAGE_SIZE or sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected the exact official v1.0.38 main application")
    if sha256(stock_package.read_bytes()).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the exact official v1.0.38 .fw package")
    if stock[HOOK_OFFSET:HOOK_OFFSET + 4] != ORIGINAL_HOOK:
        raise ValueError("Stock my_app entry bytes differ from the pinned hook")
    if stock[CAVE_OFFSET:CAVE_OFFSET + CAVE_SIZE] != b"\xff" * CAVE_SIZE:
        raise ValueError("Code cave is not erased in the stock app")
    output.mkdir(parents=True, exist_ok=True)

    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm, elf, ld  # noqa: E402

    c_obj = compile_c(ROOT / SOURCE, output)
    c_program = elf.read(c_obj.read_bytes())
    undefined = {s.name for s in c_program.symbols if s.name and s.shndx == 0}
    if not undefined <= API:
        raise ValueError(f"C code needs symbols the firmware does not provide: {sorted(undefined - API)}")
    data_sections = [s.name for s in c_program.sections
                     if s.name.startswith((".data", ".bss", ".sdata", ".sbss")) and len(s.data or b"")]
    if data_sections:
        raise ValueError(f"C code has data sections (no RAM is set aside for them): {data_sections}")

    entry_obj = output / "entry.o"
    entry_obj.write_bytes(asm.assemble_source((ROOT / "entry.s").read_text(encoding="utf-8"),
                                              filename=str(ROOT / "entry.s")))
    linked = output / f"h9_c_{TAG}.elf"
    state, elf_bytes = ld.link(ld.parse_args(["-T", str(ROOT / "link.ld"), "-o", str(linked),
                                              str(entry_obj), str(c_obj)]))
    if elf_bytes is None or state.errors:
        raise RuntimeError(f"Pi32 linker failed: {state.errors}")
    linked.write_bytes(elf_bytes)
    program = elf.read(elf_bytes)
    text = next(s for s in program.sections if s.name == ".text")
    if text.addr != IMAGE_BASE + CAVE_OFFSET or program.entry != text.addr:
        raise ValueError("Linked program is not at the pinned cave address")
    code = bytes(text.data)
    if len(code) > CAVE_SIZE:
        raise ValueError(f"Program ({len(code)} bytes) exceeds the {CAVE_SIZE}-byte cave")
    symbols = {s.name: s.value for s in program.symbols if s.name}

    call = bytes(next(s for s in elf.read(asm.assemble_source(
        f"call {CAVE_OFFSET - HOOK_OFFSET - 4}\n")).sections if s.name == ".text").data)
    patched = bytearray(stock)
    patched[HOOK_OFFSET:HOOK_OFFSET + 4] = call
    patched[CAVE_OFFSET:CAVE_OFFSET + len(code)] = code
    raw = output / f"h9_c_{TAG}_NOT_FOR_FLASH.raw"
    raw.write_bytes(patched)

    package = output / f"h9_c_{TAG}_OFFLINE_EXPERIMENT.fw"
    rebuilt = output / f"h9_c_{TAG}_REBUILD_CHECK.fw"
    destination = rebuilt if package.exists() else package
    if destination.exists():
        raise ValueError(f"Refusing to overwrite package check: {destination}")
    result = subprocess.run([sys.executable, str(TOOLS / "h9-firmware-research" / "firmware.py"),
                             "repack", str(stock_package), "--app", str(raw), "--output", str(destination)],
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"Stock-derived package rebuild failed:\n{result.stdout}{result.stderr}")
    if destination == rebuilt:
        same = rebuilt.read_bytes() == package.read_bytes()
        rebuilt.unlink()
        if not same:
            raise ValueError("Rebuilt package differs from the existing package (build is not reproducible)")

    sys.path.insert(0, str(RESEARCH / "h9-flasher"))
    from h9_flasher import parse_firmware  # noqa: E402
    original = parse_firmware(stock_package).image
    target = parse_firmware(package).image
    changed = tuple(o for o in range(0, len(original), 4096) if original[o:o + 4096] != target[o:o + 4096])
    if changed != SECTORS:
        raise ValueError(f"Unexpected changed flash sectors: {[hex(x) for x in changed]}")
    manifest = {
        "kind": "c_hello_my_app_replacement_experiment",
        "compiler": "JieLi pi32v2 clang 4.0.1 (Linux toolchain 2.5.3, Docker)",
        "c_flags": C_FLAGS,
        "stock_app_sha256": STOCK_SHA256,
        "hook_offset": hex(HOOK_OFFSET),
        "hook_new_bytes": call.hex(),
        "cave_offset": hex(CAVE_OFFSET),
        "code_bytes": len(code),
        "c_main": hex(symbols["h9_main"]),
        "c_object_sha256": sha256(c_obj.read_bytes()).hexdigest(),
        "app_sha256": sha256(patched).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target).hexdigest(),
        "changed_flash_sectors": [hex(o) for o in changed],
        "hardware_observation": "Not yet flashed",
    }
    (output / f"h9_c_{TAG}-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stock-image", type=Path, required=True)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    parser.add_argument("--source", default="main.c", help="C file in this folder (main.c or probe.c)")
    args = parser.parse_args()
    global SOURCE, TAG
    SOURCE = args.source
    TAG = "hello" if args.source == "main.c" else Path(args.source).stem
    print(json.dumps(build(args.stock_image.resolve(), args.stock_package.resolve(), args.output.resolve()), indent=2))
    print("Offline build only; not hardware tested")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
