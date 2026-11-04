#!/usr/bin/env python3
"""Render a read-only 40 KiB TD-H9 logo-region dump as RGB565 both byte orders."""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path

from PIL import Image


WIDTH = 128
HEIGHT = 160
SIZE = WIDTH * HEIGHT * 2


def render(data: bytes, byte_order: str) -> Image.Image:
    image = Image.new("RGB", (WIDTH, HEIGHT))
    pixels = []
    for offset in range(0, SIZE, 2):
        word = int.from_bytes(data[offset:offset + 2], byte_order)
        r = (word >> 11) & 31
        g = (word >> 5) & 63
        b = word & 31
        pixels.append(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    image.putdata(pixels)
    return image


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path)
    parser.add_argument("--output-prefix", required=True, type=Path)
    args = parser.parse_args()
    data = args.dump.read_bytes()
    if len(data) != SIZE:
        parser.error(f"Expected exactly {SIZE} bytes; got {len(data)}")
    print(f"Distinct byte values: {len(set(data))}; most common: {Counter(data).most_common(5)}")
    for byte_order in ("little", "big"):
        output = args.output_prefix.with_name(args.output_prefix.name + f"-{byte_order}.png")
        if output.exists():
            parser.error(f"Refusing to overwrite {output}")
        output.parent.mkdir(parents=True, exist_ok=True)
        render(data, byte_order).save(output)
        print(output)


if __name__ == "__main__":
    main()
