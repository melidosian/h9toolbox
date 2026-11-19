#!/usr/bin/env python3
"""Offline tests for the module loader (no toolchain or stock inputs needed).
    python3 -I -m unittest test_build.py   (from this folder)
"""
from pathlib import Path
import json
import re
import struct
import sys
import tempfile
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


class SerialCloneWrite(unittest.TestCase):
    """The patch that sends serial / USB clone writes at 0x2000-0x3FFF through Bluetooth's sector-aware routine."""

    def test_shim_routes_the_range_and_keeps_the_stock_path(self):
        shim = STUB[STUB.index("ml_serial_write:"):STUB.index("/* ---- what a module gets in r0")]
        self.assertIn("if (r0 < 0x2000) goto mlsw_stock", shim)
        self.assertIn("if (r0 >= 0x4000) goto mlsw_stock", shim)
        self.assertIn("r3 = 0x01e5027c", shim)                 # Bluetooth's write: it overwrites r3, so r3 may be clobbered
        self.assertIn("goto r3", shim)
        stock = shim[shim.index("mlsw_stock:"):shim.index("ml_aprs_boot:")]
        self.assertIn("[--sp] = {rets, r4}", stock)            # r3 (the first-block flag) must survive for the stock routine
        self.assertIn("r4 = 0x01e78348", stock)
        self.assertIn("call r4", stock)
        self.assertNotIn("r3 =", stock)

    def test_build_patches_the_pinned_literal(self):
        self.assertEqual(build.SERIAL_WRITE_SITE, 0x78AA0)
        self.assertEqual(build.SERIAL_WRITE_OLD, (0x01E78348).to_bytes(4, "little"))
        self.assertEqual(build.IMAGE_BASE + 0x78A9E, 0x01E78B9E)    # c2ff 4883 e701: r2 = 0x01E78348 in the serial wrapper

    def test_documented_in_the_loader_source(self):
        self.assertIn("CHIRP issue 12216", STUB)


class ResourceWriteProtection(unittest.TestCase):
    def test_s2_control_and_data_check_the_same_reserved_range(self):
        from pi32v2isa import objdump, table

        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            obj = build.assemble(ROOT / "src" / "stub.s", out)
            origin = build.IMAGE_BASE + build.CAVE
            stub = build.link([obj], origin, "ml_hotkey", out, "stub")
        image = build.text_of(stub, origin)
        decoder = objdump.decoder_for(table.load(), mcpu="r3")

        def instructions(name, count):
            offset = build.symbol(stub, name) - origin
            for _ in range(count):
                _, _, size, instruction = decoder.decode(image, offset)
                yield offset, size, instruction
                offset += size

        for _, target, name in build.S2_WRITE_GUARDS:
            self.assertEqual(next(instructions(name, 1))[2], f"r3 = {build.IMAGE_BASE + target}")
        shift, low, high, deny, ret, allow = instructions("ml_s2_guard", 6)
        self.assertEqual(shift[2], "r2 = r0 >> 12")
        for branch in (low, high):
            match = re.fullmatch(r"if \(r2 (?:< 1018|>= 1024)\) goto (-?\d+)", branch[2])
            self.assertIsNotNone(match, branch[2])
            self.assertEqual(branch[0] + branch[1] + int(match[1]), allow[0])
        self.assertEqual((deny[2], ret[2], allow[2]), ("r0 = 0", "rts", "goto r3"))
        self.assertEqual((0x3fa << 12, 0x400 << 12), (0x3FA000, 0x400000))


