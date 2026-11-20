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

# A record inside the stock menu dispatch table. Its exact role as an entry
# point still needs call-site tracing; do not treat it as the Menu-key hook.
MENU_TABLE_ENTRY_SITE = 0xB6BC4
MENU_TABLE_ENTRY_TARGET = 0x01E6D426
ENGLISH_ROOT_LABELS = 0xCC9AC
ROOT_CATEGORY_IDS = tuple(range(11))
MORSE_ID = 7
MENU_SHARED_STATE = 0x2B880
SUBMENU_BUILDER = (0x6811E, 0x68296)
SUBMENU_BUILDER_SHA256 = "6be77c78a40b1781b3cecd81cb71f2b402ae1979e620773e33100adcf4845892"
# The stock selector's eleven branches write these final counts at RAM
# 0x12C74. Most loops stop *before* the comparator's bound (N - 1 items).
SUBMENU_COUNTS = (43, 16, 2, 3, 6, 12, 9, 9, 10, 2, 7)
# English pointer runs that match those counts in category order. The stock
# SHA check above pins the table bytes as well as the builder.
SUBMENU_LABEL_STARTS = (0xCCA4C, 0xCCBA4, 0xCCA04, 0xCCC24, 0xCCC3C,
                        0xCCC6C, 0xCCCCC, 0xCCD14, 0xCCE0C, 0xCCE5C, 0xCCE6C)


def stock_label(stock: bytes, pointer_site: int) -> str:
    address = struct.unpack_from("<I", stock, pointer_site)[0]
    offset = address - build.IMAGE_BASE
    if not 0 <= offset < len(stock):
        raise ValueError(f"Label pointer at {pointer_site:#x} is outside app.bin")
    end = stock.find(b"\0", offset, min(offset + 32, len(stock)))
    if end < 0:
        raise ValueError(f"Label at {pointer_site:#x} is unterminated")
    try:
        return stock[offset:end].decode("ascii")
    except UnicodeDecodeError as error:
        raise ValueError(f"Label at {pointer_site:#x} is not ASCII") from error


def root_labels(stock: bytes) -> list[str]:
    if sha256(stock).hexdigest() != build.STOCK_SHA256:
        raise ValueError("Expected pinned factory TD-H9 v1.0.38 app.bin")
    target = struct.unpack_from("<I", stock, MENU_TABLE_ENTRY_SITE)[0]
    if target != MENU_TABLE_ENTRY_TARGET:
        raise ValueError(f"Stock menu table entry changed: {target:#x}")
    start, end = SUBMENU_BUILDER
    if sha256(stock[start:end]).hexdigest() != SUBMENU_BUILDER_SHA256:
        raise ValueError("Stock submenu builder changed")
    return [stock_label(stock, ENGLISH_ROOT_LABELS + 4 * category)
            for category in ROOT_CATEGORY_IDS]


def inventory(stock: bytes, bundle: bytes, far_menu: bytes | None = None) -> dict:
    labels = root_labels(stock)
    modules = build.parse_bundle(bundle)
    menu = next((module for module in modules if module["name"] == "MENU"), None)
    if menu is None:
        raise ValueError("Bundle has no MENU module")
    if menu["size"] == 8 and far_menu is None:
        raise ValueError("Eight-byte MENU trampoline requires --far-menu")
    if far_menu is not None and menu["size"] != 8:
        raise ValueError("--far-menu requires an eight-byte MENU trampoline")
    menu_ceiling = MENU_SHARED_STATE - build.LOAD
    report = {
        "stock_sha256": sha256(stock).hexdigest(),
        "menu_table_entry_site": hex(MENU_TABLE_ENTRY_SITE),
        "menu_table_entry_target": hex(MENU_TABLE_ENTRY_TARGET),
        "stock_root_categories": [
            {"id": index, "label": label,
             "items": [stock_label(stock, SUBMENU_LABEL_STARTS[index] + 4 * item)
                       for item in range(SUBMENU_COUNTS[index])],
             "disabled_in_dev": index == MORSE_ID}
            for index, label in enumerate(labels)
        ],
        "factory_menu_items": sum(SUBMENU_COUNTS),
        "active_dev_menu_items": sum(SUBMENU_COUNTS) - SUBMENU_COUNTS[MORSE_ID],
        "bundle_sha256": sha256(bundle).hexdigest(),
        "toolbox_menu_bytes": menu["size"],
        "toolbox_menu_safe_ceiling": menu_ceiling,
        "toolbox_menu_free_bytes": menu_ceiling - menu["size"],
    }
    if far_menu is not None:
        report["far_menu"] = {"flash_offset": hex(build.FAR_MENU_FLASH),
                              "bytes": len(far_menu), "sha256": sha256(far_menu).hexdigest(),
                              "window_free_bytes": build.FAR_MENU_END - build.FAR_MENU_FLASH - len(far_menu)}
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stock_image", type=Path)
    parser.add_argument("module_bundle", type=Path)
    parser.add_argument("--far-menu", type=Path)
    args = parser.parse_args()
    far_menu = args.far_menu.read_bytes() if args.far_menu else None
    print(json.dumps(inventory(args.stock_image.read_bytes(), args.module_bundle.read_bytes(), far_menu), indent=2))


if __name__ == "__main__":
    main()
