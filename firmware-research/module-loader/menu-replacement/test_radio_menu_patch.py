import sys
from pathlib import Path
from hashlib import sha256
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build
import radio_menu_patch as menu
from pi32v2isa import objdump, table


def assemble(source):
    return bytes(next(s for s in build.elf.read(build.asm.assemble_source(source)).sections if s.name == ".text").data)


class NativeMenuPatch(unittest.TestCase):
    def test_replaces_only_root_and_guards_far_payload(self):
        stock = bytearray(b"\xff" * 0xC0000)
        for site, target in menu.UI_POINTERS:
            stock[site:site + 4] = (menu.IMAGE_BASE + target).to_bytes(4, "little")
        image = bytearray(stock)
        with patch.object(menu, "STOCK_SHA256", sha256(stock).hexdigest()):
            report = menu.apply(stock, image, 0x1EA7B40, 0x1EA7C00, 0x1EE5000, 20000, 0x8A42, assemble)
        cursor = 0
        for _, start, end in menu.REMOVED_UI:
            self.assertEqual(image[cursor:start], stock[cursor:start])
            cursor = end
            if start != menu.ROOT_START:
                self.assertEqual(image[start:start + 8], assemble(f"r3 = {menu.IMAGE_BASE + menu.ROOT_START:#x}\n goto r3\n"))
                self.assertEqual(image[start + 8:end], bytes(end-start-8))
        self.assertEqual(image[cursor:], stock[cursor:])
        self.assertEqual(report["root_removed_bytes"], 104)
        self.assertEqual(report["removed_bytes"], sum(end-start for _,start,end in menu.REMOVED_UI))
        decode = objdump.decoder_for(table.load(), mcpu="r3").decode
        body = image[menu.ROOT_START:menu.ROOT_END]
        offset = 0
        instructions, offsets = [], []
        while offset < report["guard_bytes"]:
            _, _, n, op = decode(body, offset)
            instructions.append(op)
            offsets.append(offset)
            offset += n
        self.assertIn("r0 = 32395264", instructions)  # 0x1ee5000
        self.assertIn("r1 = 20000", instructions)
        self.assertIn("r1 = 35394", instructions)  # expected CRC
        self.assertIn("r4 = 33557864", instructions)  # 0x2000d68 USB recovery
        loader = instructions.index(f"r4 = {0x1EA7B40}")
        self.assertEqual(instructions[loader:loader + 9], [
            f"r4 = {0x1EA7B40}", "call r4",
            "r4 = 72400", "r0 = 0",
            "b[r4+54] = r0",  # native view index
            f"r0 = {0x1E55D44}",
            "[r4+524] = r0",  # native home callback
            "{rets, r4} = [sp++]", "goto r0",
        ])
        mismatch = instructions[loader - 1]
        self.assertTrue(mismatch.startswith("if (r0 != r1) goto "))
        self.assertEqual(offsets[loader] + int(mismatch.split()[-1]),
                         offsets[instructions.index("r4 = 33557864")])
        self.assertEqual(body[report["guard_bytes"]:], bytes(104 - report["guard_bytes"]))

    def test_rejects_wrong_image(self):
        with self.assertRaisesRegex(ValueError, "factory"):
            menu.apply(bytes(100), bytearray(100), 0, 0, 0, 0, 0, assemble)

    def test_rejects_overlap_without_mutating_any_span(self):
        stock = bytearray(b"\xff" * 0xC0000)
        for site, target in menu.UI_POINTERS:
            stock[site:site+4] = (menu.IMAGE_BASE+target).to_bytes(4,"little")
        image = bytearray(stock)
        image[0x73496] ^= 1
        before = bytes(image)
        with patch.object(menu,"STOCK_SHA256",sha256(stock).hexdigest()):
            with self.assertRaisesRegex(ValueError,"overlaps"):
                menu.apply(stock,image,0,0,0,0,0,assemble)
        self.assertEqual(bytes(image),before)


if __name__ == "__main__":
    unittest.main()
