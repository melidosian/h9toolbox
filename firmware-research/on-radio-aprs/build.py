#!/usr/bin/env python3
"""Build the Type-C v1.0.38 KISS app and matching on-radio APRS stage."""
from __future__ import annotations

import argparse
from binascii import crc32
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
RESEARCH = ROOT.parent
TOOLS = RESEARCH / "brick-breaker" / "tools"
KISS = RESEARCH / "kiss-tnc"
APP_BASE = 0x01E00100
STAGE_ADDRESS = 0xF4000
STAGE_SIZE = 0x6000
STAGE_XIP = 0x01EEF000
IMAGE_OFFSET = 0x1000
IMAGE_SIZE = 17304
CAVE_OFFSET = 0xA7D74
CAVE_END = 0xA7E50
HOOKS = (
    (0x08847E, "bfea688d", "call", "stock_screen"),
    (0x088488, "bfea398e", "call", "stock_launch"),
    (0x07A0AC, "bfea2aff", "call", "stock_idle_tick"),
    (0x07A0C2, "50ee4206", "call", "stock_ready"),
    (0x078F6E, "ffea56fb", "goto", "block_ack"),
)
HEAP_OFFSET = 0x05FA9E
BENCH_PACKAGE_SHA256 = "ee06b1f2ac4d224341ce25048b00ea2db999c16b2fe076251ff30a102ae7fc04"
BENCH_APP_SHA256 = "c011b13a35d04d5c817e8cdff1fea17b08940e38a48b108441e45101fd976fcf"
C_FLAGS = ["-target", "pi32v2", "-mcpu=r3", "-Oz", "-integrated-as",
           "-ffreestanding", "-fno-builtin", "-fno-common", "-Wall", "-Werror"]

sys.path[:0] = [str(TOOLS / name) for name in ("pi32v2-isa", "h9-firmware-research", "h9-flasher")]
from pi32v2isa import asm, elf, ld  # noqa: E402
from firmware import app_layout, decode_app_area, parse_firmware_bytes, rebuild  # noqa: E402


def digest(data: bytes) -> str:
    return sha256(data).hexdigest()


def checked_bytes(path: Path, expected: str) -> bytes:
    data = path.read_bytes()
    if digest(data) != expected:
        raise ValueError(f"{path.name} differs from its pinned release")
    return data


def app_bytes(flash: bytes) -> bytes:
    decoded = decode_app_area(flash)
    _, app, _ = app_layout(decoded)
    return bytes(decoded[app.data_offset:app.data_offset + app.size])


def assemble(source: str, destination: Path) -> Path:
    destination.write_bytes(asm.assemble_source(source, filename=str(destination)))
    return destination


def link(script: Path, destination: Path, objects: list[Path]):
    state, data = ld.link(ld.parse_args(["-T", str(script), "-o", str(destination),
                                         *(str(obj) for obj in objects)]))
    if data is None or state.errors:
        raise ValueError(f"Pi32 linker failed: {state.errors}")
    destination.write_bytes(data)
    return elf.read(data)


def text_section(program):
    return next(section for section in program.sections if section.name == ".text")


def inline(source: str) -> bytes:
    return bytes(text_section(elf.read(asm.assemble_source(source + "\n"))).data)


def compile_stage(output: Path, image: bytes, state: int) -> bytes:
    relative = output.relative_to(ROOT)
    (output / "stage-config.h").write_text(
        f"#define APRS_UPDATE_STATE {state:#x}u\n"
        f"#define APRS_STAGE_IMAGE 0x{STAGE_XIP + IMAGE_OFFSET:08x}u\n"
        f"#define APRS_STAGE_CRC32 0x{crc32(image):08x}u\n")
    objects = []
    for source in (ROOT / "update.c", ROOT / "target.c"):
        obj = relative / f"{source.stem}.o"
        subprocess.run([str(RESEARCH / "toolchain" / "h9cc"), "clang", *C_FLAGS,
                        "-I", str(relative), "-c", source.name, "-o", str(obj)], cwd=ROOT, check=True)
        for section in elf.read((ROOT / obj).read_bytes()).sections:
            if section.name.startswith((".data", ".bss", ".sdata", ".sbss")) and section.size:
                raise ValueError(f"Stage requires writable storage in {section.name}")
        objects.append(ROOT / obj)
    api = assemble((ROOT / "target-api.s").read_text(), output / "target-api.o")
    program = link(ROOT / "stage.ld", output / "aprs-stage.elf", [*objects, api])
    code = text_section(program)
    if code.addr != STAGE_XIP or program.entry != STAGE_XIP or not 0 < len(code.data) <= IMAGE_OFFSET:
        raise ValueError("Unexpected stage entry or size")
    return bytes(code.data)


