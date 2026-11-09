"""Offline tests for restore_stock.py against a simulated PTT+2 radio."""

import binascii
import os
from pathlib import Path
import random
import unittest
from unittest.mock import patch

import restore_stock as rs

STOCK = Path(os.environ.get("H9_STOCK_RESTORE_POINT",
                            Path.home() / "h9-backups" / "h9-stock-v1.0.38-restore-point.bin"))


class SimulatedRadio:
    """1 MiB main flash behind the three HID wrappers, with NOR semantics."""

    def __init__(self, image: bytes):
        self.flash = bytearray(b"\xff" * 0x100000)
        self.flash[:len(image)] = image
        self.erases, self.writes, self.order = [], 0, []
        self.fail_after_erases = None

    def crcs(self, device, physical, count):
        return [binascii.crc_hqx(bytes(self.flash[p:p + 4096]), 0)
                for p in range(physical, physical + count * 4096, 4096)]

    def erase(self, device, physical):
        assert physical % 4096 == 0 and rs.APP_START <= physical < rs.MAIN_END
        if self.fail_after_erases is not None and len(self.erases) >= self.fail_after_erases:
            raise RuntimeError("simulated USB drop")
        self.erases.append(physical)
        self.order.append(("erase", physical))
        self.flash[physical:physical + 4096] = b"\xff" * 4096

    def write(self, device, address, payload):
        assert rs.APP_START <= address and address + len(payload) <= rs.MAIN_END
        assert len(payload) <= rs.WRITE_CHUNK
        self.writes += 1
        self.order.append(("write", address))
        for i, b in enumerate(payload):
            self.flash[address + i] &= b  # NOR programming only clears bits

    def patches(self):
        return (patch.object(rs, "hid_crcs", self.crcs), patch.object(rs, "hid_erase", self.erase),
                patch.object(rs, "hid_write", self.write))


@unittest.skipUnless(STOCK.exists(), "stock restore point not available")
class RestoreStockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.stock = rs.load_stock(STOCK)

    def run_tool(self, radio, known=frozenset(), write=True, everything=False):
        a, b, c = radio.patches()
        with a, b, c:
            return rs.run(None, self.stock, set(known), write=write, everything=everything)

    def modified(self, sectors, seed=1):
        image = bytearray(self.stock)
        rng = random.Random(seed)
        for s in sectors:
            image[s:s + 4096] = bytes(rng.randrange(256) for _ in range(4096))
        return bytes(image)

    def test_restores_any_changed_sectors_and_header_last(self):
        changed = [0x5000, 0x20000, 0x62000, 0xAC000, 0xD9000]
        radio = SimulatedRadio(self.modified(changed))
        done = self.run_tool(radio)
        self.assertEqual(sorted(done), changed)
        self.assertEqual(bytes(radio.flash[:rs.MAIN_END]), self.stock)
        self.assertEqual(radio.erases[-1], 0x5000)            # header sector last
        header_writes = [a for kind, a in radio.order if kind == "write" and 0x5000 <= a < 0x6000]
        self.assertEqual(header_writes[-1], 0x5000)            # its first 32 bytes last

    def test_large_custom_firmware(self):
        changed = list(range(0x60000, 0xC0000, 0x1000))         # a 384 KiB rewrite
        radio = SimulatedRadio(self.modified(changed, seed=7))
        self.run_tool(radio)
        self.assertEqual(bytes(radio.flash[:rs.MAIN_END]), self.stock)

    def test_inspect_writes_nothing(self):
        radio = SimulatedRadio(self.modified([0x30000]))
        todo = self.run_tool(radio, write=False)
        self.assertEqual(todo, [0x30000])
        self.assertEqual((radio.erases, radio.writes), ([], 0))

    def test_stock_radio_needs_nothing(self):
        radio = SimulatedRadio(self.stock)
        self.assertEqual(self.run_tool(radio), [])
        self.assertEqual(radio.erases, [])

    def test_known_sectors_are_rewritten_even_with_a_stock_crc(self):
        # Simulate a CRC collision: the radio's CRC says stock, but the known
        # package changed the sector, so it is rewritten anyway.
        radio = SimulatedRadio(self.stock)
        done = self.run_tool(radio, known={0x8D000})
        self.assertEqual(done, [0x8D000])
        self.assertEqual(radio.erases, [0x8D000])

    def test_all_rewrites_every_app_sector(self):
        radio = SimulatedRadio(self.stock)
        self.assertEqual(len(self.run_tool(radio, everything=True)), 213)

    def test_interrupted_restore_resumes(self):
        changed = [0x10000, 0x40000, 0x90000, 0x5000]
        radio = SimulatedRadio(self.modified(changed))
        radio.fail_after_erases = 2                               # USB drops mid-restore
        with self.assertRaisesRegex(RuntimeError, "simulated USB drop"):
            self.run_tool(radio)
        radio.fail_after_erases = None
        self.run_tool(radio)
        self.assertEqual(bytes(radio.flash[:rs.MAIN_END]), self.stock)

    def test_never_touches_beyond_the_app_image(self):
        radio = SimulatedRadio(self.modified([0xD9000]))
        radio.flash[0xDA000:0xDA004] = b"\x55\xaa\xaa\x55"      # settings VM
        radio.flash[0xEA000:0xEA004] = b"H9X1"                  # far program
        tail = bytes(radio.flash[rs.MAIN_END:])
        self.run_tool(radio)
        self.assertEqual(bytes(radio.flash[rs.MAIN_END:]), tail)

    def test_rejects_a_wrong_restore_point(self):
        bad = Path(self.id() + ".bin")
        bad.write_bytes(self.stock[:-1] + b"\x00")
        try:
            with self.assertRaisesRegex(ValueError, "pinned stock"):
                rs.load_stock(bad)
        finally:
            bad.unlink()


if __name__ == "__main__":
    unittest.main()
