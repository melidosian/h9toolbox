"""Offline tests for the USB resource route; no radio or USB device is used."""

from __future__ import annotations

import argparse
import contextlib
import io
import json
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import resource_usb
import rom
import usb_msc

rom.vendor()
from jltech.crc import jl_crc16  # noqa: E402 - on the path once rom.vendor() has run


class FakeRom:
    """SCSI-level stand-in for the BR23 mask ROM (UBOOT1.00)."""

    def __init__(self, reads_encrypted=True, jump_returns=True, product="UBOOT1.00", spi2=None):
        # Matches this radio: WRITE_MEMORY decrypts, READ_MEMORY encrypts.
        self.ram = bytearray(rom.RAM_WINDOW.stop - rom.RAM_WINDOW.start)
        self.reads_encrypted, self.jump_returns, self.product = reads_encrypted, jump_returns, product
        self.jumps = []
        # spi2: list of (status, jedec) answers the SPI2 program gives per run.
        self.spi2 = list(spi2 or [])
        self.spi2_ops = []
        self.spi2_masks = []
        self.closed = False

    def _span(self, address, length):
        start = address - rom.RAM_WINDOW.start
        assert 0 <= start and start + length <= len(self.ram), hex(address)
        return start

    def execute(self, cdb, data_out, data_in, max_sense_len=32, return_sense_buffer=False):
        cdb = bytes(cdb)
        if cdb[0] == 0x12:
            reply = b"\0" * 8 + b"BR23".ljust(8) + self.product.encode().ljust(16) + b"1.00"
            data_in[:36] = reply
            return 0
        command = int.from_bytes(cdb[:2], "big")
        address = int.from_bytes(cdb[2:6], "big")
        if command == 0xFB06:
            length = int.from_bytes(cdb[6:8], "big")
            assert len(data_out) == length
            assert int.from_bytes(cdb[9:11], "little") == jl_crc16(data_out)
            payload = rom.encrypt(bytes(data_out))
            start = self._span(address, length)
            self.ram[start:start + length] = payload
            return 0
        if command == 0xFD07:
            length = int.from_bytes(cdb[6:8], "big")
            start = self._span(address, length)
            stored = bytes(self.ram[start:start + length])
            data_in[:length] = rom.encrypt(stored) if self.reads_encrypted else stored
            return 0
        if command == 0xFB08:
            argument = int.from_bytes(cdb[6:8], "big")
            self.jumps.append((address, argument))
            if not self.jump_returns:
                raise TimeoutError("device stopped answering")
            spi2_code = resource_usb.program("spi2_rom")
            if bytes(self.ram[self._span(address, len(spi2_code)):][:len(spi2_code)]) == spi2_code:
                block = self._span(resource_usb.PARAMS, 32)
                assert self.ram[block:block + 4] == b"H9U2"
                op = int.from_bytes(self.ram[block + 4:block + 8], "little")
                self.spi2_ops.append(op)
                self.spi2_masks.append(struct.unpack_from("<II", self.ram, block + 32))
                if op == 3:
                    status, jedec = 0, 0
                    snapshot = self._span(resource_usb.BUFFER, resource_usb.SNAPSHOT_WORDS * 4)
                    words = [0] * resource_usb.SNAPSHOT_WORDS
                    words[16 + 2] = 0x00000200  # bank 1 direction: pin 25 input
                    words[16 + 3] = 0x00000080  # bank 1 input enable: pin 23
                    words[72] = 0x0801          # SPI0 control: enabled, width bits = 2
                    words[76] = 0x40B1          # SPI2 control
                    self.ram[snapshot:snapshot + len(words) * 4] = struct.pack(f"<{len(words)}I", *words)
                else:
                    status, jedec = self.spi2.pop(0)
                count = int.from_bytes(self.ram[block + 28:block + 32], "little") + 1
                self.ram[block + 20:block + 32] = struct.pack("<III", status, jedec, count)
                data_in[:16] = cdb[:2] + bytes(14)
                return 0
            program = bytes(self.ram[self._span(address, 24):][:24])
            if program != resource_usb.program("u1_marker"):
                raise ConnectionError("garbage executed; device reset")
            marker = self._span(resource_usb.MARKER, 16)
            counter = int.from_bytes(self.ram[marker + 12:marker + 16], "little") + 1
            self.ram[marker:marker + 16] = struct.pack("<4sIII", b"H9U1", argument, 0x1234, counter)
            data_in[:16] = cdb[:2] + bytes(14)
            return 0
        raise AssertionError(f"unexpected command {cdb.hex()}")

    def close(self):
        self.closed = True


