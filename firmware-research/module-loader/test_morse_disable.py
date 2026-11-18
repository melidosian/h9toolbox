from pathlib import Path
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

    def test_only_four_pinned_sites_change(self):
        edits = m.patches(self.stock)
        patched = m.apply(self.stock, self.stock)
        self.assertEqual(len(patched), len(self.stock))
        allowed = {i for site, old, new in edits for i in range(site, site+len(old))}
        self.assertTrue({i for i,(a,b) in enumerate(zip(self.stock, patched)) if a != b} <= allowed)
        for site, old, new in edits:
            self.assertEqual(patched[site:site+len(new)], new)
        # Both enable states now take the same existing off path; no register writes.
        decoder = m.objdump.decoder_for(m.table.load(), mcpu='r3')
        for site, _ in m.ENTRY_GATES:
            insn = decoder.decode(patched, site)
            self.assertEqual(insn[2], 2)
            self.assertEqual(site+2+int(insn[3][5:]), m.DISABLED_TARGET)
            self.assertEqual(patched[site+2:site+6], b'\0'*4)
        site, _, target = m.KEY_GATE
        insn = decoder.decode(patched, site)
        self.assertEqual(insn[2], 2)
        self.assertEqual(site+2+int(insn[3][5:]), target)
        self.assertEqual(patched[site+2:site+6], b'\0'*4)
        self.assertEqual(m.MENU_TABLE+2*int.from_bytes(patched[m.MENU_ENTRY:m.MENU_ENTRY+2], 'little'), m.MENU_EXIT)

    def test_refuse_wrong_stock_and_conflicts(self):
        wrong = bytearray(self.stock); wrong[0] ^= 1
        with self.assertRaises(ValueError): m.apply(wrong, self.stock)
        for site, old, new in m.patches(self.stock):
            wrong = bytearray(self.stock); wrong[site] ^= 1
            before = bytes(wrong)
            with self.assertRaises(ValueError): m.apply(self.stock, wrong)
            self.assertEqual(bytes(wrong), before)

if __name__ == '__main__': unittest.main()