class DevKiss(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        manifest = json.loads(build.KISS_MANIFEST.read_text())
        cls.app = bytearray(b"\xff" * build.TABLES)
        for entry in manifest["patches"]:
            site = int(entry["offset"], 16)
            before = bytes.fromhex(entry["before"])
            cls.app[site:site + len(before)] = before
        cls.helper = bytes.fromhex(manifest["patches"][-1]["after"])

    def test_relocates_calls_and_keeps_loader_and_helper_bytes(self):
        from pi32v2isa import objdump, table

        app = self.app.copy()
        end = build.KISS_HELPER + 166
        app[build.CAVE:end] = b"\x5a" * (end - build.CAVE)
        original_stub = app[build.CAVE:end]
        report, sectors = build.apply_dev_kiss(app, end)
        destination = int(report["helper_app_offset"], 16)
        self.assertEqual(destination, end + 2)
        self.assertEqual(app[build.CAVE:end], original_stub)
        self.assertEqual(app[destination:destination + len(self.helper)], self.helper)
        self.assertEqual(app[0x86328:0x8632C], bytes.fromhex("40200000"))
        self.assertEqual(app[0x5DB44:0x5DB46], b"\0\0")
        decode = objdump.decoder_for(table.load(), mcpu="r3").decode
        for site, target in build.KISS_CALLS.items():
            _, _, size, call = decode(app, site)
            self.assertEqual(size, 4)
            self.assertTrue(call.startswith("call "))
            self.assertEqual(site + size + int(call[5:]), destination + target - build.KISS_HELPER)
        self.assertEqual(sectors, {0x62000, 0x8B000, 0x8C000, 0xAC000})

    def test_rejects_existing_patch_and_cave_overflow_before_applying(self):
        app = self.app.copy()
        app[0x870E0] ^= 1
        before = app.copy()
        with self.assertRaisesRegex(ValueError, "overlaps changed bytes"):
            build.apply_dev_kiss(app, build.KISS_HELPER)
        self.assertEqual(app, before)
        with self.assertRaisesRegex(ValueError, "do not fit"):
            build.apply_dev_kiss(self.app, build.TABLES - 4)


class FlashHelpers(unittest.TestCase):
    # These addresses are part of ABI 5 and are already embedded in modules.
    origin = 0x01E85180
    exports = (
        "gfx_fill", "gfx_text", "gfx_wait_release", "gfx_wait_key",
        "h9_lcd_window", "h9_lcd_write", "h9_key", "h9_delay",
        "h9_led", "h9_usb_loader", "h9_reset", "memcpy", "memset",
    )

    @classmethod
    def setUpClass(cls):
        from pi32v2isa import objdump, table

        folder = tempfile.TemporaryDirectory()
        cls.addClassCleanup(folder.cleanup)
        out = Path(folder.name)
        fake = out / "fake_library.s"
        fake.write_text('    .section .text,"ax",@progbits\n' +
                        "".join(f"    .globl {name}\n{name}:\n    rts\n" for name in cls.exports))
        cls.flash, bridges = build.build_flash_helpers([build.assemble(fake, out)], out)
        cls.helpers = build.elf.read((out / "flash_helpers.elf").read_bytes())
        module = out / "test_module.s"
        module.write_text('    .section .text,"ax",@progbits\n    .globl module_main\nmodule_main:\n' +
                          "".join(f"    call {name}\n" for name in cls.exports) + "    rts\n")
        cls.module = build.link([build.assemble(module, out), bridges], build.LOAD,
                                "module_flash_entry", out, "test_module")
        cls.body = build.text_of(cls.module, build.LOAD)
        cls.decode = objdump.decoder_for(table.load(), mcpu="r3").decode

    def instructions(self, name, count):
        address = build.symbol(self.module, name)
        result = []
        for _ in range(count):
            _, _, step, instruction = self.decode(self.body, address - build.LOAD)
            address += step
            result.append((address, instruction))
        return result

    def test_stable_flash_jump_targets(self):
        self.assertEqual(self.helpers.entry, self.origin)
        for index, name in enumerate(self.exports):
            off = 4 * index
            _, _, size, op = self.decode(self.flash, off)
            self.assertEqual(size, 4)
            self.assertTrue(op.startswith("goto "))
            self.assertEqual(self.origin + off + size + int(op[5:]), build.symbol(self.helpers, name))

    def test_module_checks_loader_before_entering(self):
        self.assertEqual(self.module.entry, build.symbol(self.module, "module_flash_entry"))
        read, branch, run, zero, ret = self.instructions("module_flash_entry", 5)
        self.assertEqual(read[1], "r2 = [r0+4]")
        mismatch = re.fullmatch(r"if \(r2 != 5\) goto (-?\d+)", branch[1])
        self.assertIsNotNone(mismatch)
        self.assertEqual(branch[0] + int(mismatch[1]), run[0])
        self.assertEqual(run[0] + int(run[1].removeprefix("goto ")), build.symbol(self.module, "module_main"))
        self.assertEqual((zero[1], ret[1]), ("r0 = 0", "rts"))

    def test_tail_calls_preserve_fourth_and_stack_arguments(self):
        for index, name in enumerate(self.exports):
            target = self.origin + 4 * index
            if name in ("gfx_fill", "gfx_text", "h9_lcd_window"):
                instructions = [op for _, op in self.instructions(name, 7)]
                self.assertEqual(instructions, ["sp += -8", "[sp] = r4", f"r4 = {target}",
                                                "[sp+4] = r4", "r4 = [sp]", "sp += 4", "pc = [sp++]"])
            else:
                self.assertEqual([op for _, op in self.instructions(name, 2)], [f"r3 = {target}", "goto r3"])


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