def run_command(fake, command, **options):
    with tempfile.TemporaryDirectory() as temp:
        report = Path(temp) / "report.json"
        args = argparse.Namespace(device="4C4A:2342", scsi=None, report=str(report), **options)
        with mock.patch.object(resource_usb, "open_transport", return_value=fake), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            status = command(args)
        return status, json.loads(report.read_text())


def run_u1(fake, stage="full"):
    with tempfile.TemporaryDirectory() as temp:
        report = Path(temp) / "u1.json"
        args = argparse.Namespace(device="4C4A:4155", scsi=None, report=str(report), stage=stage)
        with mock.patch.object(resource_usb, "open_transport", return_value=fake), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            status = resource_usb.command_u1(args)
        return status, json.loads(report.read_text())


class ProgramTests(unittest.TestCase):
    def test_programs_assemble_to_pinned_bytes(self):
        for name in resource_usb.PROGRAMS:
            code = resource_usb.program(name)
            self.assertLessEqual(rom.PROGRAM_ADDRESS + len(code), resource_usb.MARKER)

    def test_cipher_round_trip_matches_flasher_loader(self):
        loader = (rom.VENDOR_DIR / "data" / "br23loader.bin").read_bytes()
        plain = rom.encrypt(loader[:512])
        self.assertEqual(plain[:2], bytes.fromhex("d8e8"))
        self.assertEqual(rom.encrypt(plain), loader[:512])

    def test_ram_window_is_enforced(self):
        session = rom.RomSession(FakeRom())
        with self.assertRaises(ValueError):
            session.write(0x11FFF, b"x")
        with self.assertRaises(ValueError):
            session.read_raw(rom.RAM_WINDOW.stop - 4, 8)
        with self.assertRaises(ValueError):
            session.jump(0x2000000, 0)


