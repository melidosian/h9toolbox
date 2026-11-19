from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build
import morse_disable as m

class MorseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = build.RESEARCH / 'setup/stock/app.bin'
        if not path.exists():
            raise unittest.SkipTest('pinned stock app not installed')
        cls.stock = path.read_bytes()

    def test_only_pinned_sites_change(self):
        edits = m.patches(self.stock)
        patched = m.apply(self.stock, self.stock)
        self.assertEqual(len(patched), len(self.stock))
        allowed = {i for site, old, new in edits for i in range(site, site+len(old))}
        self.assertTrue({i for i,(a,b) in enumerate(zip(self.stock, patched)) if a != b} <= allowed)
        for site, old, new in edits:
            self.assertEqual(patched[site:site+len(new)], new)
        # Both enable states now take the same existing off path; no register writes.
        decoder = m.objdump.decoder_for(m.table.load(), mcpu='r3')
        for site, _, _, target in m.ENTRY_GATES:
            insn = decoder.decode(patched, site)
            self.assertEqual(insn[2], 2)
            self.assertEqual(site+2+int(insn[3][5:]), target)
            self.assertEqual(patched[site+2:site+6], b'\0'*4)
        self.assertEqual(m.MENU_TABLE+2*int.from_bytes(patched[m.MENU_ENTRY:m.MENU_ENTRY+2], 'little'), m.MENU_EXIT)
        for start, end, _ in m.REMOVED:
            self.assertEqual(patched[start:end], bytes(end-start))
            self.assertEqual(patched[end:end+2], self.stock[end:end+2])

    def test_top_level_menu_keeps_category_ids_for_every_locale(self):
        patched = m.apply(self.stock, self.stock)
        decode = m.objdump.decoder_for(m.table.load(), mcpu='r3').decode

        def initialize_menu(image):
            # The stock case initializes IDs 0–6 before the patched tail.
            descriptor = bytearray([0xFF, 0x15, 0x12, *range(7), 7, 8, 9, 10, 0xAD])
            offset = m.MENU_ROOT_TAIL
            end = offset + len(m.MENU_ROOT_STOCK)
            r0 = -1
            while offset < end:
                _, _, length, op = decode(image, offset)
                offset += length
                if value := re.fullmatch(r'r0 = (\d+)', op):
                    r0 = int(value[1])
                elif store := re.fullmatch(r'b\[r1\+(\d+)\] = r0', op):
                    descriptor[int(store[1])] = r0
                else:
                    self.assertEqual(op, 'nop')
            self.assertEqual(offset, end)
            self.assertEqual(decode(image, offset)[3], '{pc, r4} = [sp++]')
            self.assertEqual(r0, 10)
            self.assertEqual((descriptor[1], descriptor[2], descriptor[14]), (0x15, 0x12, 0xAD))
            return list(descriptor[3:3 + descriptor[0]])

        self.assertEqual(initialize_menu(self.stock), list(range(11)))
        self.assertEqual(initialize_menu(patched), [*range(7), 8, 9, 10])
        self.assertEqual(patched[0xB3350:0xB3398], self.stock[0xB3350:0xB3398])

    def test_refuse_wrong_stock_and_conflicts(self):
        wrong = bytearray(self.stock); wrong[0] ^= 1
        with self.assertRaises(ValueError): m.apply(wrong, self.stock)
        for site, old, new in m.patches(self.stock):
            wrong = bytearray(self.stock); wrong[site] ^= 1
            before = bytes(wrong)
            with self.assertRaises(ValueError): m.apply(self.stock, wrong)
            self.assertEqual(bytes(wrong), before)

if __name__ == '__main__': unittest.main()
