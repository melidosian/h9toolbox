#!/usr/bin/env python3
"""Build an offline candidate with the verified RC4 menu and on-radio modem updater."""
import argparse
import json
from pathlib import Path

from build import (APP_BASE, KISS, ROOT, app_bytes, checked_bytes, compile_stage,
                   digest, parse_firmware_bytes, patch_app, patch_modem, rebuild)

BASE_PACKAGE_SHA = "8e7cb1fc97605704665bb7fa0add10fb83ef4c0950189e0920200193a6e189ce"
BASE_FAR_SHA = "83228873b940b95d39adb8150bd42add25799be05e6567749816922dd9019445"
STOCK_SHA = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
FAR_START, STAGE_START, RESIDENT_START, FAR_END = 0xEA000, 0xF7A00, 0xFA000, 0xFC000
STAGE_ENTRY = 0x01E00000 + STAGE_START - 0x5000
STAGE_CODE_LIMIT = 0x800
# Reclaimed native menu functions. Preserve the redirect at each entry point.
STUB_START, STUB_END = 0x6812E, 0x682CC
IMAGE_PARTS = ((0x6F368, 4800), (0x734A6, 6000))
RC4_HEAP_END = 0x29900
RC4_VERSION = 0x04000106
COMBINED_VERSION = 0x05000106


def build_combined(base: Path, stock_path: Path, output: Path) -> dict:
    package = checked_bytes(base / "h9_toolbox_modules_OFFLINE_EXPERIMENT.fw", BASE_PACKAGE_SHA)
    original_flash = parse_firmware_bytes(package).image
    original = app_bytes(original_flash)
    stock = checked_bytes(stock_path, STOCK_SHA)
    far = bytearray(checked_bytes(base / "unified-far-0xEA000.bin", BASE_FAR_SHA))
    stage_start = STAGE_START - FAR_START
    stage_end = RESIDENT_START - FAR_START
    if far[stage_start:stage_end] != b"\xff" * (stage_end - stage_start):
        raise ValueError("The relocated modem stage overlaps the RC4 menu")
    image = patch_modem((KISS / "APRS-1.0.15-rx15-rxff.bin").read_bytes())
    output.mkdir(parents=True, exist_ok=True)

    code = compile_stage(output, image, RC4_HEAP_END - 0x100, entry=STAGE_ENTRY,
                         image_offset=STAGE_CODE_LIMIT,
                         split=tuple((APP_BASE + start, size) for start, size in IMAGE_PARTS))
    version_at = original.index(b"H9AP") + 24
    patched, stub_size, hooks = patch_app(
        original, code, output, RC4_HEAP_END, version_at, COMBINED_VERSION,
        stage_entry=STAGE_ENTRY, cave_offset=STUB_START, cave_end=STUB_END,
        cave_fill=b"\x00", old_version=RC4_VERSION, heap_opcode=bytes.fromhex("c5ff"),
    )
    app = bytearray(patched)
    image_offset = 0
    for start, size in IMAGE_PARTS:
        if app[start:start + size] != bytes(size):
            raise ValueError(f"Modem image would overwrite main app code at {start:#x}")
        app[start:start + size] = image[image_offset:image_offset + size]
        image_offset += size
    tail_start = stage_start + STAGE_CODE_LIMIT
    tail = image[image_offset:]
    if tail_start + len(tail) > stage_end:
        raise ValueError("Modem image does not fit before the APRS scheduler")
    far[stage_start:stage_start + len(code)] = code
    far[tail_start:tail_start + len(tail)] = tail

    combined = rebuild(stock, bytes(app))
    combined_flash = parse_firmware_bytes(combined).image
    decoded = app_bytes(combined_flash)
    recovered = b"".join(decoded[start:start + size] for start, size in IMAGE_PARTS)
    recovered += far[tail_start:tail_start + len(tail)]
    if (decoded != app or recovered != image or combined_flash[:0x5000] != original_flash[:0x5000]
            or len(combined_flash) != len(original_flash)):
        raise ValueError("Repacked firmware or modem image did not round-trip")
    far.extend(b"\xff" * (FAR_END - FAR_START - len(far)))
    manifest = {
        "version": "0.1.6-rc.5",
        "base_package_sha256": BASE_PACKAGE_SHA,
        "package_sha256": digest(combined),
        "image_sha256": digest(combined_flash),
        "far_sha256": digest(far),
        "stage_address": hex(STAGE_START),
        "stage_code_bytes": len(code),
        "stage_stub_bytes": stub_size,
        "main_hook_spans": hooks,
        "modem_image_sha256": digest(image),
    }
    (output / "h9-toolbox-firmware-v0.1.6-rc.5.fw").write_bytes(combined)
    (output / "h9-toolbox-v0.1.6-rc.5-far.bin").write_bytes(far)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rc4", type=Path)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "combined")
    args = parser.parse_args()
    print(json.dumps(build_combined(args.rc4, args.stock_package, args.output.resolve()), indent=2))
