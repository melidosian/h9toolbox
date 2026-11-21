#!/usr/bin/env python3
"""Assemble the production renderer's PPM frames into a PNG, using only stdlib."""
from pathlib import Path
import struct
import sys
import zlib

folder = Path(sys.argv[1])
frames = []
for name in ("unified-home", "unified-aprs", "beacon-type", "messages"):
    header, width_height, maximum, pixels = (folder / f"{name}.ppm").read_bytes().split(b"\n", 3)
    if (header, width_height, maximum, len(pixels)) != (b"P6", b"160 128", b"255", 160 * 128 * 3):
        raise ValueError(f"Unexpected production frame: {name}")
    frames.append(pixels)

rows = bytearray()
for first in range(0, len(frames), 2):
    for y in range(128):
        row = bytearray(b"\0")  # PNG filter: none
        for pixels in frames[first:first + 2]:
            for x in range(160):
                offset = (y * 160 + x) * 3
                row += pixels[offset:offset + 3] * 3
        rows += row * 3

def chunk(kind, payload):
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))

png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", 960, 768, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
(folder / "unified-preview.png").write_bytes(png)
