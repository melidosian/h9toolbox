"""Generate the offline C menu label catalog from the pinned stock inventory."""

from __future__ import annotations

import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "stock-menu-v1.0.38.json"
OUTPUT = HERE / "stock_menu_catalog.h"


def main() -> None:
    data = json.loads(SOURCE.read_text())
    if data["factory_menu_items"] != 119 or data["active_dev_menu_items"] != 110:
        raise ValueError("Stock menu inventory changed")
    categories = [category for category in data["stock_root_categories"]
                  if not category["disabled_in_dev"]]
    if len(categories) != 10 or sum(len(c["items"]) for c in categories) != 110:
        raise ValueError("Expected ten active stock categories and 110 rows")
    lines = ["/* Generated from stock-menu-v1.0.38.json; do not edit by hand. */",
             "#ifndef STOCK_MENU_CATALOG_H", "#define STOCK_MENU_CATALOG_H", "",
             "struct radio_category { const char *name; const char *const *items; unsigned count; };",
             "enum { RADIO_CATEGORY_COUNT = 10, RADIO_ITEM_COUNT = 110 };", ""]
    for i, category in enumerate(categories):
        lines.append(f"static const char *const radio_items_{i}[] = {{")
        lines.extend(f"    {json.dumps(label)}," for label in category["items"])
        lines.append("};")
    lines += ["", "static const struct radio_category radio_categories[RADIO_CATEGORY_COUNT] = {"]
    for i, category in enumerate(categories):
        lines.append(f"    {{ {json.dumps(category['label'])}, radio_items_{i}, "
                     f"sizeof radio_items_{i} / sizeof *radio_items_{i} }},")
    lines += ["};", "", "#endif", ""]
    OUTPUT.write_text("\n".join(lines))


if __name__ == "__main__":
    main()
