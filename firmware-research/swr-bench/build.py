#!/usr/bin/env python3
"""Build the offline-only Type-C TD-H9 SWR bench firmware."""

from __future__ import annotations

import argparse
from hashlib import sha256
import importlib.util
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parent
RESEARCH = ROOT.parent
IMAGE_BASE = 0x01E00100
CAVE = 0x0A77F0
CAVE_SIZE = 1704
HEAP_END_SITE = 0x05FA9E
LOG_RAM = 0x2B700
USED_RAM_END = 0x2BE04
PACKAGE_NAME = "h9-swr-bench-OFFLINE.fw"

CONTEXTS = {
    0x088728: "c1ffc896e801 4020 422a 4320 8bea66d8",
    0x0895C8: "7504 c4ffd01a0100 50ee4705",
    0x05D010: "d11d 8a83 4024 4321 bfeacbad",
    HEAP_END_SITE: "c5ff00bf0200",
}
HOOKS = {
    0x088734: "swr_bench_register",
    0x05D018: "swr_bench_read",
}


def read_loader():
    spec = importlib.util.spec_from_file_location(
        "h9_module_builder", RESEARCH / "module-loader" / "build.py")
    loader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loader)
    return loader


def build(stock_image: Path, stock_package: Path, toolchain: Path, output: Path) -> dict:
    loader = read_loader()
    stock = stock_image.read_bytes()
    if sha256(stock).hexdigest() != loader.STOCK_SHA256:
        raise ValueError("Expected the pinned Type-C v1.0.38 app.bin")
    if sha256(stock_package.read_bytes()).hexdigest() != loader.STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the pinned Type-C v1.0.38 .fw package")
    package = output / PACKAGE_NAME
    if package.exists():
        raise ValueError(f"Choose a new output directory. Package already exists at {package}")

    for offset, text in CONTEXTS.items():
        expected = bytes.fromhex(text)
        if stock[offset:offset + len(expected)] != expected:
            raise ValueError(f"Stock instructions differ at app+0x{offset:X}")
    if stock[CAVE:CAVE + CAVE_SIZE] != b"\xff" * CAVE_SIZE:
        raise ValueError("Recorder code cave is occupied")
    # Check the portion of the old heap used by the probe for stock literals.
    # The higher stock references near 0x2BEE0 stay outside that portion.
    for offset in range(0, len(stock) - 3, 2):
        address = struct.unpack_from("<I", stock, offset)[0]
        if LOG_RAM <= address < USED_RAM_END:
            raise ValueError(f"Stock literal at app+0x{offset:X} points into recorder RAM")

    output.mkdir(parents=True, exist_ok=True)
    compiler = toolchain / "pi32v2" / "bin" / "clang"
    objects = []
    for name in ("bench", "capture"):
        obj = output / (name + ".o")
        loader.run([str(compiler), *loader.C_FLAGS, "-c", str(ROOT / (name + ".c")), "-o", str(obj)])
        objects.append(obj)
    origin = IMAGE_BASE + CAVE
    linked = loader.link(objects, origin, "swr_bench_register", output, "swr_bench",
                         keep=("swr_bench_read",))
    body = loader.text_of(linked, origin)
    if not body or len(body) > CAVE_SIZE:
        raise ValueError(f"Recorder code size {len(body)} exceeds {CAVE_SIZE} bytes")

    app = bytearray(stock)
    app[CAVE:CAVE + len(body)] = body
    app[HEAP_END_SITE + 2:HEAP_END_SITE + 6] = struct.pack("<I", LOG_RAM)
    hooks = {}
    for offset, name in HOOKS.items():
        target = loader.symbol(linked, name) - IMAGE_BASE
        code = loader.call_bytes(target, offset)
        app[offset:offset + len(code)] = code
        hooks[hex(offset)] = {"target": name, "bytes": code.hex()}

    raw = output / "app-bench.raw"
    raw.write_bytes(app)
    loader.run([sys.executable, str(RESEARCH / "h9-firmware-research" / "firmware.py"),
                "repack", str(stock_package), "--app", str(raw), "--output", str(package)])

    sys.path.insert(0, str(RESEARCH / "h9-flasher"))
    from h9_flasher import parse_firmware
    original = parse_firmware(stock_package).image
    updated = parse_firmware(package).image
    changed = [offset for offset in range(0, len(original), 4096)
               if original[offset:offset + 4096] != updated[offset:offset + 4096]]
    expected = sorted({0x5000, 0xD9000, *(
        (offset + 0x5100) & ~0xFFF for offset in (CAVE, HEAP_END_SITE, *HOOKS)
    )})
    if changed != expected:
        raise ValueError(f"Unexpected main-flash changes: {list(map(hex, changed))}")
    manifest = {
        "kind": "td-h9-type-c-1.0.38-swr-bench",
        "hardware_tested": False,
        "stock_app_sha256": loader.STOCK_SHA256,
        "app_sha256": sha256(app).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(updated).hexdigest(),
        "changed_flash_sectors": [f"0x{offset:05X}" for offset in changed],
        "code_bytes": len(body),
        "hooks": hooks,
        "capture_ram": f"0x{LOG_RAM:05X}",
        "capture_clone_window": "0x3C00..0x425F",
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock-image", required=True, type=Path)
    parser.add_argument("--stock-package", required=True, type=Path)
    parser.add_argument("--toolchain", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.stock_image.resolve(), args.stock_package.resolve(),
                           args.toolchain.resolve(), args.output.resolve()), indent=2))


if __name__ == "__main__":
    main()