def patch_app(original: bytes, code: bytes, output: Path, old_heap: int,
              version_at: int, version_word: int) -> tuple[bytes, int, list[dict]]:
    stub_base = APP_BASE + CAVE_OFFSET
    source = (ROOT / "stub.s.in").read_text()
    source = (source.replace("@ENTRY@", hex(STAGE_XIP))
              .replace("@END@", hex(STAGE_XIP + len(code)))
              .replace("@CRC_STATE@", hex(crc32(code) ^ 0xFFFFFFFF))
              .replace("@STATE@", hex(old_heap - 0x100)))
    obj = assemble(source, output / "stub.o")
    script = output / "stub.ld"
    script.write_text(f"ENTRY(stock_screen)\nSECTIONS {{ .text {stub_base:#x} : {{ *(.text) }} }}\n")
    stub = link(script, output / "aprs-hook.elf", [obj])
    text = text_section(stub)
    cave = bytes(text.data)
    if text.addr != stub_base or not 0 < len(cave) <= CAVE_END - CAVE_OFFSET:
        raise ValueError(f"Stub is {len(cave)} bytes in the {CAVE_END - CAVE_OFFSET}-byte cave")
    if original[CAVE_OFFSET:CAVE_END] != b"\xff" * (CAVE_END - CAVE_OFFSET):
        raise ValueError("Main application code cave is already occupied")
    symbols = {symbol.name: symbol.value for symbol in stub.symbols if symbol.name}
    patched = bytearray(original)
    changes = []

    def replace(offset: int, before: bytes, after: bytes) -> None:
        if len(before) != len(after) or original[offset:offset + len(before)] != before:
            raise ValueError(f"Unexpected patch size or original bytes at {offset:#x}")
        patched[offset:offset + len(after)] = after
        changes.append({"app_offset": hex(offset), "before": before.hex(), "after": after.hex()})

    for offset, old, operation, name in HOOKS:
        displacement = symbols[name] - (APP_BASE + offset + 4)
        replace(offset, bytes.fromhex(old), inline(f"{operation} {displacement}"))
    replace(HEAP_OFFSET, inline(f"r5 = {old_heap:#x}"), inline(f"r5 = {old_heap - 0x100:#x}"))
    if original[version_at - 24:version_at - 20] != b"H9AP":
        raise ValueError("Missing H9 Toolbox version table")
    replace(version_at, bytes.fromhex("04010000"), version_word.to_bytes(4, "little"))
    replace(CAVE_OFFSET, b"\xff" * len(cave), cave)
    return bytes(patched), len(cave), changes


def build(stock_package: Path, output: Path, base_package: Path | None = None) -> dict:
    kiss_manifest = json.loads((KISS / "main_kiss_patch.json").read_text())
    stock = checked_bytes(stock_package, kiss_manifest["stock_package_sha256"])
    if base_package is None:
        base = checked_bytes(KISS / "TD-H9-v1.0.38-KISS.fw", kiss_manifest["package_sha256"])
        app_hash, old_heap, version_at = kiss_manifest["app_sha256"], 0x29F00, 0xA7C78
    else:
        base = checked_bytes(base_package, BENCH_PACKAGE_SHA256)
        app_hash, old_heap, version_at = BENCH_APP_SHA256, 0x29A00, 0xA7CF4
    image = checked_bytes(KISS / "APRS-1.0.15-rx15-rxff.bin",
                          "3a70e2f48283cfd0d06fb29a08f462fc0b8302898e09d41946d5f74205daf741")
    if len(image) != IMAGE_SIZE:
        raise ValueError("Unexpected modem image size")
    baseline = parse_firmware_bytes(base).image
    original = app_bytes(baseline)
    if digest(original) != app_hash:
        raise ValueError("Unexpected KISS application contents")
    version = (ROOT / "VERSION").read_text().strip()
    components = tuple(map(int, version.split(".")))
    if len(components) != 3 or any(not 0 <= part <= 255 for part in components):
        raise ValueError("Expected a three-part firmware version, with each part below 256")
    major, minor, patch = components
    version_word = (major << 16) | (minor << 8) | patch
    output.relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=True)
    code = compile_stage(output, image, old_heap - 0x100)
    patched, stub_size, changes = patch_app(original, code, output, old_heap, version_at, version_word)
    package = rebuild(stock, patched)
    flash = parse_firmware_bytes(package).image
    if app_bytes(flash) != patched or flash[:0x5000] != baseline[:0x5000] or len(flash) != 0xDA000:
        raise ValueError("Repacked application or boot area failed verification")
    stage = (code.ljust(IMAGE_OFFSET, b"\xff") + image).ljust(STAGE_SIZE, b"\xff")
    (output / "h9-kiss-aprs-stage.fw").write_bytes(package)
    (output / "aprs-stage-0xf4000.bin").write_bytes(stage)
    manifest = {
        "firmware_version": version,
        "base_package_sha256": digest(base),
        "main_package_sha256": digest(package),
        "main_flash_sha256": digest(flash),
        "main_changed_sectors_vs_kiss": [
            hex(offset) for offset in range(0, len(flash), 4096)
            if flash[offset:offset + 4096] != baseline[offset:offset + 4096]
        ],
        "main_patch_spans": changes,
        "main_stub_size": stub_size,
        "stage_sha256": digest(stage),
        "stage_address": hex(STAGE_ADDRESS),
        "stage_size": STAGE_SIZE,
        "stage_code_size": len(code),
        "stage_code_crc32": hex(crc32(code)),
        "modem_image_sha256": digest(image),
        "modem_image_offset": hex(IMAGE_OFFSET),
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock-package", metavar="TD-H9-V1.0.38.fw", type=Path, required=True)
    parser.add_argument("--base-package", type=Path, help="Pinned live-squelch KISS and resident-beacon bench build")
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    manifest = build(args.stock_package, args.output.resolve(), args.base_package)
    print(f"Built {manifest['main_stub_size']}-byte main stub and {manifest['stage_code_size']}-byte updater")
    print(f"Main package SHA-256 {manifest['main_package_sha256']}")
    print(f"Stage SHA-256 {manifest['stage_sha256']}")


if __name__ == "__main__":
    main()
