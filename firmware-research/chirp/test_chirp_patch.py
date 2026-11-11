"""The H9 Toolbox patch to CHIRP's tdh8.py, run against simulated radios (CHIRP itself is stubbed).

    CHIRP_TDH8_ORIGINAL=/path/tdh8.py CHIRP_TDH8_PATCHED=/path/tdh8_h9toolbox.py python3 -I test_chirp_patch.py
"""
import os, sys, types, unittest
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, os.path.join(HERE, "..", "module-loader")]
import chirp_stubs
from test_serial_clone import FakeRadio


class FakeSerial:
    """The pyserial calls the module makes, over a FakeRadio."""
    is_open = True
    name = "fake"
    baudrate = 38400
    timeout = 0.5
    def __init__(self, radio): self.radio = radio; self.log_lines = []
    def write(self, data): self.radio.write(bytes(data))
    def flush(self): pass
    def read(self, n):
        got = self.radio.read(n, 0)
        return got                                    # short or empty = a timeout
    @property
    def in_waiting(self): return len(self.radio.out)
    def reset_input_buffer(self): self.radio.out.clear()
    def reset_output_buffer(self): pass
    def log(self, msg): self.log_lines.append(msg)


def make_radio(module, fake, image):
    return types.SimpleNamespace(
        pipe=FakeSerial(fake), _is_on_ble=False, _idents=[module.TD_H3], ident_mode=b"TDH9\xff\xff\xff\x4e",
        _ranges_main=[(0x0000, 0x1F80), (0x3000, 0x3140)], BLOCKSIZE_UP=0x20, BLOCKSIZE=0x20,
        BAUD_RATE=38400, TIMEOUT=0.5, status_fn=lambda status: None,
        get_mmap=lambda: b"TDH9\xff\xff\xff\x4e" + bytes(image))


def image_for(fake, upper_edits=(), main_edit=True):
    """The radio's current memory with edits (offset, new byte) applied: what CHIRP would upload."""
    image = bytearray(fake.mem)
    if main_edit:
        image[0x0100:0x0120] = bytes(range(32))             # a channel edit in the main region
    for offset, value in upper_edits:
        image[offset] = value
    return image


class ChirpPatch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.patched = chirp_stubs.load(os.environ["CHIRP_TDH8_PATCHED"])
        cls.original = chirp_stubs.load(os.environ["CHIRP_TDH8_ORIGINAL"])

    def upload(self, module, sector_aware, edits, main_edit=True):
        fake = FakeRadio(sector_aware=sector_aware, talk_around=0)
        fake.mem[0x3010:0x3040] = bytes([0x00] * 0x30)       # 0x00 bytes: any set bit is a 0 -> 1 change
        image = image_for(fake, edits, main_edit)
        radio = make_radio(module, fake, image)
        try:
            module._do_upload(radio)
            error = None
        except chirp_stubs.RadioError as e:
            error = str(e)
        return fake, image, error

    def test_a_fixed_radio_takes_every_change(self):
        fake, image, error = self.upload(self.patched, True, [(0x3014, 0x01), (0x3020, 0xFF), (0x3030, 0x0F)])
        self.assertIsNone(error)
        self.assertEqual(bytes(fake.mem[0x0100:0x0120]), bytes(range(32)))
        self.assertEqual(bytes(fake.mem[0x3000:0x3140]), bytes(image[0x3000:0x3140]))

    def test_stock_serial_is_reported_instead_of_passing_silently(self):
        fake, image, error = self.upload(self.patched, False, [(0x3014, 0x01)])
        self.assertIsNotNone(error)
        self.assertIn("0x3000", error)
        self.assertIn("cannot set bits", error)
        self.assertEqual(bytes(fake.mem[0x0100:0x0120]), bytes(range(32)))     # the main region was written fine first

    def test_clearing_bits_needs_no_erase_and_raises_no_false_alarm(self):
        fake, image, error = self.upload(self.patched, False, [(0x3000, 0x00)])    # 0x1a -> 0x00 only clears bits
        self.assertIsNone(error)
        self.assertEqual(fake.mem[0x3000], 0x00)

    def test_the_original_module_fails_silently_on_stock_serial(self):
        fake, image, error = self.upload(self.original, False, [(0x3014, 0x01)])
        self.assertIsNone(error)                                                   # ACKed, no complaint...
        self.assertNotEqual(fake.mem[0x3014], 0x01)                                # ...and the radio kept the old value

    def test_the_original_module_still_works_on_a_fixed_radio(self):
        fake, image, error = self.upload(self.original, True, [(0x3014, 0x01)])
        self.assertIsNone(error)
        self.assertEqual(fake.mem[0x3014], 0x01)

    def test_read_block_names_the_block_on_a_bad_reply(self):
        fake = FakeRadio(sector_aware=True)
        radio = make_radio(self.patched, fake, fake.mem)
        radio.pipe.read = lambda n: b"\x00" * n                                    # a reply with the wrong header
        with self.assertRaises(chirp_stubs.RadioError) as ctx:
            self.patched._read_block(radio, 0x3000, 0x20)
        self.assertIn("0x3000", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
