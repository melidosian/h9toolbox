#!/usr/bin/env python3
"""Stage a unified-menu candidate with the on-radio modem updater."""
import argparse
import json
import re
from pathlib import Path

from build import (APP_BASE, KISS, ROOT, app_bytes, checked_bytes, compile_stage,
                   digest, parse_firmware_bytes, patch_app, patch_modem, rebuild)

BASE_PACKAGE_SHA = "8e7cb1fc97605704665bb7fa0add10fb83ef4c0950189e0920200193a6e189ce"
BASE_FAR_SHA = "83228873b940b95d39adb8150bd42add25799be05e6567749816922dd9019445"
STOCK_SHA = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
FAR_START, STAGE_START, RESIDENT_START, FAR_END = 0xEA000, 0xF7A00, 0xFA000, 0xFC000
STAGE_CODE_LIMIT = 0x800
# Reclaimed native menu functions. Preserve the redirect at each entry point.
STUB_START, STUB_END = 0x6812E, 0x682CC
# Reserve the checked entry before using the rest of its retired function.
STUB_SPILL_START = STUB_START + 0xD0
SPILL_CODE_LIMIT = 0x660
FIVE_PART_STAGE_BYTES = 0x644
SIX_PART_STAGE_BYTES = 0x658
EIGHT_PART_STAGE_BYTES = 0x684
# A complete CW-only span cleared by morse_disable.py on unified builds.
MORSE_SPILL = (0x76BCE, 0x76CDC - 0x76BCE)
# Additional retired CW spans before the KISS helper and native mode dispatch.
EXTRA_MORSE_SPILL = ((0x84E98, 0x84F88 - 0x84E98), (0x76AA6, 0x76B52 - 0x76AA6))
# Retired CW tail after the relocated dev KISS helper.
FINAL_MORSE_SPILL = (0x85586, 0x85666 - 0x85586)
IMAGE_PARTS = ((0x6F368, 4800), (0x734A6, 6000))
RC4_HEAP_END = 0x29900
RC4_VERSION = 0x04000106
RC5_VERSION = (0, 1, 6, 5)


def version_parts(version: str) -> tuple[int, int, int, int]:
    match = re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-rc\.([1-9][0-9]*))?", version)
    if not match or any(int(part) > 255 for part in match.groups() if part is not None):
        raise ValueError(f"Invalid firmware version {version!r}")
    major, minor, patch, candidate = match.groups()
    return int(major), int(minor), int(patch), int(candidate) if candidate else 256


def version_word(parts: tuple[int, int, int, int]) -> int:
    major, minor, patch, candidate = parts
    return (candidate & 0xff) << 24 | major << 16 | minor << 8 | patch


