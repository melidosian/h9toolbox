#!/usr/bin/env python3
"""Correct one confirmed Pi32v2 SLEIGH byte-load offset error in Ghidra-JieLi.

The long unsigned-byte load has an 8-bit positive offset split across the
second instruction word. The upstream constructor shifts its high nibble by
eight instead of four, turning e.g. +0x1c into +0x10c. It then creates wrong
data references and decompiler variable names. This modifies the *local*
processor source only; rerun Ghidra's sleigh compiler and reimport the image.
"""

from __future__ import annotations

import argparse
from pathlib import Path


OLD = "[ imm16 = (imm2427 << 8) | imm1619; ]"
NEW = "[ imm16 = (imm2427 << 4) | imm1619; ]"
RELATIVE = Path("data/languages/pi32v2_ins_loadstore.sinc")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jieli_module", type=Path,
                        help="Path to Ghidra/Processors/JieLi")
    args = parser.parse_args()
    target = args.jieli_module / RELATIVE
    original = target.read_text(encoding="utf-8")
    if original.count(NEW) == 1 and OLD not in original:
        print(f"Already fixed: {target}")
        return 0
    if original.count(OLD) != 1 or NEW in original:
        raise ValueError(f"Unexpected source: refusing to change {target}")
    backup = target.with_suffix(target.suffix + ".original")
    if backup.exists():
        raise ValueError(f"Backup already exists: refusing to change {target}")
    backup.write_text(original, encoding="utf-8")
    target.write_text(original.replace(OLD, NEW), encoding="utf-8")
    print(f"Fixed: {target}")
    print(f"Original saved: {backup}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
