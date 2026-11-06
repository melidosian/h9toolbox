"""Offline check of shifted-CRC backup reconstruction with a fake HID radio."""

from __future__ import annotations

import random
import sys
from pathlib import Path
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import hid_install as backup  # noqa: E402


class HidBackupTest(unittest.TestCase):
    def test_reconstruct_accessible_tail(self) -> None:
        rng = random.Random(20261106)
        stock = rng.randbytes(backup.MAIN_END)
        full = stock + rng.randbytes(backup.FLASH_SIZE - backup.MAIN_END)

        def fetch(_device, offset: int, count: int) -> list[int]:
            start = backup.APP_START + offset
            return [backup.crc(full[start + i * backup.BLOCK_SIZE:
                                    start + (i + 1) * backup.BLOCK_SIZE])
                    for i in range(count)]

        with patch.object(backup, "fetch_crcs", side_effect=fetch):
            aligned = backup.scan(None)
            backup.verify_stock_seed(None, aligned, stock)
            self.assertEqual(backup.reconstruct_accessible(None, aligned, stock),
                             full[backup.APP_START:])
            wrong = bytearray(stock)
            wrong[backup.APP_START + 17] ^= 1
            with self.assertRaises(RuntimeError):
                backup.verify_stock_seed(None, aligned, bytes(wrong))


if __name__ == "__main__":
    unittest.main()
