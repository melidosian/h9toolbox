"""Inventory the pinned stock menu before replacing its UI.

This is read-only. It deliberately accepts only the factory 1.0.38 app image;
addresses in the report are not portable to another firmware build.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build  # noqa: E402

STOCK_MENU_CALLBACK_SITE = 0xB6BC4
STOCK_MENU_CALLBACK = 0x01E6D426
ENGLISH_ROOT_LABELS = 0xCC9AC
ROOT_CATEGORY_IDS = tuple(range(11))
MORSE_ID = 7
MENU_SHARED_STATE = 0x2B880


def root_labels(stock: bytes) -> list[str]:
    if sha256(stock).hexdigest() != build.STOCK_SHA256:
        raise ValueError("Expected pinned factory TD-H9 v1.0.38 app.bin")
    callback = struct.unpack_from("<I", stock, STOCK_MENU_CALLBACK_SITE)[0]
    if callback != STOCK_MENU_CALLBACK:
        raise ValueError(f"Stock Menu callback changed: {callback:#x}")
    labels = []
    for category in ROOT_CATEGORY_IDS:
        address = struct.unpack_from("<I", stock, ENGLISH_ROOT_LABELS + 4 * category)[0]
        offset = address - build.IMAGE_BASE
        if not 0 <= offset < len(stock):
            raise ValueError(f"Category {category} label pointer is outside app.bin")
        end = stock.find(b"\0", offset, min(offset + 32, len(stock)))
        if end < 0:
            raise ValueError(f"Category {category} label is unterminated")
        labels.append(stock[offset:end].decode("ascii"))
    return labels


def inventory(stock: bytes, bundle: bytes) -> dict:
    labels = root_labels(stock)
    modules = build.parse_bundle(bundle)
    menu = next((module for module in modules if module["name"] == "MENU"), None)
    if menu is None:
        raise ValueError("Bundle has no MENU module")
    menu_ceiling = MENU_SHARED_STATE - build.LOAD
    return {
        "stock_sha256": sha256(stock).hexdigest(),
        "stock_menu_callback_site": hex(STOCK_MENU_CALLBACK_SITE),
        "stock_menu_callback": hex(STOCK_MENU_CALLBACK),
        "stock_root_categories": [
            {"id": index, "label": label, "disabled_in_dev": index == MORSE_ID}
            for index, label in enumerate(labels)
        ],
        "bundle_sha256": sha256(bundle).hexdigest(),
        "toolbox_menu_bytes": menu["size"],
        "toolbox_menu_safe_ceiling": menu_ceiling,
        "toolbox_menu_free_bytes": menu_ceiling - menu["size"],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock_image", type=Path)
    parser.add_argument("module_bundle", type=Path)
    args = parser.parse_args()
    print(json.dumps(inventory(args.stock_image.read_bytes(), args.module_bundle.read_bytes()), indent=2))


if __name__ == "__main__":
    main()
