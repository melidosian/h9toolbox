#!/usr/bin/env python3
"""Does this radio's serial / USB-C clone write really set bits above 0x3000?

The stock firmware's serial clone write only programs flash, so over a K-plug cable or USB-C serial a write to
0x3000-0x3FFF can clear bits (1 -> 0) but not set them (0 -> 1); Bluetooth uses a sector-aware routine and works
(resource-flash-log.md, 2026-11-11). H9 Toolbox Firmware (build b0135e0c and later) sends 0x2000-0x3FFF through the
Bluetooth routine. This tool checks which behaviour a radio has, harmlessly:

  * reads the 32-byte block at 0x3000 (the settings block that holds "Talk around", 0x3014 bit 0),
  * writes it back with that one bit set to 0, then to 1 (the 0 -> 1 change is the one stock serial cannot do),
    reading the block back each time,
  * always restores the original block, then leaves programming mode.

--range does what CHIRP does with the extended settings: it reads 0x3000-0x311F (9 blocks), writes all of them back to
back (the first with the Talk around bit toggled, the rest with exactly what they hold), reads them all back, compares,
restores, and reports how long each ACK took (clients time out if it is too slow).

    python3 -I serial_clone_test.py /dev/cu.usbserial-XXXX          (needs: pip install pyserial)
    python3 -I serial_clone_test.py COM5 --read-only                (just read 0x3000-0x311F)
    python3 -I serial_clone_test.py PORT --range                    (the whole-range test, as CHIRP writes it)

Use the normal radio (not a firmware-update mode), radio on, K-plug or USB-C connected, 115200 baud. Quit CHIRP or
any other program that holds the port. Protocol as the browser programmer (dist/app.js) speaks it."""

from __future__ import annotations

import argparse
import sys
import time

MAGIC = bytes([0x50, 0x56, 0x4F, 0x4A, 0x48, 0x5C, 0x14])
BLOCK = 0x20
TEST_ADDRESS, TEST_BYTE, TEST_MASK = 0x3000, 0x14, 0x01     # "Talk around": a harmless setting
MODES = {0x4E: "Normal", 0x48: "Ham", 0x47: "GMRS"}


class Radio:
    """The clone protocol over any object with write(bytes) and read(n, timeout) (a pyserial port, or a fake)."""

    def __init__(self, port):
        self.port = port

    def read_exact(self, n, timeout=2.0):
        out = bytearray()
        end = time.monotonic() + timeout
        while len(out) < n and time.monotonic() < end:
            out += self.port.read(n - len(out), 0.05)
        if len(out) < n:
            raise TimeoutError(f"the radio sent {len(out)} of {n} bytes")
        return bytes(out)

    def expect_ack(self, what):
        got = self.read_exact(1)
        if got != b"\x06":
            raise RuntimeError(f"{what}: expected ACK 06, got {got.hex()}")

    def enter(self):
        self.port.write(MAGIC)
        self.expect_ack("handshake")
        self.port.write(b"\x02")
        first = self.read_exact(1)
        for _ in range(4):
            if first != b"\x06":
                break
            first = self.read_exact(1)
        ident = first + self.read_exact(7)
        if ident[:7] != b"TDH9\xff\xff\xff" or ident[7] not in MODES:
            raise RuntimeError(f"unexpected identity {ident.hex()}")
        self.port.write(b"\x06")
        self.expect_ack("programming mode")
        return MODES[ident[7]]

    def exit(self):
        try:
            self.port.write(b"\x45")
        except Exception:
            pass

    def read_block(self, address):
        self.port.write(bytes([0x52, address >> 8, address & 255, BLOCK]))
        head = self.read_exact(4)
        if head != bytes([0x57, address >> 8, address & 255, BLOCK]):
            raise RuntimeError(f"bad read header at 0x{address:04x}: {head.hex()}")
        data = self.read_exact(BLOCK)
        try:
            self.read_exact(1, 0.15)           # the firmware's checksum byte, if it sends one
        except TimeoutError:
            pass
        return data

    def write_block(self, address, data):
        """Writes one block and waits for the ACK; returns the seconds the ACK took."""
        assert len(data) == BLOCK
        start = time.monotonic()
        self.port.write(bytes([0x57, address >> 8, address & 255, BLOCK]) + data + bytes([sum(data) & 255]))
        self.expect_ack(f"write at 0x{address:04x}")
        return time.monotonic() - start