def build_combined(base: Path, stock_path: Path, output: Path, version: str | None = None) -> dict:
    stage_address, heap_end, old_version = STAGE_START, RC4_HEAP_END, RC4_VERSION
    package_sha, far_sha = BASE_PACKAGE_SHA, BASE_FAR_SHA
    image_parts, code_limit = IMAGE_PARTS, STAGE_CODE_LIMIT
    if version is not None:
        # The unified build's manifest identifies the exact app, menu and
        # scheduler. Keep the pinned RC4 workflow for reproducing RC5.
        loader = json.loads((base / "h9_toolbox_modules-manifest.json").read_text())
        firmware, menu = loader["firmware"], loader["far_menu"]
        resident = firmware["resident_beacon"]
        package_sha = firmware["package_sha256"]
        heap_end = int(firmware["heap_end"], 16)
        base_version = version_parts(firmware["firmware_version"])
        combined_version = version_parts(version)
        old_version = version_word(base_version)
        if (int(menu["flash_offset"], 16) != FAR_START or
                int(resident["flash_offset"], 16) != RESIDENT_START or heap_end != RC4_HEAP_END or
                not firmware["unified_menu"] or combined_version <= RC5_VERSION or
                combined_version < base_version or (combined_version == base_version and base_version[-1] != 256)):
            raise ValueError("Unexpected unified base layout or candidate version")
        menu_bytes = checked_bytes(base / "menu_0xEA000_OFFLINE.bin", menu["sha256"])
        resident_bytes = checked_bytes(base / "resident_0xFA000_OFFLINE.bin", resident["sha256"])
        if (len(menu_bytes) != menu["bytes"] or len(menu_bytes) > RESIDENT_START - FAR_START or
                len(resident_bytes) != resident["body_bytes"] or len(resident_bytes) > FAR_END - RESIDENT_START):
            raise ValueError("Unified payload lengths differ from the manifest or reserved windows")
        far_sha = digest(menu_bytes.ljust(RESIDENT_START - FAR_START, b"\xff") + resident_bytes)
        stage_address = max(STAGE_START, (FAR_START + len(menu_bytes) + 3) & ~3)
        # Use the retired category function after its redirect, and reserve
        # only the space needed by the relocated updater's checked entry.
        image_parts = (
            (0x6F360, 0x70650 - 0x6F360),
            (0x7349E, 0x74C32 - 0x7349E),
            (0x6ECB6, 0x6EEB2 - 0x6ECB6),
        )
        code_limit = 0x630

    package = checked_bytes(base / "h9_toolbox_modules_OFFLINE_EXPERIMENT.fw", package_sha)
    original_flash = parse_firmware_bytes(package).image
    original = app_bytes(original_flash)
    stock = checked_bytes(stock_path, STOCK_SHA)
    far = bytearray(checked_bytes(base / "unified-far-0xEA000.bin", far_sha))
    if len(far) > FAR_END - FAR_START:
        raise ValueError("Unified payload exceeds the far flash window")
    stage_start = stage_address - FAR_START
    stage_end = RESIDENT_START - FAR_START
    image = patch_modem((KISS / "APRS-1.0.15-rx15-rxff.bin").read_bytes())
    extra_start = (len(far) + 3) & ~3
    image_offset = sum(size for _, size in image_parts)
    tail_start = stage_start + code_limit
    overflow = max(0, len(image) - image_offset - (stage_end - tail_start))
    cave_end = STUB_END
    if version is not None and extra_start + overflow > FAR_END - FAR_START:
        cave_end = STUB_SPILL_START
        spill_size = STUB_END - cave_end
        image_parts += ((cave_end, spill_size),)
        image_offset += spill_size
        # Keep the usual staging reservation if it fits. Otherwise give
        # its unused aligned tail to the remaining modem bytes.
        available_code = stage_end - stage_start + image_offset + FAR_END - FAR_START - extra_start - len(image)
        code_limit = SPILL_CODE_LIMIT
        if available_code < FIVE_PART_STAGE_BYTES:
            image_parts += (MORSE_SPILL,)
            image_offset += MORSE_SPILL[1]
            available_code += MORSE_SPILL[1]
            code_limit = 0x680
            if available_code < SIX_PART_STAGE_BYTES:
                image_parts += EXTRA_MORSE_SPILL
                extra_size = sum(size for _, size in EXTRA_MORSE_SPILL)
                image_offset += extra_size
                available_code += extra_size
                code_limit = 0x6C0
                if available_code < EIGHT_PART_STAGE_BYTES:
                    # Replace the smaller retired span to keep the same
                    # number of modem fragments when the far menu grows.
                    extra = FINAL_MORSE_SPILL[1] - image_parts[-1][1]
                    image_parts = image_parts[:-1] + (FINAL_MORSE_SPILL,)
                    image_offset += extra
                    available_code += extra
        code_limit = min(code_limit, available_code & ~3)
        if code_limit <= 0:
            raise ValueError("No aligned space remains for the relocated modem stage")
        tail_start = stage_start + code_limit
        overflow = max(0, len(image) - image_offset - (stage_end - tail_start))
    if tail_start > stage_end or far[stage_start:stage_end] != b"\xff" * (stage_end - stage_start):
        raise ValueError("The relocated modem stage overlaps the unified menu")
    if extra_start + overflow > FAR_END - FAR_START:
        raise ValueError("Modem image does not fit alongside the unified menu and scheduler")
    if overflow:
        far.extend(b"\xff" * (extra_start - len(far)))
        far.extend(image[image_offset:image_offset + overflow])

    output.mkdir(parents=True, exist_ok=True)
    stage_entry = 0x01E00000 + stage_address - 0x5000
    split = tuple((APP_BASE + start, size) for start, size in image_parts)
    if overflow:
        split += ((0x01E00000 + FAR_START + extra_start - 0x5000, overflow),)
    code = compile_stage(output, image, heap_end - 0x100, entry=stage_entry,
                         image_offset=code_limit, split=split)
    version = version or "0.1.6-rc.5"
    version_at = original.index(b"H9AP") + 24
    patched, stub_size, hooks = patch_app(
        original, code, output, heap_end, version_at, version_word(version_parts(version)),
        stage_entry=stage_entry, cave_offset=STUB_START, cave_end=cave_end,
        cave_fill=b"\x00", old_version=old_version, heap_opcode=bytes.fromhex("c5ff"),
    )
    app = bytearray(patched)
    image_offset = 0
    for start, size in image_parts:
        if app[start:start + size] != bytes(size):
            raise ValueError(f"Modem image would overwrite main app code at {start:#x}")
        app[start:start + size] = image[image_offset:image_offset + size]
        image_offset += size
    tail = image[image_offset + overflow:]
    far[stage_start:stage_start + len(code)] = code
    far[tail_start:tail_start + len(tail)] = tail

    combined = rebuild(stock, bytes(app))
    combined_flash = parse_firmware_bytes(combined).image
    decoded = app_bytes(combined_flash)
    recovered = b"".join(decoded[start:start + size] for start, size in image_parts)
    recovered += far[extra_start:extra_start + overflow]
    recovered += far[tail_start:tail_start + len(tail)]
    if (decoded != app or recovered != image or combined_flash[:0x5000] != original_flash[:0x5000]
            or len(combined_flash) != len(original_flash)):
        raise ValueError("Repacked firmware or modem image did not round-trip")
    far.extend(b"\xff" * (FAR_END - FAR_START - len(far)))
    manifest = {
        "version": version,
        "base_package_sha256": package_sha,
        "package_sha256": digest(combined),
        "image_sha256": digest(combined_flash),
        "far_sha256": digest(far),
        "stage_address": hex(stage_address),
        "stage_code_bytes": len(code),
        "stage_stub_bytes": stub_size,
        "main_hook_spans": hooks,
        "modem_image_sha256": digest(image),
    }
    (output / f"h9-toolbox-firmware-v{version}.fw").write_bytes(combined)
    (output / f"h9-toolbox-v{version}-far.bin").write_bytes(far)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rc4", type=Path)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "combined")
    parser.add_argument("--version", help="package a verified unified build with its release version")
    args = parser.parse_args()
    print(json.dumps(build_combined(args.rc4, args.stock_package, args.output.resolve(), args.version), indent=2))
