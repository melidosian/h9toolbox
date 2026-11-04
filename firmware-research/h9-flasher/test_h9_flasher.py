"""Offline safeguards for the TD-H9 flasher; no USB device is accessed."""

import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


MODULE_PATH = Path(__file__).with_name("h9_flasher.py")
spec = importlib.util.spec_from_file_location("h9_flasher", MODULE_PATH)
flasher = importlib.util.module_from_spec(spec)
import sys
sys.modules[spec.name] = flasher
spec.loader.exec_module(flasher)
sys.path.insert(0, str(MODULE_PATH.parent / "vendor"))
from jltech.uboot import JL_MSCProtocolBase
from scsiio.common import SCSIException

SAMPLE_FIRMWARE = Path(os.environ.get("H9_STOCK_FW", "missing-stock-firmware.fw"))
SAMPLE_BACKUP = Path(os.environ.get("H9_PREUPDATE_BACKUP", "missing-preupdate-backup.bin"))


class FakeLoader:
    def __init__(self, size):
        self.flash = bytearray(b"\0" * size)
        self.erases = []
        self.writes = []

    def flash_erase_sector(self, offset):
        assert offset % flasher.ERASE_SIZE == 0
        self.erases.append(offset)
        self.flash[offset:offset + flasher.ERASE_SIZE] = b"\xff" * flasher.ERASE_SIZE

    def flash_write(self, offset, data):
        assert offset // flasher.ERASE_SIZE * flasher.ERASE_SIZE in self.erases
        self.writes.append((offset, len(data)))
        self.flash[offset:offset + len(data)] = data

    def flash_read(self, offset, size):
        return bytes(self.flash[offset:offset + size])