class U1Tests(unittest.TestCase):
    def test_probe_sends_only_inquiry(self):
        fake = FakeRom()
        fake.execute = mock.Mock(wraps=fake.execute)
        status, report = run_u1(fake, "probe")
        self.assertEqual(status, 0)
        self.assertEqual([call.args[0][0] for call in fake.execute.call_args_list], [0x12])
        self.assertEqual(report["steps"], [{"step": "inquiry", "device": "BR23 UBOOT1.00 1.00"}])

    def test_read_stage_never_writes_or_jumps(self):
        fake = FakeRom()
        fake.execute = mock.Mock(wraps=fake.execute)
        status, report = run_u1(fake, "read")
        self.assertEqual(status, 0)
        commands = {bytes(call.args[0][:2]).hex() for call in fake.execute.call_args_list}
        self.assertEqual(commands, {"1200", "fd07"})
        self.assertEqual([s["address"] for s in report["steps"][1:]], ["0x12000", "0x13000"])
        self.assertTrue(all(s["repeat_matches"] for s in report["steps"][1:]))

    def test_pass_when_jump_returns(self):
        fake = FakeRom()
        status, report = run_u1(fake)
        self.assertEqual(status, 0)
        self.assertTrue(report["result"].startswith("pass"))
        self.assertEqual(fake.jumps, [(0x12000, 0x4839)] * 2)
        self.assertEqual([s["counter"] for s in report["steps"] if s["step"].startswith("jump")], [1, 2])
        self.assertIn({"step": "cipher", "mode": "writes decrypted, reads encrypted"}, report["steps"])
        self.assertTrue(fake.closed)

    def test_rom_with_raw_reads(self):
        status, report = run_u1(FakeRom(reads_encrypted=False))
        self.assertEqual(status, 0)
        self.assertIn({"step": "cipher", "mode": "writes decrypted, reads raw"}, report["steps"])

    def test_writes_are_never_sent_unencrypted(self):
        # Regression for 2026-11-08: a read-back check that assumed raw reads
        # sent the program unencrypted, so the ROM ran garbage and reset.
        fake = FakeRom()
        fake.execute = mock.Mock(wraps=fake.execute)
        status, _ = run_u1(fake)
        self.assertEqual(status, 0)
        program = resource_usb.program("u1_marker")
        sent = [bytes(call.args[1]) for call in fake.execute.call_args_list if call.args[1]]
        self.assertIn(rom.encrypt(program), sent)
        self.assertNotIn(program, sent)

    def test_jump_without_return_is_reported(self):
        fake = FakeRom(jump_returns=False)
        status, report = run_u1(fake)
        self.assertEqual(status, 1)
        self.assertIn("does not return", report["result"])
        self.assertEqual(len(fake.jumps), 1)
        self.assertTrue(fake.closed)

    def test_other_device_gets_no_vendor_commands(self):
        fake = FakeRom(product="UDISK")
        status, report = run_u1(fake)
        self.assertEqual(status, 1)
        self.assertIn("Refusing device", report["result"])
        self.assertEqual(fake.jumps, [])
        self.assertEqual(fake.ram, bytearray(len(fake.ram)))


