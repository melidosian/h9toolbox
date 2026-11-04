"""Decode the pinned APRS v1.0.15 startup data record without modifying it.

The Thumb routine at 0x080021C0 consumes a control byte with low-nibble
literal count and high-nibble zero count. A zero nibble is followed by an
extended count byte. Both loops decrement before copying, so each encoded
count produces one fewer output byte.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

FLASH_BASE = 0x08002000
TABLE_ADDR = 0x08006358
EXPECTED_SHA256 = "1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33"
EXPECTED_RECORD = (0x08006378, 0x20000000, 0x14C, 0x080021C0)
EXPECTED_NEXT_SOURCE = 0x08006398


def decode_startup(image: bytes) -> dict[str, object]:
    digest = hashlib.sha256(image).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError(f"Unexpected APRS image SHA-256: {digest}")
    table_offset = TABLE_ADDR - FLASH_BASE
    if len(image) < table_offset + 32:
        raise ValueError("Startup initialization table is truncated")
    record = struct.unpack_from("<IIII", image, table_offset)
    next_record = struct.unpack_from("<IIII", image, table_offset + 16)
    if record != EXPECTED_RECORD or next_record[0] != EXPECTED_NEXT_SOURCE:
        raise ValueError("Unexpected startup initialization records")

    source = record[0] - FLASH_BASE
    source_end = next_record[0] - FLASH_BASE
    if source < 0 or source_end > len(image) or source >= source_end:
        raise ValueError("Compressed startup record is outside the image")
    encoded = image[source:source_end]
    offset = 0
    result = bytearray()

    def get_byte() -> int:
        nonlocal offset
        if offset >= len(encoded):
            raise ValueError("Compressed startup record ended early")
        value = encoded[offset]
        offset += 1
        return value

    while len(result) < record[2]:
        control = get_byte()
        literal_count = control & 0x0F or get_byte()
        zero_count = control >> 4 or get_byte()
        # Both ARM loops decrement before their first conditional copy.
        literal_count -= 1
        zero_count -= 1
        if literal_count < 0 or zero_count < 0:
            raise ValueError("Invalid zero run count")
        if offset + literal_count > len(encoded):
            raise ValueError("Literal run exceeds compressed record")
        if len(result) + literal_count + zero_count > record[2]:
            raise ValueError("Decoded startup data exceeds RAM record")
        result += encoded[offset:offset + literal_count]
        offset += literal_count
        result += bytes(zero_count)

    if offset != len(encoded) - 1 or encoded[-1] != 0:
        raise ValueError("Unexpected compressed-record tail")
    words = {
        f"0x{record[1] + address:08X}": struct.unpack_from("<I", result, address)[0]
        for address in range(0xC0, 0x14C, 4)
    }
    return {
        "image_sha256": digest,
        "decoded_bytes": len(result),
        "compressed_bytes_consumed": offset,
        "compressed_bytes_total": len(encoded),
        "initial_system_clock_hz": words["0x200000C0"],
        "initial_elapsed_tick": words["0x200000C4"],
        "initial_tick_auxiliary": words["0x200000C8"],
        "initial_tick_step_ms": words["0x200000CC"],
        "initialized_words": {key: value for key, value in words.items() if value},
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="pinned APRS-1.0.15.bin")
    args = parser.parse_args()
    print(json.dumps(decode_startup(args.image.read_bytes()), indent=2))


if __name__ == "__main__":
    main()
