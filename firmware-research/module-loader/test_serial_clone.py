#!/usr/bin/env python3
"""Tests serial_clone_test.py against two simulated radios: stock serial (programs flash: bits can only be cleared
above 0x3000, writes are ACKed anyway) and a radio with the sector-aware write.
    python3 -I -m unittest test_serial_clone.py   (from this folder)"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import serial_clone_test as tool  # noqa: E402


class FakeRadio:
    """Just enough of the clone protocol; `sector_aware` False = stock serial (AND semantics above 0x2000)."""

    def __init__(self, sector_aware, checksum=True, talk_around=0):
        self.sector_aware, self.checksum = sector_aware, checksum
        self.mem = bytearray(0x3140)
        self.mem[0x3014] = 0x80 | talk_around          # other bits set too: a stock 1 -> 0 write must keep them
        self.out = bytearray()
        self.buffer = bytearray()
        self.state = "idle"

    def write(self, data):
        self.buffer += data
        while self.buffer:
            if self.state == "idle":
                if len(self.buffer) < 7: return
                assert bytes(self.buffer[:7]) == tool.MAGIC
                del self.buffer[:7]; self.out += b"\x06"; self.state = "ident"
            elif self.state == "ident":
                if self.buffer[0] == 0x02:
                    del self.buffer[:1]; self.out += b"TDH9\xff\xff\xff\x4e"
                elif self.buffer[0] == 0x06:
                    del self.buffer[:1]; self.out += b"\x06"; self.state = "prog"
                else:
                    raise AssertionError(self.buffer)
            else:
                c = self.buffer[0]
                if c == 0x45:
                    del self.buffer[:1]; self.state = "idle"; self.out += b"\x06"
                elif c == 0x52:
                    if len(self.buffer) < 4: return
                    a = self.buffer[1] << 8 | self.buffer[2]
                    del self.buffer[:4]
                    self.out += bytes([0x57, a >> 8, a & 255, 0x20]) + self.mem[a:a + 0x20]
                    if self.checksum: self.out += bytes([sum(self.mem[a:a + 0x20]) & 255])
                elif c == 0x57:
                    if len(self.buffer) < 37: return
                    a = self.buffer[1] << 8 | self.buffer[2]
                    data = self.buffer[4:36]
                    assert self.buffer[36] == sum(data) & 255
                    del self.buffer[:37]
                    for i, b in enumerate(data):
                        self.mem[a + i] = b if (self.sector_aware or a < 0x2000) else self.mem[a + i] & b   # stock: flash can only be cleared
                    self.out += b"\x06"                         # ACKed either way
                else:
                    raise AssertionError(hex(c))

    def read(self, n, timeout):
        got = bytes(self.out[:n]); del self.out[:n]
        return got


class SerialCloneTool(unittest.TestCase):
    def go(self, **kw):
        fake = FakeRadio(**kw)
        radio = tool.Radio(fake)
        self.assertEqual(radio.enter(), "Normal")
        ok, message = tool.check(radio, lambda *_: None)
        radio.exit()
        return fake, ok, message

    def test_sector_aware_radio_passes_and_is_restored(self):
        for talk in (0, 1):
            fake, ok, message = self.go(sector_aware=True, talk_around=talk)
            self.assertTrue(ok, message)
            self.assertEqual(fake.mem[0x3014], 0x80 | talk)

    def test_stock_serial_radio_fails_when_the_bit_is_clear(self):
        fake, ok, message = self.go(sector_aware=False, talk_around=0)
        self.assertFalse(ok)
        self.assertIn("FAIL", message)
        self.assertEqual(fake.mem[0x3014], 0x80)           # nothing changed, so nothing to restore

    def test_stock_serial_radio_fails_when_the_bit_is_set_and_says_it_could_not_restore(self):
        lines = []
        fake = FakeRadio(sector_aware=False, talk_around=1)
        radio = tool.Radio(fake); radio.enter()
        ok, message = tool.check(radio, lines.append)
        self.assertFalse(ok)
        self.assertEqual(fake.mem[0x3014], 0x80)           # the 1 -> 0 took, the restore could not: the tool says so
        self.assertTrue(any("NOT RESTORED" in l for l in lines))

    def test_reads_without_a_checksum_byte(self):
        fake, ok, _ = self.go(sector_aware=True, checksum=False)
        self.assertTrue(ok)

    def test_range_test_on_a_sector_aware_radio(self):
        for talk in (0, 1):
            fake = FakeRadio(sector_aware=True, talk_around=talk)
            before = bytes(fake.mem)
            radio = tool.Radio(fake); radio.enter()
            ok, message = tool.range_test(radio, lambda *_: None)
            self.assertTrue(ok, message)
            self.assertEqual(bytes(fake.mem), before)             # everything restored

    def test_range_test_on_stock_serial_fails_and_leaves_other_blocks_alone(self):
        fake = FakeRadio(sector_aware=False, talk_around=0)
        fake.mem[0x3040:0x3060] = bytes(range(32))
        before = bytes(fake.mem)
        radio = tool.Radio(fake); radio.enter()
        ok, message = tool.range_test(radio, lambda *_: None)
        self.assertFalse(ok)
        self.assertEqual(bytes(fake.mem), before)                 # the identical rewrites changed nothing; the bit never took

    def test_main_region_is_unaffected(self):
        fake = FakeRadio(sector_aware=False)
        radio = tool.Radio(fake); radio.enter()
        radio.write_block(0x0010, bytes(range(32)))
        self.assertEqual(radio.read_block(0x0010), bytes(range(32)))


if __name__ == "__main__":
    unittest.main()
