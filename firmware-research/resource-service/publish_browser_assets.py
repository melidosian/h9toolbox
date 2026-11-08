"""Publish the merged H9 Toolbox Firmware package for the web flasher.

Copies the exact build (stage bb-s2) into dist/firmware-update/ with two
seeds, so the browser can rebuild other exact images from the hosted one:
the stock bytes of the eight sectors the build changes (exact stock v1.0.38),
and the original Brick Breaker's bytes of the seven sectors where it differs
(to recognise, back up, and upgrade radios still running it). The original
Brick Breaker package is no longer hosted; its old files are removed. The
browser checks every file by SHA-256 before it will inspect or write.
Run build.py --stage bb-s2 first (it also rebuilds the original Brick Breaker).

    python3 -I publish_browser_assets.py --stock-package <TD-H9-V1.0.38.fw>
"""

from __future__ import annotations

import argparse
from hashlib import sha256
from pathlib import Path
from shutil import copyfile
import sys

ROOT = Path(__file__).resolve().parent
SITE = ROOT.parents[1] / "dist" / "firmware-update"
sys.path.insert(0, str(ROOT.parent / "h9-flasher"))
from h9_flasher import parse_firmware  # noqa: E402

PACKAGE = ROOT / "build" / "h9_toolbox_firmware_OFFLINE_EXPERIMENT.fw"
ORIGINAL = ROOT / "build" / "brick-breaker" / "h9_brick_breaker_hotkey_OFFLINE_EXPERIMENT.fw"
ORIGINAL_PACKAGE_SHA256 = "208409d8d7e335904f70f9e28e27e9c413c6296a3d185a69a55707ac8ddaee68"
ORIGINAL_IMAGE_SHA256 = "741bbeb87eb84cd7c6988054c56a267bb73c89bbe5cc74dc4e5bc0d75ce7e193"
ORIGINAL_SECTORS = (0x5000, 0x62000, 0xAA000, 0xAC000, 0xAE000, 0xAF000, 0xD9000)
RETIRED = ("brick-breaker-v1.0.38.fw", "brick-breaker-stock-sectors.bin")
STOCK_PACKAGE_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
STOCK_IMAGE_SHA256 = "74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648"
PACKAGE_SHA256 = "b9d24cb43937621358053356382ab72608bfbab677468febe4db867242098dc5"
IMAGE_SHA256 = "83f79fb3281c37ab22a4fd3f154a6c52a50d3cf0d8063b546eb47ce3c603bac6"
SECTORS = (0x5000, 0x62000, 0x8D000, 0xAA000, 0xAC000, 0xAE000, 0xAF000, 0xD9000)
SECTOR_SIZE = 4096


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stock-package", type=Path, required=True)
    args = parser.parse_args()
    if sha256(args.stock_package.read_bytes()).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the exact official v1.0.38 .fw package")
    if sha256(PACKAGE.read_bytes()).hexdigest() != PACKAGE_SHA256:
        raise ValueError(f"Unexpected package; rebuild with build.py --stage bb-s2: {PACKAGE}")
    stock = parse_firmware(args.stock_package).image
    build = parse_firmware(PACKAGE).image
    if sha256(stock).hexdigest() != STOCK_IMAGE_SHA256 or sha256(build).hexdigest() != IMAGE_SHA256:
        raise ValueError("Embedded image SHA-256 mismatch")
    if stock[:0x5000] != build[:0x5000]:
        raise ValueError("Protected boot bytes differ")
    changed = tuple(offset for offset in range(0x5000, len(stock), SECTOR_SIZE)
                    if stock[offset:offset + SECTOR_SIZE] != build[offset:offset + SECTOR_SIZE])
    if changed != SECTORS:
        raise ValueError(f"Unexpected changed sectors: {[hex(x) for x in changed]}")
    seed = b"".join(stock[offset:offset + SECTOR_SIZE] for offset in SECTORS)
    rebuilt = bytearray(build)
    for index, offset in enumerate(SECTORS):
        rebuilt[offset:offset + SECTOR_SIZE] = seed[index * SECTOR_SIZE:(index + 1) * SECTOR_SIZE]
    if bytes(rebuilt) != stock:
        raise ValueError("Stock seed does not exactly reconstruct stock")

    if sha256(ORIGINAL.read_bytes()).hexdigest() != ORIGINAL_PACKAGE_SHA256:
        raise ValueError(f"Unexpected original Brick Breaker package: {ORIGINAL}")
    original = parse_firmware(ORIGINAL).image
    if sha256(original).hexdigest() != ORIGINAL_IMAGE_SHA256:
        raise ValueError("Original Brick Breaker image SHA-256 mismatch")
    differs = tuple(offset for offset in range(0, len(build), SECTOR_SIZE)
                    if original[offset:offset + SECTOR_SIZE] != build[offset:offset + SECTOR_SIZE])
    if differs != ORIGINAL_SECTORS:
        raise ValueError(f"Unexpected original Brick Breaker sectors: {[hex(x) for x in differs]}")
    previous = b"".join(original[offset:offset + SECTOR_SIZE] for offset in ORIGINAL_SECTORS)
    rebuilt = bytearray(build)
    for index, offset in enumerate(ORIGINAL_SECTORS):
        rebuilt[offset:offset + SECTOR_SIZE] = previous[index * SECTOR_SIZE:(index + 1) * SECTOR_SIZE]
    if bytes(rebuilt) != original:
        raise ValueError("Original sectors do not exactly reconstruct the original Brick Breaker")

    copyfile(PACKAGE, SITE / "h9-toolbox-firmware-v1.0.38.fw")
    (SITE / "h9-toolbox-firmware-stock-sectors.bin").write_bytes(seed)
    (SITE / "brick-breaker-original-sectors.bin").write_bytes(previous)
    for name in RETIRED:
        (SITE / name).unlink(missing_ok=True)
    print(f"Package: {PACKAGE.stat().st_size} bytes, SHA-256 {PACKAGE_SHA256}")
    print(f"Stock seed: {len(seed)} bytes, SHA-256 {sha256(seed).hexdigest()}")
    print(f"Original Brick Breaker sectors: {len(previous)} bytes, SHA-256 {sha256(previous).hexdigest()}")


if __name__ == "__main__":
    main()