def check(radio, say=print):
    """Returns (ok, message). Restores the block it changed (if it can)."""
    original = radio.read_block(TEST_ADDRESS)
    was = original[TEST_BYTE] & TEST_MASK
    say(f"block 0x{TEST_ADDRESS:04x} byte 0x{TEST_BYTE:02x} = 0x{original[TEST_BYTE]:02x} (Talk around bit {1 if was else 0})")
    sets = []                                   # did each 0 -> 1 write take effect?
    steps = [0, TEST_MASK] if was else [TEST_MASK]      # when the bit is set, clear it first: the 0 -> 1 write is then the restore
    try:
        for bit in steps:
            want = bytearray(original)
            want[TEST_BYTE] = (want[TEST_BYTE] & ~TEST_MASK) | bit
            radio.write_block(TEST_ADDRESS, bytes(want))
            got = radio.read_block(TEST_ADDRESS)
            if bit:
                sets.append(got == bytes(want))
            say(f"  write bit {1 if bit else 0}: read back {'OK' if got == bytes(want) else 'DIFFERENT'} (byte 0x{got[TEST_BYTE]:02x})")
    finally:
        radio.write_block(TEST_ADDRESS, original)
        back = radio.read_block(TEST_ADDRESS)
        restored = back == original
        say(f"  restored the original block: {'OK' if restored else 'NOT RESTORED (set Talk around again in the radio menu)'}")
    if all(sets):
        return True, "PASS: a 0 -> 1 write above 0x3000 took effect over this connection."
    return False, "FAIL: a 0 -> 1 write above 0x3000 did not take effect: the stock serial behaviour (no sector erase)."


RANGE = range(0x3000, 0x3120, BLOCK)


def range_test(radio, say=print):
    """CHIRP's pattern over 0x3000-0x311F. Returns (ok, message)."""
    original = {a: radio.read_block(a) for a in RANGE}
    first = original[TEST_ADDRESS]
    toggled = bytearray(first)
    toggled[TEST_BYTE] ^= TEST_MASK
    want = dict(original)
    want[TEST_ADDRESS] = bytes(toggled)
    say(f"read {len(original)} blocks; Talk around bit {first[TEST_BYTE] & TEST_MASK} -> {toggled[TEST_BYTE] & TEST_MASK} in the first")
    times, problems = [], []
    try:
        for a in RANGE:
            times.append(radio.write_block(a, want[a]))
        for a in RANGE:
            got = radio.read_block(a)
            if got != want[a]:
                problems.append(a)
                say(f"  0x{a:04x}: read back differs")
    finally:
        restored = []
        for a in RANGE:                                  # put everything back, whatever happened
            try:
                radio.write_block(a, original[a])
                restored.append(radio.read_block(a) == original[a])
            except Exception as error:                   # keep restoring the other blocks
                say(f"  0x{a:04x}: restore failed ({error})")
                restored.append(False)
        say(f"  restored all {len(restored)} blocks: {'OK' if all(restored) else 'NOT ALL RESTORED, check the radio (a Bluetooth write can fix it)'}")
    if times:
        say(f"  ACK times: first {times[0] * 1000:.0f} ms, slowest {max(times) * 1000:.0f} ms, mean {sum(times) / len(times) * 1000:.0f} ms, total {sum(times):.2f} s for {len(times)} blocks")
    if problems:
        return False, f"FAIL: {len(problems)} of {len(RANGE)} blocks did not read back as written (the stock serial behaviour for a 0 -> 1 change)."
    return True, "PASS: all blocks written back to back over this connection read back as written."


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--read-only", action="store_true", help="only read 0x3000-0x311F")
    parser.add_argument("--range", action="store_true", help="the whole-range back-to-back write test (CHIRP's pattern)")
    parser.add_argument("--yes", action="store_true", help="do not ask before writing")
    args = parser.parse_args(argv)
    try:
        import serial
    except ImportError:
        print("pip install pyserial", file=sys.stderr)
        return 2

    class Port:
        def __init__(self):
            self.s = serial.Serial(args.port, args.baud, timeout=0.05, write_timeout=2)
        def write(self, b):
            self.s.write(b); self.s.flush()
        def read(self, n, timeout):
            self.s.timeout = timeout
            return self.s.read(n)

    port = Port()
    radio = Radio(port)
    try:
        print("Radio:", radio.enter())
        if args.read_only:
            for address in range(0x3000, 0x3120, BLOCK):
                print(f"{address:04x}: {radio.read_block(address).hex(' ')}")
            return 0
        if not args.yes and input("This changes and restores the Talk around setting. Continue? [y/N] ").lower() != "y":
            return 1
        ok, message = range_test(radio) if args.range else check(radio)
        print(message)
        return 0 if ok else 1
    finally:
        radio.exit()


if __name__ == "__main__":
    raise SystemExit(main())
