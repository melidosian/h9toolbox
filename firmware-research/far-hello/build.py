#!/usr/bin/env python3
"""Build the far-region test: C-hello running from main flash 0xEA000,
launched from H9 Toolbox Firmware by holding Back (the Brick Breaker key).

Outputs, flashed separately in hold-8 mode:
  - h9_far_hello_0xEA000_unencrypted.bin / _encrypted.bin: the program in
    the two candidate on-flash forms, for `h9_flasher.py write-far`. Flash 0xEA000 is outside the settings VM (two
    32 KiB banks at 0xDA000) and maps to 0x01EE5000.
  - h9_far_hello_OFFLINE_EXPERIMENT.fw: H9 Toolbox Firmware with two changes:
    a 24-byte stub in the spare tail of the .tables cave (app+0xA7E80), and
    the Back-hold launcher's jump target (app+0xA9DD4) pointed at the stub.
    The stub enters the far program only if flash 0xEA000 reads back as the
    "H9X1" magic through the CPU's mapping; otherwise Brick Breaker starts as
    before. Bluetooth and the resource-flash service are unchanged.

Hypothesis under test: the flash mapping decrypts beyond the app with the
same address-keyed stream as the app area (key = 0xF181 ^ ((flash - 0x5000)
>> 2) per 32 bytes). If not, the magic check fails and Brick Breaker runs.

    export JIELI_TOOLCHAIN=~/path/to/jieli-linux-toolchains-20260730.1
    python3 -I build.py --stock-package <TD-H9-V1.0.38.fw>
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
C_HELLO = RESEARCH / "c-hello"
BASE_RAW = RESEARCH / "resource-service" / "build" / "h9_toolbox_firmware_NOT_FOR_FLASH.raw"
BASE_PACKAGE = RESEARCH / "resource-service" / "build" / "h9_toolbox_firmware_OFFLINE_EXPERIMENT.fw"
BASE_APP_SHA256 = "1334a32c9ec3191ea0c8ca3f657331a5c5ce110a71185db1d075e7cd5d4a5ead"
BASE_FLASH_SHA256 = "83f79fb3281c37ab22a4fd3f154a6c52a50d3cf0d8063b546eb47ce3c603bac6"
STOCK_PACKAGE_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
IMAGE_BASE = 0x01E00100
APP_FLASH = 0x5100          # app.bin offset 0 sits at flash 0x5100
APP_AREA = 0x5000           # encryption stream origin
CHIP_KEY = 0xF181
FAR_FLASH = 0xEA000
FAR_ADDRESS = 0x01EE5000    # 0x01E00000 + (flash - 0x5000)
FAR_LIMIT = 0xFC000 - FAR_FLASH
MAGIC = 0x31583948          # "H9X1"
STUB_OFFSET = 0xA7E80       # spare tail of the .tables cave (24 bytes to 0xA7E98)
STUB_SIZE = 24
LAUNCHER_JUMP = 0xA9DD2     # r3 = brick_breaker_after_stock_setup; goto r3
LAUNCHER_ORIGINAL = bytes.fromhex("c3ff4279ea01d300")
BRICK_BREAKER_ENTRY = 0x01EA7942
SECTORS_FROM_STOCK = (0x5000, 0x62000, 0x8D000, 0xAA000, 0xAC000, 0xAE000, 0xAF000, 0xD9000)
SECTORS_FROM_BASE = (0x5000, 0xAC000, 0xAE000, 0xD9000)  # stub, launcher, app CRC, metadata
C_FLAGS = ["-target", "pi32v2", "-mcpu=r3", "-Oz", "-integrated-as",
           "-ffreestanding", "-fno-builtin", "-fno-common", "-Wall", "-Werror"]
# Split point in c-hello/entry.s: after the stock controller setup, where the
# Brick Breaker launcher also enters (the radio has already run that setup).
SPLIT = "    r0 = 1\n    r3 = 0x01e4c2da\n    call r3\n"
HEADER = f"""    .section .far_header,"ax",@progbits
    .globl far_header
far_header:
    .long 0x{MAGIC:08x}
    .long 0
    r3 = far_after_stock_setup
    goto r3