class FlashSafetyTests(unittest.TestCase):
    def test_br23_ram_helper_is_sent_without_second_encryption(self):
        writes = []
        jumps = []

        class Boot:
            def __init__(self, device):
                pass

            def mem_write(self, address, data):
                writes.append((address, data))

            def mem_jump(self, address, argument):
                jumps.append((address, argument))

        class Loader:
            class DevType:
                SPI0_NOR = 3

            def __init__(self, device):
                pass

            def online_device(self):
                return {"type": 3, "id": flasher.FLASH_ID}

        flasher.start_loader(object(), (None, Boot, Loader))
        self.assertEqual(b"".join(data for _, data in writes), flasher.LOADER_FILE.read_bytes())
        self.assertEqual(jumps, [(flasher.LOADER_ADDRESS, flasher.LOADER_ARGUMENT)])

    def test_protocol_rejects_wrong_or_short_command_replies(self):
        class FakeDevice:
            def __init__(self, reply, residue=0):
                self.reply = reply
                self.residue = residue

            def execute(self, cdb, data_out, data_in):
                data_in[:] = self.reply
                return self.residue

        with self.assertRaisesRegex(SCSIException, "Wrong command response"):
            JL_MSCProtocolBase(FakeDevice(b"\x12\x34" + b"\0" * 14)).cmd_exec(0xABCD, b"")
        with self.assertRaisesRegex(SCSIException, "Short command response"):
            JL_MSCProtocolBase(FakeDevice(b"\xAB\xCD" + b"\0" * 14, 1)).cmd_exec(0xABCD, b"")

    def test_erases_writes_and_reads_back_complete_image(self):
        image = bytes(range(256)) * (flasher.ERASE_SIZE * 2 // 256)
        loader = FakeLoader(len(image))
        flasher.flash_image(loader, image, bytes(loader.flash))
        self.assertEqual(loader.erases, [0, flasher.ERASE_SIZE])
        self.assertEqual(len(loader.writes), len(image) // flasher.TRANSFER_SIZE)
        self.assertEqual(flasher.read_flash(loader, len(image)), image)

    def test_matching_boot_sector_is_not_erased(self):
        image = bytes(range(256)) * (flasher.ERASE_SIZE * 2 // 256)
        loader = FakeLoader(len(image))
        loader.flash[:flasher.ERASE_SIZE] = image[:flasher.ERASE_SIZE]
        flasher.flash_image(loader, image, bytes(loader.flash))
        self.assertEqual(loader.erases, [flasher.ERASE_SIZE])
        self.assertEqual(bytes(loader.flash), image)

    def test_failed_sector_verification_stops_before_next_erase(self):
        image = bytes(range(256)) * (flasher.ERASE_SIZE * 2 // 256)

        class BadRead(FakeLoader):
            def flash_read(self, offset, size):
                data = super().flash_read(offset, size)
                return b"\xff" + data[1:] if offset == 0 else data

        loader = BadRead(len(image))
        with self.assertRaisesRegex(flasher.FlashError, "Sector readback mismatch"):
            flasher.flash_image(loader, image, bytes(loader.flash))
        self.assertEqual(loader.erases, [0])

    def test_short_flash_read_stops_before_a_backup_could_be_used(self):
        class ShortRead:
            def flash_read(self, offset, size):
                return b"\0" * (size - 1)

        with self.assertRaisesRegex(flasher.FlashError, "Short flash read"):
            flasher.read_flash(ShortRead(), 256)

    def test_wrong_package_is_rejected_before_hardware_access(self):
        fake = flasher.Firmware("0" * 64, "0" * 64, b"", flasher.FlashHeader(b"", b"", 0), 0)
        with patch.object(flasher, "parse_firmware", return_value=fake):
            with self.assertRaisesRegex(flasher.FlashError, "locked to the supplied"):
                flasher.checked_firmware(Path("unrelated.fw"))

    @unittest.skipUnless(SAMPLE_FIRMWARE.exists(), "Supplied TD-H9 package is not installed")
    def test_custom_backup_manifest_binds_radio_snapshot_and_target(self):
        firmware = flasher.checked_firmware(SAMPLE_FIRMWARE)
        with tempfile.TemporaryDirectory() as temp:
            backup = Path(temp) / "backup.bin"
            backup.write_bytes(firmware.image)
            flasher.ensure_backup_manifest(backup, firmware.image, firmware)
            flasher.require_backup_manifest(backup, firmware.image, firmware)
            changed = bytes([firmware.image[0] ^ 1]) + firmware.image[1:]
            with self.assertRaisesRegex(flasher.FlashError, "manifest does not match"):
                flasher.require_backup_manifest(backup, changed, firmware)
            wrong_target = flasher.Firmware("0" * 64, firmware.image_sha256,
                                            firmware.image, firmware.header, firmware.chipkey)
            with self.assertRaisesRegex(flasher.FlashError, "target firmware"):
                flasher.require_backup_manifest(backup, firmware.image, wrong_target)

    @unittest.skipUnless(SAMPLE_FIRMWARE.exists(), "Supplied TD-H9 package is not installed")
    def test_custom_recovery_requires_manifest_before_hardware_access(self):
        firmware = flasher.checked_firmware(SAMPLE_FIRMWARE)
        with tempfile.TemporaryDirectory() as temp:
            backup = Path(temp) / "backup.bin"
            backup.write_bytes(firmware.image)
            with patch.object(flasher, "load_protocol") as protocol:
                with self.assertRaisesRegex(flasher.FlashError, "valid backup manifest"):
                    flasher.run_flash("fake device", firmware, backup, recover=True, custom=True)
                protocol.assert_not_called()

    @unittest.skipUnless(SAMPLE_FIRMWARE.exists(), "Supplied TD-H9 package is not installed")
    def test_custom_backup_manifest_created_before_any_flash_write(self):
        firmware = flasher.checked_firmware(SAMPLE_FIRMWARE)

        class Device:
            def __init__(self, path):
                pass

            def __enter__(self):
                return self

            def __exit__(self, *args):
                pass

        class Loader:
            def chip_key(self):
                return firmware.chipkey

        with tempfile.TemporaryDirectory() as temp:
            backup = Path(temp) / "before-custom.bin"
            with patch.object(flasher, "load_protocol", return_value=(Device,)), \
                 patch.object(flasher, "require_bootloader"), \
                 patch.object(flasher, "start_loader", return_value=Loader()), \
                 patch.object(flasher, "read_flash", return_value=firmware.image), \
                 patch.object(flasher, "flash_image") as write:
                flasher.run_flash("fake device", firmware, backup, write=False, custom=True)
                write.assert_not_called()
            flasher.require_backup_manifest(backup, backup.read_bytes(), firmware)

    @unittest.skipUnless(SAMPLE_FIRMWARE.exists(), "Supplied TD-H9 package is not installed")
    def test_custom_restore_uses_manifest_bound_backup(self):
        firmware = flasher.checked_firmware(SAMPLE_FIRMWARE)

        class Device:
            def __init__(self, path):
                pass

            def __enter__(self):
                return self

            def __exit__(self, *args):
                pass

        class Loader:
            def chip_key(self):
                return firmware.chipkey

        with tempfile.TemporaryDirectory() as temp:
            backup = Path(temp) / "before-custom.bin"
            backup.write_bytes(firmware.image)
            flasher.ensure_backup_manifest(backup, firmware.image, firmware)
            damaged = b"\xff" * len(firmware.image)
            with patch.object(flasher, "load_protocol", return_value=(Device,)), \
                 patch.object(flasher, "require_bootloader"), \
                 patch.object(flasher, "start_loader", return_value=Loader()), \
                 patch.object(flasher, "read_flash", side_effect=[damaged, firmware.image]), \
                 patch.object(flasher, "flash_image") as write:
                flasher.run_flash("fake device", firmware, backup, recover=True,
                                  restore=True, custom=True)
                write.assert_called_once_with(unittest.mock.ANY, firmware.image, damaged)

    @unittest.skipUnless(SAMPLE_FIRMWARE.exists(), "Supplied TD-H9 package is not installed")
    def test_supplied_firmware_and_corruption(self):
        firmware = flasher.checked_firmware(SAMPLE_FIRMWARE)
        self.assertEqual(len(firmware.image), 0xDA000)
        self.assertEqual(firmware.chipkey, 0xF181)
        self.assertEqual(firmware.header.pid, b"X12345678")
        with tempfile.TemporaryDirectory() as temp:
            bad_path = Path(temp) / "corrupt.fw"
            package = bytearray(SAMPLE_FIRMWARE.read_bytes())
            package[0x200] ^= 1
            bad_path.write_bytes(package)
            with self.assertRaisesRegex(flasher.FlashError, "CRC mismatch"):
                flasher.parse_firmware(bad_path)

    @unittest.skipUnless(SAMPLE_FIRMWARE.exists(), "Supplied TD-H9 package is not installed")
    def test_backup_command_never_erases_and_refuses_changed_radio(self):
        firmware = flasher.checked_firmware(SAMPLE_FIRMWARE)

        class Device:
            def __init__(self, path):
                self.path = path

            def __enter__(self):
                return self

            def __exit__(self, *args):
                pass

        class Loader:
            def chip_key(self):
                return firmware.chipkey

        with tempfile.TemporaryDirectory() as temp:
            backup = Path(temp) / "backup.bin"
            with patch.object(flasher, "load_protocol", return_value=(Device,)), \
                 patch.object(flasher, "require_bootloader"), \
                 patch.object(flasher, "start_loader", return_value=Loader()), \
                 patch.object(flasher, "read_flash", return_value=firmware.image), \
                 patch.object(flasher, "flash_image") as write:
                flasher.run_flash("fake device", firmware, backup, write=False)
                self.assertEqual(backup.read_bytes(), firmware.image)
                write.assert_not_called()
                backup.write_bytes(b"changed")
                with self.assertRaisesRegex(flasher.FlashError, "does not match"):
                    flasher.run_flash("fake device", firmware, backup, write=True)
                write.assert_not_called()

    @unittest.skipUnless(SAMPLE_FIRMWARE.exists(), "Supplied TD-H9 package is not installed")
    def test_resume_requires_exact_backup_before_hardware_access(self):
        firmware = flasher.checked_firmware(SAMPLE_FIRMWARE)
        with tempfile.TemporaryDirectory() as temp:
            backup = Path(temp) / "backup.bin"
            backup.write_bytes(b"invalid")
            with patch.object(flasher, "load_protocol") as protocol:
                with self.assertRaisesRegex(flasher.FlashError, "exact backup"):
                    flasher.run_flash("fake device", firmware, backup, recover=True)
                protocol.assert_not_called()

    @unittest.skipUnless(SAMPLE_FIRMWARE.exists() and SAMPLE_BACKUP.exists(),
                         "Supplied TD-H9 package and radio backup are not installed")
    def test_restore_writes_only_the_verified_radio_backup(self):
        firmware = flasher.checked_firmware(SAMPLE_FIRMWARE)
        saved = SAMPLE_BACKUP.read_bytes()
        self.assertEqual(flasher.sha256(saved).hexdigest(), flasher.BACKUP_SHA256)

        class Device:
            def __init__(self, path):
                pass

            def __enter__(self):
                return self

            def __exit__(self, *args):
                pass

        class Loader:
            def chip_key(self):
                return firmware.chipkey

        damaged = b"\xff" * len(saved)
        with patch.object(flasher, "load_protocol", return_value=(Device,)), \
             patch.object(flasher, "require_bootloader"), \
             patch.object(flasher, "start_loader", return_value=Loader()), \
             patch.object(flasher, "read_flash", side_effect=[damaged, saved]), \
             patch.object(flasher, "flash_image") as write:
            flasher.run_flash("fake device", firmware, SAMPLE_BACKUP, recover=True, restore=True)
            write.assert_called_once_with(unittest.mock.ANY, saved, damaged)


if __name__ == "__main__":
    unittest.main()
