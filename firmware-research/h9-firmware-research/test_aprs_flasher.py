"""Offline checks for the reconstructed APRS updater protocol."""

import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest


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


if __name__ == "__main__":
    unittest.main()