class U2Tests(unittest.TestCase):
    def run_u2(self, fake, divider=31, op="jedec"):
        return run_command(fake, resource_usb.command_u2, divider=divider, op=op)

    def test_register_snapshot_is_decoded_per_pin(self):
        fake = FakeRom()
        status, report = self.run_u2(fake, op="regs")
        self.assertEqual(status, 0)
        self.assertEqual(fake.spi2_ops, [3])
        pin25 = next(s for s in report["steps"] if s["step"] == "gpio 25")
        self.assertEqual(pin25["dir"], 1)
        pin23 = next(s for s in report["steps"] if s["step"] == "gpio 23")
        self.assertEqual(pin23["ie"], 1)
        spi0 = next(s for s in report["steps"] if s["step"] == "spi0")
        self.assertEqual((spi0["control"], spi0["width_bits_10_11"]), ("00000801", 2))
        spi2 = next(s for s in report["steps"] if s["step"] == "spi2")
        self.assertEqual(spi2["control"], "000040B1")
        self.assertIn("53", {s["step"][5:] for s in report["steps"] if s["step"].startswith("gpio ")})

    def test_bitbang_swap_sends_swapped_data_pins(self):
        fake = FakeRom(spi2=[(0, 0xEF4018), (0, 0xEF4018)])
        status, report = self.run_u2(fake, op="bitbang-swap")
        self.assertEqual(status, 0, report["result"])
        self.assertEqual(fake.spi2_ops, [5, 5])
        self.assertEqual(fake.spi2_masks, [(0x200, 0x400)] * 2)

    def test_sample_op_reports_changing_pins(self):
        fake = FakeRom(spi2=[(0, 0xFFFFFF), (0, 0xFFFFFF)])
        original = fake.execute

        def execute(cdb, data_out, data_in, *rest):
            result = original(cdb, data_out, data_in, *rest)
            if bytes(cdb[:2]) == b"\xfb\x08":
                # MISO on GPIO 21 answers 0xEF in the first response byte.
                start = fake._span(resource_usb.SAMPLES, 33 * 4)
                words = [0x0FCF] * 33
                for bit in range(8):
                    if 0xEF >> (7 - bit) & 1 == 0:
                        words[9 + bit] &= ~(1 << 5)
                    words[9 + bit] |= 1 << 5 if 0xEF >> (7 - bit) & 1 else 0
                fake.ram[start:start + 33 * 4] = struct.pack("<33I", *words)
            return result

        fake.execute = execute
        status, report = self.run_u2(fake, op="sample")
        self.assertEqual(status, 0, report["result"])
        self.assertEqual(fake.spi2_ops, [6, 6])
        samples = next(s for s in report["steps"] if s["step"] == "sample 1 samples")
        self.assertEqual(samples["gpio_changing"], [21])
        self.assertEqual(samples["per_pin_response_bits"]["21"][:8], "11101111")
        self.assertIn("samples recorded", report["result"])

    def test_led_op_runs_once(self):
        fake = FakeRom(spi2=[(0, 0)])
        status, report = self.run_u2(fake, op="led")
        self.assertEqual(status, 0, report["result"])
        self.assertEqual(fake.spi2_ops, [7])
        self.assertIn("top LED blinked twice", report["result"])

    def test_bitbang_and_pins_ops_are_sent(self):
        for op, code in (("bitbang", 5), ("pins", 4)):
            fake = FakeRom(spi2=[(0, 0xEF4018), (0, 0xEF4018)])
            status, report = self.run_u2(fake, op=op)
            self.assertEqual(status, 0, report["result"])
            self.assertEqual(fake.spi2_ops, [code, code])
            self.assertIn(f"{op}: reaches a 16 MiB flash", report["result"])

    def test_parameter_block_matches_program_layout(self):
        block = resource_usb.spi2_parameters(1, 0x900000, 16, 31)
        self.assertEqual(len(block), 40)
        self.assertEqual(block[:4], b"H9U2")
        self.assertEqual(struct.unpack_from("<IIII", block, 4), (1, 0x900000, 16, 31))
        self.assertEqual(block[20:32], bytes(12))
        self.assertEqual(struct.unpack_from("<II", block, 32), (0x400, 0x200))

    def test_pass_with_16_mib_chip(self):
        fake = FakeRom(spi2=[(0, 0xEF4018), (0, 0xEF4018)])
        status, report = self.run_u2(fake)
        self.assertEqual(status, 0)
        self.assertIn("JEDEC ID EF4018", report["result"])
        chip = next(s for s in report["steps"] if s["step"] == "chip")
        self.assertEqual(chip["bytes"], 16 * 1024 * 1024)
        self.assertEqual(len(fake.jumps), 2)
        block = fake._span(resource_usb.PARAMS, 32)
        self.assertEqual(int.from_bytes(fake.ram[block + 16:block + 20], "little"), 31)

    def test_spi2_timeout_is_explained(self):
        status, report = self.run_u2(FakeRom(spi2=[(2, 0)]))
        self.assertEqual(status, 1)
        self.assertIn("clock or port mapping", report["result"])

    def test_silent_bus_is_refused(self):
        status, report = self.run_u2(FakeRom(spi2=[(0, 0xFFFFFF), (0, 0xFFFFFF)]))
        self.assertEqual(status, 1)
        self.assertIn("nothing answered", report["result"])

    def test_wrong_capacity_is_refused(self):
        status, report = self.run_u2(FakeRom(spi2=[(0, 0xEF4017), (0, 0xEF4017)]))
        self.assertEqual(status, 1)
        self.assertIn("not 16 MiB", report["result"])

    def test_unstable_id_is_refused(self):
        status, report = self.run_u2(FakeRom(spi2=[(0, 0xEF4018), (0, 0xC84018)]))
        self.assertEqual(status, 1)
        self.assertIn("changed between reads", report["result"])

    def test_program_that_does_not_return(self):
        fake = FakeRom(jump_returns=False)
        status, report = self.run_u2(fake)
        self.assertEqual(status, 1)
        self.assertIn("did not answer", report["result"])