"""


def xor_jieli(data: bytes, key: int) -> bytes:
    out = bytearray(data)
    for i in range(len(out)):
        out[i] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF
    return bytes(out)


def flash_form(plain: bytes, flash: int) -> bytes:
    """App-area stream cipher (symmetric) for bytes placed at `flash`."""
    if flash % 32:
        raise ValueError("Encrypted placement must be 32-byte aligned")
    padded = plain + b"\xff" * (-len(plain) % 32)
    return b"".join(xor_jieli(padded[o:o + 32], CHIP_KEY ^ ((flash + o - APP_AREA) >> 2))
                    for o in range(0, len(padded), 32))


def section(program, name):
    return next(s for s in program.sections if s.name == name)


def link(ld, elf, script: Path, output: Path, objects: list[Path]):
    state, data = ld.link(ld.parse_args(["-T", str(script), "-o", str(output), *map(str, objects)]))
    if data is None or state.errors:
        raise RuntimeError(f"Pi32 linker failed: {state.errors}")
    output.write_bytes(data)
    return elf.read(data)


def build(stock_package: Path, output: Path) -> dict:
    if sha256(stock_package.read_bytes()).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the exact official v1.0.38 .fw package")
    base = BASE_RAW.read_bytes()
    if sha256(base).hexdigest() != BASE_APP_SHA256:
        raise ValueError("H9 Toolbox Firmware base app differs from the pinned build (run resource-service/build.py --stage bb-s2)")
    if base[STUB_OFFSET:STUB_OFFSET + STUB_SIZE] != b"\xff" * STUB_SIZE:
        raise ValueError("Stub space is not erased in the base app")
    if base[LAUNCHER_JUMP:LAUNCHER_JUMP + 8] != LAUNCHER_ORIGINAL:
        raise ValueError("Launcher jump differs from the pinned Brick Breaker target")
    output.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm, elf, ld  # noqa: E402

    # Far program: header + C-hello startup (from the split) + C-hello main.c.
    entry = (C_HELLO / "entry.s").read_text(encoding="utf-8")
    if entry.count(SPLIT) != 1:
        raise ValueError("c-hello/entry.s split point not found exactly once")
    entry = HEADER + entry.replace(SPLIT, SPLIT + "    .globl far_after_stock_setup\nfar_after_stock_setup:\n")
    entry_obj = output / "far_entry.o"
    entry_obj.write_bytes(asm.assemble_source(entry, filename="far_entry.s"))
    c_obj = output / "main.o"
    result = subprocess.run([str(H9CC), "clang", *C_FLAGS, "-c", "c-hello/main.c",
                             "-o", str(c_obj.relative_to(RESEARCH))],
                            cwd=RESEARCH, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"JieLi clang failed:\n{result.stdout}{result.stderr}")
    c_program = elf.read(c_obj.read_bytes())
    data_sections = [s.name for s in c_program.sections
                     if s.name.startswith((".data", ".bss", ".sdata", ".sbss")) and len(s.data or b"")]
    if data_sections:
        raise ValueError(f"C code has data sections (no RAM is set aside for them): {data_sections}")
    far = link(ld, elf, ROOT / "far.ld", output / "h9_far_hello.elf", [entry_obj, c_obj])
    header, text = section(far, ".far_header"), section(far, ".text")
    if header.addr != FAR_ADDRESS or len(header.data) != 16 or text.addr != FAR_ADDRESS + 16:
        raise ValueError("Far program layout differs from header (16 bytes) + text at 0x01EE5000")
    if far.entry != FAR_ADDRESS:
        raise ValueError("Far program entry is not its header")
    plain = bytes(header.data) + bytes(text.data)
    if len(plain) > FAR_LIMIT:
        raise ValueError(f"Far program ({len(plain)} bytes) exceeds the far region")
    if int.from_bytes(plain[:4], "little") != MAGIC:
        raise ValueError("Far header does not start with the magic")
    symbols = {s.name: s.value for s in far.symbols if s.name}
    blob = flash_form(plain, FAR_FLASH)
    if flash_form(blob, FAR_FLASH)[:len(plain)] != plain:
        raise ValueError("Far encryption does not round-trip")
    # Two candidate on-flash forms for write-far. Hardware run 1 (encrypted)
    # fell back to Brick Breaker, so the magic did not read back that way.
    (output / "h9_far_hello_0xEA000_encrypted.bin").write_bytes(blob)
    (output / "h9_far_hello_0xEA000_unencrypted.bin").write_bytes(plain)

    # Stub and launcher retarget in H9 Toolbox Firmware.
    stub_obj = output / "stub.o"
    stub_obj.write_bytes(asm.assemble_source((ROOT / "stub.s").read_text(encoding="utf-8"), filename="stub.s"))
    stub_program = link(ld, elf, ROOT / "stub.ld", output / "stub.elf", [stub_obj])
    stub = bytes(section(stub_program, ".text").data)
    if section(stub_program, ".text").addr != IMAGE_BASE + STUB_OFFSET or len(stub) != STUB_SIZE:
        raise ValueError("Stub does not fill the 24-byte spare space exactly")
    patched = bytearray(base)
    patched[STUB_OFFSET:STUB_OFFSET + STUB_SIZE] = stub
    patched[LAUNCHER_JUMP + 2:LAUNCHER_JUMP + 6] = (IMAGE_BASE + STUB_OFFSET).to_bytes(4, "little")
    raw = output / "h9_far_hello_NOT_FOR_FLASH.raw"
    raw.write_bytes(patched)

    package = output / "h9_far_hello_OFFLINE_EXPERIMENT.fw"
    rebuilt = output / "h9_far_hello_REBUILD_CHECK.fw"
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
    stock = parse_firmware(stock_package).image
    base_target = parse_firmware(BASE_PACKAGE).image
    target = parse_firmware(package).image
    if sha256(base_target).hexdigest() != BASE_FLASH_SHA256:
        raise ValueError("H9 Toolbox Firmware package differs from the pinned build")
    differs = lambda a, b: tuple(o for o in range(0, len(a), 4096) if a[o:o + 4096] != b[o:o + 4096])
    if differs(stock, target) != SECTORS_FROM_STOCK:
        raise ValueError(f"Unexpected sectors vs stock: {[hex(x) for x in differs(stock, target)]}")
    if differs(base_target, target) != SECTORS_FROM_BASE:
        raise ValueError(f"Unexpected sectors vs H9 Toolbox: {[hex(x) for x in differs(base_target, target)]}")
    # Our cipher must reproduce the repacker's on-flash bytes for the stub blocks.
    start = (APP_FLASH + STUB_OFFSET) & ~31
    end = (APP_FLASH + STUB_OFFSET + STUB_SIZE + 31) & ~31
    if flash_form(bytes(patched[start - APP_FLASH:end - APP_FLASH]), start) != target[start:end]:
        raise ValueError("Cipher check against the repacked app failed")
    manifest = {
        "kind": "far_region_c_hello_experiment",
        "base": "H9 Toolbox Firmware (resource-service bb-s2)",
        "base_flash_sha256": BASE_FLASH_SHA256,
        "far_flash_offset": hex(FAR_FLASH),
        "far_cpu_address": hex(FAR_ADDRESS),
        "far_plain_bytes": len(plain),
        "far_plain_sha256": sha256(plain).hexdigest(),
        "far_encrypted_sha256": sha256(blob).hexdigest(),
        "far_unencrypted_sha256": sha256(plain).hexdigest(),
        "far_h9_main": hex(symbols["h9_main"]),
        "stub_offset": hex(STUB_OFFSET),
        "stub_hex": stub.hex(),
        "launcher_jump_offset": hex(LAUNCHER_JUMP),
        "launcher_old_target": hex(BRICK_BREAKER_ENTRY),
        "launcher_new_target": hex(IMAGE_BASE + STUB_OFFSET),
        "c_object_sha256": sha256(c_obj.read_bytes()).hexdigest(),
        "app_sha256": sha256(patched).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target).hexdigest(),
        "changed_flash_sectors_vs_stock": [hex(o) for o in SECTORS_FROM_STOCK],
        "changed_flash_sectors_vs_h9_toolbox": [hex(o) for o in SECTORS_FROM_BASE],
        "hardware_observation": "Not yet flashed",
    }
    (output / "h9_far_hello-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    print(json.dumps(build(args.stock_package.resolve(), args.output.resolve()), indent=2))
    print("Offline build only; not hardware tested")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
