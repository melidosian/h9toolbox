#!/usr/bin/env python3
"""Offline tests for the module loader (no toolchain or stock inputs needed).
    python3 -I -m unittest test_build.py   (from this folder)
"""
from pathlib import Path
import re
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402

sys.path.insert(0, str(ROOT / "modules" / "common"))
import font5x7  # noqa: E402

STUB = (ROOT / "src" / "stub.s").read_text(encoding="utf-8")
UPLOAD = (ROOT.parents[1] / "dist" / "doom" / "upload.mjs").read_text(encoding="utf-8")


def module(name: str, size: int, entry: int = 0, bss: int = 0, seed: int = 1) -> dict:
    return {"name": name, "body": bytes((i * 37 + seed) & 255 for i in range(size)), "entry": entry, "bss": bss}


class Bundle(unittest.TestCase):
    def test_crc16_is_xmodem(self):
        self.assertEqual(build.crc16(b"123456789"), 0x31C3)

    def test_round_trip(self):
        mods = [module("MENU", 1760, 0x268), module("BRICK BREAKER", 1629, 0x52, seed=7), module("X", 4, 0, 16)]
        data = build.bundle(mods)
        parsed = build.parse_bundle(data)
        self.assertEqual([p["name"] for p in parsed], ["MENU", "BRICK BREAKER", "X"])
        for p, m in zip(parsed, mods):
            self.assertEqual(p["body"], m["body"])
            self.assertEqual(p["entry"], m["entry"])
            self.assertEqual(p["offset"] % 4, 0)
        self.assertEqual(struct.unpack_from("<I", data, 12)[0], len(data))

    def test_limits(self):
        build.bundle([module("A", 0x2000)])
        with self.assertRaises(ValueError):
            build.bundle([module("A", 0x2000, bss=1)])  # body + bss over the 8 KiB slot
        with self.assertRaises(ValueError):
            build.bundle([module("A", 16, entry=16)])   # entry outside the body
        with self.assertRaises(ValueError):
            build.bundle([module("NAME-LONGER-THAN-16", 4)])
        with self.assertRaises(ValueError):
            build.bundle([])

    def test_corruption_is_rejected(self):
        data = bytearray(build.bundle([module("MENU", 100), module("GAME", 200)]))
        data[-1] ^= 1
        with self.assertRaises(ValueError):
            build.parse_bundle(bytes(data))


class StubMatchesLayout(unittest.TestCase):
    """The stub's constants must agree with build.py and the site's uploader."""

    def test_slot_addresses(self):
        self.assertIn("MODULES_DIRECTORY = 0xcc0000", UPLOAD)
        self.assertIn("start: MODULES_DIRECTORY + 0x1000, end: MUSIC_DIRECTORY", UPLOAD)
        self.assertEqual((build.SLOT_DIRECTORY, build.SLOT_DATA), (0xCC0000, 0xCC1000))
        for value in ("0x00cc0000", "0x00cc1000", "0x00cc0fe0", "0x00cc1010"):
            self.assertIn(value, STUB)
        self.assertIn("r1 = 0x1f000", STUB)  # 124 KiB, the slot's data size
        self.assertEqual(0x1F000, build.SLOT_LIMIT - build.SLOT_DATA)

    def test_load_slot(self):
        # the loader and ml_api use it 5 times; the resident hook (res_tick) twice more (the lower
        # bound of the entry it may call, and the slot it checks the CRC of)
        self.assertEqual(STUB.count("0x00029f00"), 7)
        self.assertIn("r1 = 0x2000", STUB)
        self.assertEqual(build.SLOT_END - build.LOAD, 0x2000)

    def test_magic_words(self):
        words = {int(v, 16) for v in re.findall(r"r1 = (0x[0-9a-f]{8})", STUB)}
        for text in (b"H9DA", b"TA01", b"TA-C", b"OMPL", b"H9MO", b"DS01"):
            self.assertIn(struct.unpack("<I", text)[0], words, text)
        self.assertIn(".long 0x50413948", STUB)  # "H9AP"
        self.assertEqual(struct.unpack("<I", b"H9AP")[0], 0x50413948)


class Font(unittest.TestCase):
    def test_glyphs(self):
        glyphs = font5x7.parse()
        for c in " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:/>?":
            self.assertIn(c, glyphs)
        self.assertEqual(glyphs[" "], [0] * 7)
        self.assertTrue(all(0 <= r < 32 for rows in glyphs.values() for r in rows))
        self.assertEqual(len({tuple(r) for r in glyphs.values()}), len(glyphs), "two glyphs are identical")


if __name__ == "__main__":
    unittest.main()