class FlasherCopyTests(unittest.TestCase):
    def test_flasher_transport_matches_tested_copy(self):
        flasher_copy = rom.ROOT.parent / "h9-flasher" / "vendor" / "scsiio" / "usb_msc.py"
        self.assertEqual(flasher_copy.read_bytes(), (rom.ROOT / "usb_msc.py").read_bytes())


class BulkOnlyTests(unittest.TestCase):
    def test_cbw_layout(self):
        cbw = usb_msc.build_cbw(7, 16, True, bytes.fromhex("fd07"))
        self.assertEqual(len(cbw), 31)
        self.assertEqual(cbw[:4], b"USBC")
        self.assertEqual(struct.unpack_from("<IIBBB", cbw, 4), (7, 16, 0x80, 0, 2))
        self.assertEqual(cbw[15:17], bytes.fromhex("fd07"))
        with self.assertRaises(ValueError):
            usb_msc.build_cbw(1, 0, False, bytes(17))

    def test_csw_checks_signature_and_tag(self):
        csw = struct.pack("<4sIIB", b"USBS", 9, 3, 0)
        self.assertEqual(usb_msc.parse_csw(csw, 9), (3, 0))
        with self.assertRaises(usb_msc.TransportError):
            usb_msc.parse_csw(csw, 8)
        with self.assertRaises(usb_msc.TransportError):
            usb_msc.parse_csw(b"USBX" + csw[4:], 9)

    def test_device_argument(self):
        self.assertEqual(usb_msc.parse_device("4c4a:4155"), (0x4C4A, 0x4155))
        with self.assertRaises(ValueError):
            usb_msc.parse_device("4c4a")


@unittest.skipUnless(__import__("importlib").util.find_spec("usb"), "pyusb not installed")
class BulkOnlyExecuteTests(unittest.TestCase):
    class Endpoint:
        def __init__(self, replies=()):
            self.writes, self.replies = [], list(replies)

        def write(self, data, timeout):
            self.writes.append(bytes(data))
            return len(data)

        def read(self, length, timeout):
            return self.replies.pop(0)

    def device(self, replies):
        device = usb_msc.BulkOnlyDevice.__new__(usb_msc.BulkOnlyDevice)
        device.tag, device.is_open, device.csw_notes = 0, True, []
        device.ep_out, device.ep_in = self.Endpoint(), self.Endpoint(replies)
        return device

    def test_data_in_command(self):
        device = self.device([b"A" * 16, struct.pack("<4sIIB", b"USBS", 1, 0, 0)])
        data = bytearray(16)
        self.assertEqual(device.execute(bytes.fromhex("fd07") + bytes(14), None, data), 0)
        self.assertEqual(data, b"A" * 16)
        self.assertEqual(device.ep_out.writes[0][12], 0x80)

    def test_data_out_residue_follows_bytes_carried(self):
        # The BR23 ROM reported a nonzero CSW residue for a fully sent FB06 write.
        device = self.device([struct.pack("<4sIIB", b"USBS", 1, 16, 0)])
        self.assertEqual(device.execute(bytes.fromhex("fb06") + bytes(14), bytes(16), None), 0)
        self.assertEqual(device.csw_notes, [{"command": "fb06", "length": 16, "transferred": 16, "csw_residue": 16}])

    def test_data_out_command_and_failure_status(self):
        device = self.device([struct.pack("<4sIIB", b"USBS", 1, 0, 1)])
        with self.assertRaises(usb_msc.CommandFailed):
            device.execute(bytes.fromhex("fb06") + bytes(14), b"xyz", None)
        self.assertEqual(device.ep_out.writes[1], b"xyz")
        self.assertEqual(device.ep_out.writes[0][12], 0x00)


if __name__ == "__main__":
    unittest.main()
