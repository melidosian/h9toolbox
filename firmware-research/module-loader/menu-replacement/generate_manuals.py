"""Reference the stock manual QR pixels and restore their reused white margins."""
from hashlib import sha256
from pathlib import Path
import sys

STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
APP_BASE = 0x01E00100
IMAGE_BYTES = 70 * 70 * 2
# Each tuple gives the asset start and the bounds of its unpatched pixels.
PAGES = (
    (0xA2EB2, 0xA2EB2, 0xA51A8),
    (0xA54FA, 0xA54FA, 0xA77F0),
    (0xA7B42, 0xA7E98, 0xA9D9E),
)


def verify_pixels(stock, patched):
    for start, first, last in PAGES:
        end = start + IMAGE_BYTES
        restored = b"\xff" * (first - start) + patched[first:last] + b"\xff" * (end - last)
        if restored != stock[start:end]:
            raise ValueError(f"Manual QR at app+{start:#x} differs after margin restoration")


def generate(stock):
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected pinned factory v1.0.38")
    verify_pixels(stock, stock)
    lines = [
        "/* Generated from the three factory manual QR assets. */",
        "#ifndef RADIO_MANUAL_IMAGES_H",
        "#define RADIO_MANUAL_IMAGES_H",
        "static const struct { unsigned address, first, last; } manual_images[] = {",
    ]
    for start, first, last in PAGES:
        lines.append(f"    {{ {APP_BASE + start:#010x}u, {first - start}, {last - start} }},")
    return "\n".join([*lines, "};", "#endif", ""])


if __name__ == "__main__":
    Path(__file__).with_name("radio_manual_images.h").write_text(
        generate(Path(sys.argv[1]).read_bytes()), encoding="utf-8")
