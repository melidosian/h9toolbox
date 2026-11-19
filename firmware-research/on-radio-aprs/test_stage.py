"""Test safe USB staging against a simulated H9 flash."""
from pathlib import Path
import unittest
from unittest.mock import patch

import stage


class StageTests(unittest.TestCase):
    def setUp(self):
        self.flash = bytearray(b"\xff" * stage.SIZE)
        self.bundle = bytes(range(256)) * (stage.SIZE // 256)
        self.device = object()
        self.writes = []
        for target, attribute, side_effect in (
            (stage.hid, "hid_crcs", self.crcs),
            (stage, "fetch_crcs", self.shifted),
            (stage.hid, "hid_write", self.write),
        ):
            mock = patch.object(target, attribute, side_effect=side_effect)
            mock.start()
            self.addCleanup(mock.stop)

    def crcs(self, device, physical, count):
        self.assertIs(device, self.device)
        offset = physical - stage.ADDRESS
        size = stage.hid.BLOCK_SIZE
        self.assertGreaterEqual(offset, 0)
        self.assertLessEqual(offset + count * size, len(self.flash))
        return [stage.hid.crc(self.flash[pos:pos + size])
                for pos in range(offset, offset + count * size, size)]

    def shifted(self, device, offset, count):
        return self.crcs(device, offset + stage.hid.APP_START, count)

    def write(self, device, address, data):
        self.assertIs(device, self.device)
        self.writes.append(address)
        offset = address - stage.ADDRESS
        self.assertGreaterEqual(offset, 0)
        self.assertLessEqual(offset + len(data), len(self.flash))
        self.assertEqual(bytes(self.flash[offset:offset + len(data)]), b"\xff" * len(data))
        self.flash[offset:offset + len(data)] = data

    def test_inspect_and_resumable_stage(self):
        size = stage.hid.BLOCK_SIZE
        self.flash[size:2 * size] = self.bundle[size:2 * size]
        stage.write_stage(self.device, self.bundle, write=False)
        self.assertEqual(self.writes, [])
        stage.write_stage(self.device, self.bundle, write=True)
        self.assertEqual(self.flash, self.bundle)
        self.assertEqual(self.writes[0], stage.ADDRESS + stage.SIZE - size)
        self.assertTrue(all(not stage.ADDRESS + size <= addr < stage.ADDRESS + 2 * size
                            for addr in self.writes))
        previous = len(self.writes)
        stage.write_stage(self.device, self.bundle, write=True)
        self.assertEqual(len(self.writes), previous)

    def test_refuses_occupied_sector(self):
        self.flash[-2] = 0
        with self.assertRaisesRegex(RuntimeError, "contains other data"):
            stage.write_stage(self.device, self.bundle, write=True)
        self.assertEqual(self.writes, [])

    def test_refuses_failed_shifted_check(self):
        with patch.object(stage, "fetch_crcs", return_value=[0] * (stage.SIZE // stage.hid.BLOCK_SIZE - 1)):
            with self.assertRaisesRegex(RuntimeError, "Staging window differs"):
                stage.write_stage(self.device, self.bundle, write=True)
        self.assertEqual(self.writes, [])

    def test_failed_write_stops_before_executable(self):
        def lose_write(device, address, data):
            self.writes.append(address)

        with patch.object(stage.hid, "hid_write", side_effect=lose_write):
            with self.assertRaisesRegex(RuntimeError, "verification failed"):
                stage.write_stage(self.device, self.bundle, write=True)
        self.assertTrue(self.writes)
        self.assertGreaterEqual(min(self.writes), stage.ADDRESS + stage.SIZE - stage.hid.BLOCK_SIZE)

    def test_built_stage_matches_manifest(self):
        output = Path(__file__).resolve().parent / "build"
        if not (output / "manifest.json").exists():
            self.skipTest("Run build.py for the artifact check")
        bundle = stage.load_stage(output)
        stage.write_stage(self.device, bundle, write=True)
        self.assertEqual(self.flash, bundle)


if __name__ == "__main__":
    unittest.main()
