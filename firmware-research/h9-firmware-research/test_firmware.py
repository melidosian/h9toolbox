"""Offline round-trip and integrity checks for the TD-H9 v1.0.38 package."""

from pathlib import Path
from tempfile import TemporaryDirectory
import os
import unittest

import firmware


STOCK = Path(os.environ.get("H9_STOCK_FW", "missing-stock-firmware.fw"))
BACKUP = Path(os.environ.get("H9_PREUPDATE_BACKUP", "missing-preupdate-backup.bin"))


@unittest.skipUnless(STOCK.exists(), "Supplied TD-H9 v1.0.38 package is not installed")
class FirmwareRoundTripTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.stock = STOCK.read_bytes()
        cls.original = firmware.checked_firmware(STOCK)
        cls.decoded = firmware.decode_app_area(cls.original.image)
        _, cls.app_entry, _ = firmware.app_layout(cls.decoded)
        cls.app = bytes(cls.decoded[cls.app_entry.data_offset:
                                    cls.app_entry.data_offset + cls.app_entry.size])

    def test_unmodified_application_rebuilds_exact_package(self):
        self.assertEqual(firmware.rebuild(self.stock, self.app), self.stock)

    def test_modified_string_rebuilds_integrity_without_changing_boot_or_tail(self):
        changed_app = bytearray(self.app)
        self.assertEqual(changed_app[0x94354:0x9435F], b"APRS Switch")
        changed_app[0x94354] = ord("B")
        rebuilt = firmware.rebuild(self.stock, changed_app)
        parsed = firmware.parse_firmware_bytes(rebuilt)
        decoded = firmware.decode_app_area(parsed.image)
        _, app_entry, _ = firmware.app_layout(decoded)
        self.assertEqual(decoded[app_entry.data_offset:app_entry.data_offset + app_entry.size],
                         changed_app)
        self.assertEqual(parsed.image[:firmware.APP_AREA],
                         self.original.image[:firmware.APP_AREA])
        self.assertEqual(rebuilt[0xDA200:], self.stock[0xDA200:])

    def test_replacement_must_fit_existing_application_slot(self):
        with self.assertRaisesRegex(firmware.FormatError, "exactly"):
            firmware.rebuild(self.stock, self.app + b"X")

    def test_custom_flasher_accepts_only_exact_app_only_repack(self):
        changed_app = bytearray(self.app)
        changed_app[0x94354] = ord("B")
        rebuilt = firmware.rebuild(self.stock, changed_app)
        with TemporaryDirectory() as temp:
            candidate = Path(temp) / "candidate.fw"
            candidate.write_bytes(rebuilt)
            verified = firmware.validate_repacked(STOCK, candidate)
            self.assertEqual(verified.image, firmware.parse_firmware_bytes(rebuilt).image)
            altered = bytearray(rebuilt)
            altered[0x1F0] ^= 1  # outer padding, outside the app and CRC records
            candidate.write_bytes(altered)
            with self.assertRaisesRegex(firmware.FormatError, "app-only rebuild"):
                firmware.validate_repacked(STOCK, candidate)

    @unittest.skipUnless(BACKUP.exists(), "Pre-update radio backup is not installed")
    def test_preupdate_backup_extracts_old_app_and_vm_overlap(self):
        with TemporaryDirectory() as temp:
            output = Path(temp) / "preupdate"
            firmware.extract_backup(BACKUP, output)
            self.assertEqual(len((output / "app.bin").read_bytes()), 795244)
            vm = (output / "old-vm-overlap.bin").read_bytes()
            self.assertEqual(len(vm), 0x12000)
            self.assertIn(b"TD-H9-", vm[:0x1000])


if __name__ == "__main__":
    unittest.main()
