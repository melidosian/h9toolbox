#!/usr/bin/env python3
"""Build the RX15 and full-byte receive fixes for stock TD-H9 APRS 1.0.15."""

import argparse
from hashlib import sha256
import json
from pathlib import Path


STOCK_SHA256 = "1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33"
IMAGE_BASE = 0x08002000
RX_MINIMUM = 0x0800521A - IMAGE_BASE
RX_BYTE_RETURN = 0x08005E20 - IMAGE_BASE


def patch(stock: bytes) -> bytes:
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected the stock APRS 1.0.15 image")

    result = bytearray(stock)
    # AX.25 needs a minimum of 15 body bytes plus the two FCS bytes.
    if stock[RX_MINIMUM:RX_MINIMUM + 2] != bytes.fromhex("12 28"):
        raise ValueError("Unexpected receive minimum instruction")
    result[RX_MINIMUM] = 17

    # The queue pop returns an unsigned byte or -1 when empty. SXTB in the
    # wrapper conflates valid data 0xFF with -1 after the byte was consumed.
    if stock[RX_BYTE_RETURN:RX_BYTE_RETURN + 2] != bytes.fromhex("40 b2"):
        raise ValueError("Unexpected receive queue return instruction")
    result[RX_BYTE_RETURN:RX_BYTE_RETURN + 2] = bytes.fromhex("00 bf")
    return bytes(result)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        image = patch(args.stock.read_bytes())
    except ValueError as error:
        parser.error(str(error))

    manifest = {
        "kind": "aprs_release_image",
        "flashable": True,
        "image_sha256": sha256(image).hexdigest(),
        "image_bytes": len(image),
        "image_base": IMAGE_BASE,
        "base_image_sha256": STOCK_SHA256,
        "description": "Stock APRS 1.0.15 with RX15 and unsigned receive bytes",
    }
    with args.output.open("xb") as output:
        output.write(image)
    with args.output.with_suffix(".release.json").open("x") as output:
        json.dump(manifest, output, indent=2)
        output.write("\n")
    print(manifest["image_sha256"])


if __name__ == "__main__":
    main()
