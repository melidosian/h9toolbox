#!/usr/bin/env python3
"""Build an offline TD-H9 startup-logo resource patch (no device I/O).

Stock v1.0.38's gImage_logo is the first 0xA000 bytes of resource bank 5,
whose logical base address is 0xA00000. The display reader consumes 128x160x2 bytes.
RGB565 little-endian is inferred from the Pi32 display path, not yet validated
against a dump of this radio's resource flash.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct

from PIL import Image, ImageChops, ImageOps


WIDTH = 128
HEIGHT = 160
RESOURCE_ADDRESS = 0xA00000
LOGO_SIZE = WIDTH * HEIGHT * 2


def convert(source: Path, fit: str, trim_white: bool,
            resampling: Image.Resampling = Image.Resampling.LANCZOS) -> tuple[bytes, Image.Image]:
    with Image.open(source) as opened:
        original = opened.convert("RGB")
    if trim_white:
        difference = ImageChops.difference(original, Image.new("RGB", original.size, "white"))
        bounds = difference.convert("L").point(lambda value: 255 if value > 24 else 0).getbbox()
        if bounds is None:
            raise ValueError("Image is blank after trimming white background")
        margin = max(4, int(max(bounds[2] - bounds[0], bounds[3] - bounds[1]) * 0.12))
        original = original.crop((max(0, bounds[0] - margin), max(0, bounds[1] - margin),
                                  min(original.width, bounds[2] + margin),
                                  min(original.height, bounds[3] + margin)))
    if fit == "contain":
        image = ImageOps.contain(original, (WIDTH, HEIGHT), resampling)
        canvas = Image.new("RGB", (WIDTH, HEIGHT), "white")
        canvas.paste(image, ((WIDTH - image.width) // 2, (HEIGHT - image.height) // 2))
    elif fit == "cover":
        canvas = ImageOps.fit(original, (WIDTH, HEIGHT), resampling)
    else:
        canvas = original.resize((WIDTH, HEIGHT), resampling)
    result = bytearray()
    for y in range(HEIGHT):
        for r, g, b in (canvas.getpixel((x, y)) for x in range(WIDTH)):
            pixel = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            result.extend(struct.pack("<H", pixel))
    assert len(result) == LOGO_SIZE
    return bytes(result), canvas


def ips_patch(pixels: bytes) -> bytes:
    if len(pixels) != LOGO_SIZE:
        raise ValueError("Incorrect logo length")
    return b"PATCH" + RESOURCE_ADDRESS.to_bytes(3, "big") + len(pixels).to_bytes(2, "big") + pixels + b"EOF"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--output-prefix", required=True, type=Path)
    parser.add_argument("--fit", choices=("contain", "cover", "stretch"), default="contain")
    parser.add_argument("--resample", choices=("smooth", "nearest"), default="smooth",
                        help="Use nearest for pixel art")
    parser.add_argument("--trim-white", action="store_true", help="Crop white margins before fitting")
    args = parser.parse_args()
    resampling = Image.Resampling.NEAREST if args.resample == "nearest" else Image.Resampling.LANCZOS
    pixels, preview = convert(args.image, args.fit, args.trim_white, resampling)
    raw = args.output_prefix.with_suffix(".rgb565")
    patch = args.output_prefix.with_suffix(".ips")
    preview_path = args.output_prefix.with_suffix(".preview.png")
    paths = (raw, patch, preview_path)
    for path in paths:
        if path.exists():
            parser.error(f"Refusing to overwrite {path}")
    for path in paths:
        path.parent.mkdir(parents=True, exist_ok=True)
    raw.write_bytes(pixels)
    patch.write_bytes(ips_patch(pixels))
    preview.save(preview_path)
    print(f"Logo: {WIDTH}x{HEIGHT}, RGB565 little-endian, {len(pixels)} bytes")
    print(f"Logical resource address: 0x{RESOURCE_ADDRESS:06X} (bank 5, offset 0)")
    for path in paths:
        print(path)


if __name__ == "__main__":
    main()
