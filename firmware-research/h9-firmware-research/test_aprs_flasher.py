"""Offline checks for the reconstructed APRS updater protocol."""

import importlib.util
import contextlib
import io
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch


MODULE = Path(__file__).with_name("aprs_flasher.py")
SPEC = importlib.util.spec_from_file_location("aprs_flasher", MODULE)
assert SPEC and SPEC.loader
aprs = importlib.util.module_from_spec(SPEC)
import sys
sys.modules[SPEC.name] = aprs
SPEC.loader.exec_module(aprs)


def ack(status: int) -> bytes:
    core = b"TID\x00\x01" + bytes((status,))
    return core + bytes((aprs.checksum(core[3:]),))


class FakePort:
    def __init__(self, replies):
        self.replies = bytearray().join(replies)
        self.writes = []

    def write(self, packet):
        self.writes.append(packet)
        return len(packet)

    def read(self, size):
        chunk = bytes(self.replies[:size])
        del self.replies[:size]
        return chunk

    def reset_input_buffer(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *_):
        return None


class AprsFlasherTests(unittest.TestCase):
    def test_vendor_write_frame_layout(self):
        packet = aprs.write_frame(0x12345678, b"\x01\x02\x03")
        self.assertEqual(packet, b"TID\x00\x08\x03\x12\x34\x56\x78\x01\x02\x03\x25")

    def test_reply_validation_matches_observed_vendor_fields(self):
        aprs.check_reply(ack(0x13), 0x13)
        with self.assertRaises(aprs.AprsFlashError):
            aprs.check_reply(ack(0x10), 0x13)
        with self.assertRaises(aprs.AprsFlashError):
            aprs.check_reply(ack(0x13)[:-1], 0x13)
        # Form4 does not inspect these fields in its write/exit ACKs.
        aprs.check_reply(b"\x55\x66\x77\x88\x99\x13\x00", 0x13)
        aprs.check_reply(b"\x55\x66\x77\x88\x99\x10\x00", 0x10)

    def test_entire_stock_shape_in_simulated_serial_transfer(self):
        data = bytearray(aprs.IMAGE_SIZE)
        struct.pack_into("<II", data, 0, 0x20001A50, 0x080020D5)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "aprs.bin"
            path.write_bytes(data)
            image = aprs.inspect_image(path)
        port = FakePort([ack(0x13)] * 136 + [ack(0x10)])
        aprs.transfer(port, image, progress=lambda _: None)
        self.assertEqual(len(port.writes), 137)
        self.assertEqual(port.writes[-1], aprs.EXIT_FRAME)
        self.assertEqual(port.writes[-2][4], 29)  # final 24-byte payload + 5
        self.assertEqual(port.writes[-2][6:10], (135 * 128).to_bytes(4, "big"))

    def test_failed_block_does_not_send_exit(self):
        image = aprs.Image(b"A" * 129, "", 0, 0)
        port = FakePort([ack(0x13), ack(0x10)])
        with self.assertRaises(aprs.AprsFlashError):
            aprs.transfer(port, image, progress=lambda _: None)
        self.assertEqual(len(port.writes), 2)
        self.assertNotIn(aprs.EXIT_FRAME, port.writes)

    def test_transfer_journal_records_acks_and_failure_boundary(self):
        image = aprs.Image(b"A" * 129, "test-digest", 0, 0)
        port = FakePort([ack(0x13), ack(0x10)])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "attempt.jsonl"
            with self.assertRaisesRegex(aprs.AprsFlashError,
                                        "image offset 0x80"):
                with aprs.FlashJournal(path, image, "COM7", 38400) as journal:
                    aprs.transfer(port, image, progress=lambda _: None,
                                  on_block_ack=journal.block_acknowledged,
                                  on_exit_ack=journal.exit_acknowledged)
            rows = [json.loads(line) for line in path.read_text().splitlines()]
            self.assertEqual([row["event"] for row in rows], [
                "attempt_started", "block_acknowledged", "attempt_failed"
            ])
            self.assertEqual(rows[1]["acknowledged_end"], 128)
            self.assertEqual(rows[2]["acknowledged_end"], 128)
            self.assertEqual(rows[0]["image_sha256"], "test-digest")
            self.assertNotIn(aprs.EXIT_FRAME, port.writes)
            with self.assertRaises(FileExistsError):
                with aprs.FlashJournal(path, image, "COM7", 38400):
                    pass

    def test_transfer_journal_distinguishes_exit_ack(self):
        image = aprs.Image(b"A" * 129, "test-digest", 0, 0)
        port = FakePort([ack(0x13), ack(0x13), ack(0x10)])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "attempt.jsonl"
            with aprs.FlashJournal(path, image, "/dev/ttyUSB0", 38400) as journal:
                aprs.transfer(port, image, progress=lambda _: None,
                              on_block_ack=journal.block_acknowledged,
                              on_exit_ack=journal.exit_acknowledged)
            rows = [json.loads(line) for line in path.read_text().splitlines()]
            self.assertEqual([row["event"] for row in rows], [
                "attempt_started", "block_acknowledged", "block_acknowledged",
                "exit_acknowledged", "transfer_acknowledged"
            ])
            self.assertEqual(rows[-1]["acknowledged_end"], 129)

    def test_journal_close_without_exit_ack_is_incomplete(self):
        image = aprs.Image(b"A" * 128, "test-digest", 0, 0)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "attempt.jsonl"
            with aprs.FlashJournal(path, image, "COM7", 38400) as journal:
                journal.block_acknowledged(0, 128)
            rows = [json.loads(line) for line in path.read_text().splitlines()]
            self.assertEqual(rows[-1]["event"], "attempt_incomplete")
            self.assertEqual(rows[-1]["acknowledged_end"], 128)

    def test_flash_cli_writes_new_journal_with_simulated_port(self):
        data = bytearray(aprs.IMAGE_SIZE)
        struct.pack_into("<II", data, 0, 0x20001A50, 0x080020D5)
        port = FakePort([ack(0x13)] * 136 + [ack(0x10)])
        with tempfile.TemporaryDirectory() as directory:
            image_path = Path(directory) / "test.bin"
            journal_path = Path(directory) / "attempt.jsonl"
            image_path.write_bytes(data)
            output = io.StringIO()
            serial_module = SimpleNamespace(Serial=lambda *args, **kwargs: port)
            with patch.dict(sys.modules, {"serial": serial_module}):
                with contextlib.redirect_stdout(output):
                    result = aprs.main([
                        "flash", str(image_path), "--port", "COM7", "--yes",
                        "--allow-modified", "--journal", str(journal_path)
                    ])
            self.assertEqual(result, 0)
            self.assertIn(str(journal_path), output.getvalue())
            self.assertEqual(port.writes[-1], aprs.EXIT_FRAME)
            rows = [json.loads(line) for line in journal_path.read_text().splitlines()]
            self.assertEqual(len(rows), 139)
            self.assertEqual(rows[-1]["event"], "transfer_acknowledged")
            self.assertEqual(rows[-1]["acknowledged_end"], aprs.IMAGE_SIZE)


if __name__ == "__main__":
    unittest.main()
