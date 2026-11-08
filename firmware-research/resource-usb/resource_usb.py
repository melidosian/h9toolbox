#!/usr/bin/env python3
"""USB route to the TD-H9 resource flash through the BR23 mask ROM.

Commands:
  list   USB mass-storage devices (descriptors only; nothing is claimed)
  plan   assemble the RAM programs and show what U1 would send (no device)
  u2     U2: the resource flash's JEDEC ID through the SPI2 RAM program
           --op jedec    SPI2 as stock sets it up (first hardware run: FFFFFF)
           --op regs     read-only snapshot of GPIO banks 0-3, 0x1E5100-1C, SPI0/1/2
           --op bitbang  9F bit-banged on CS 23 / CLK 24 / MOSI 26 / MISO 25
           --op bitbang-swap  the same with MOSI 25 / MISO 26
           --op led      blink GPIO 9 (top LED) twice: do outputs drive pads here?
           --op sample / sample-swap  bit-bang that also records every GPIO 16-31 input per clock
           --op pins     SPI2 after explicit SDK-style pin setup
  u1     U1 in three stages, each run only after the previous one passed:
           --stage probe  SCSI INQUIRY only (checks the USB transport)
           --stage read   INQUIRY and RAM reads (checks the ROM's READ_MEMORY)
           --stage full   RAM write, upload, and two jumps (does the jump return?)

Every command stays in RAM: nothing reads, erases, or writes the radio's
internal flash or the resource flash. Power-cycling the radio clears it all.
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import rom

MARKER = 0x13000
SCRATCH = 0x13100
PARAMS = 0x13040
BUFFER = 0x13400
U1_MAGIC = b"H9U1"
U1_ARGUMENT = 0x4839
# Pinned assembled bytes of src/u1_marker.s and src/spi2_rom.s.
PROGRAMS = {
    "u1_marker": "9623edceee28f7f4",
    "spi2_rom": "da0d4f16172d9e9c",
}


def program(name: str) -> bytes:
    code = rom.assemble(name)
    if not rom.sha256(code).startswith(PROGRAMS[name]):
        raise RuntimeError(f"{name} assembled to unexpected bytes {rom.sha256(code)}")
    return code


def open_transport(args):
    if args.scsi:
        rom.vendor()
        from scsiio import SCSIDev
        return SCSIDev(args.scsi)
    import usb_msc
    vid, pid = usb_msc.parse_device(args.device)
    return usb_msc.BulkOnlyDevice(vid, pid)


def command_list(args) -> int:
    import usb_msc
    candidates = usb_msc.list_candidates()
    if not candidates:
        print("No USB mass-storage devices found. Power on the radio holding 8 with the data cable connected.")
    for candidate in candidates:
        print(candidate)
    print("Pass the radio's VID:PID to --device. Other disks are never claimed.")
    return 0


def command_plan(args) -> int:
    for name in PROGRAMS:
        code = program(name)
        print(f"{name}: {len(code)} bytes at 0x{rom.PROGRAM_ADDRESS:X}, SHA-256 {rom.sha256(code)}")
        print(f"  plain     {code[:24].hex()}")
        print(f"  encrypted {rom.encrypt(code)[:24].hex()}")
    print(f"U1 marker RAM 0x{MARKER:X}, cipher check RAM 0x{SCRATCH:X}; no device opened")
    return 0


def run_session(args, test: str, code: bytes, body) -> int:
    """Open the ROM, run body(session, step), and always save the report."""
    report = {"test": test, "started": datetime.now(timezone.utc).isoformat(),
              "platform": sys.platform, "transport": f"scsi {args.scsi}" if args.scsi else f"libusb {args.device}",
              "program_sha256": rom.sha256(code), "steps": []}

    def step(name, **values):
        report["steps"].append({"step": name, **values})
        print(f"{name}: " + ", ".join(f"{k}={v}" for k, v in values.items()))

    transport = None
    try:
        transport = open_transport(args)
        session = rom.RomSession(transport)
        if hasattr(transport, "describe"):
            report["usb"] = transport.describe()
            step("usb", **report["usb"])
        step("inquiry", device=session.require_rom())
        report["result"] = "pass: " + body(session, step)
        print(report["result"])
        return 0
    except Exception as error:
        report["result"] = f"stopped: {error}"
        print(report["result"], file=sys.stderr)
        return 1
    finally:
        if transport is not None:
            if hasattr(transport, "csw_notes") and transport.csw_notes:
                report["csw_residue_differences"] = transport.csw_notes[:20]
                print(f"note: the device's CSW residue differed from the bytes carried on "
                      f"{len(transport.csw_notes)} command(s); first {transport.csw_notes[0]}")
            transport.close()
        if args.report:
            Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print(f"Report saved: {args.report}")


def upload(session, step, code: bytes) -> None:
    """Learn the read cipher, upload a program to 0x12000, and verify it in RAM."""
    step("cipher", mode=session.check_read_cipher(SCRATCH, b"H9 U1 RAM check!"))
    session.write(rom.PROGRAM_ADDRESS, code)
    in_ram = session.read(rom.PROGRAM_ADDRESS, len(code))
    step("upload", bytes=len(code), program=code.hex(), ram=in_ram.hex())
    if in_ram != code:
        raise RuntimeError("Uploaded program read-back differs")


def command_u1(args) -> int:
    code = program("u1_marker")

    def body(session, step):
        if args.stage == "probe":
            return "the transport reaches the BR23 UBOOT1.00 mask ROM"
        for address in (rom.PROGRAM_ADDRESS, MARKER):
            first, second = session.read_raw(address, 16), session.read_raw(address, 16)
            step("read RAM", address=hex(address), returned=first.hex(), repeat_matches=first == second)
            if first != second:
                raise RuntimeError(f"Two reads of 0x{address:X} differ")
        if args.stage == "read":
            return "the ROM answers READ_MEMORY"
        upload(session, step, code)
        session.write(MARKER, bytes(16))
        if session.read(MARKER, 16) != bytes(16):
            raise RuntimeError("Marker area did not clear")
        for run in (1, 2):
            started = time.monotonic()
            try:
                session.jump(rom.PROGRAM_ADDRESS, U1_ARGUMENT)
            except Exception as error:  # noqa: BLE001 - report exactly what the ROM did
                step(f"jump {run}", result="no reply", error=str(error))
                raise RuntimeError("The ROM did not answer after the jump. Power-cycle the radio; "
                                   "this means the jump does not return to the ROM.") from error
            marker = session.read(MARKER, 16)
            counter = int.from_bytes(marker[12:16], "little")
            step(f"jump {run}", result="returned", seconds=round(time.monotonic() - started, 3),
                 marker=marker[:4].decode("ascii", "replace"), r0=hex(int.from_bytes(marker[4:8], "little")),
                 r1=hex(int.from_bytes(marker[8:12], "little")), counter=counter)
            if marker[:4] != U1_MAGIC or counter != run:
                raise RuntimeError(f"Marker after jump {run} is {marker.hex()}")
        step("inquiry after", device=session.require_rom())
        return "the ROM runs RAM programs and returns to its command loop"

    return run_session(args, f"U1 {args.stage}", code, body)


SPI2_STATUS = {0: "ok", 1: "bad magic", 2: "SPI2 timeout", 3: "bad op or length"}


GPIO25, GPIO26 = 0x200, 0x400  # bank 1 bits 9 and 10


def spi2_parameters(op: int, address: int = 0, length: int = 0, divider: int = 31,
                    mosi: int = GPIO26, miso: int = GPIO25) -> bytes:
    """Parameter block at PARAMS for src/spi2_rom.s; outputs start cleared."""
    return struct.pack("<4sIIII12xII", b"H9U2", op, address, length, divider, mosi, miso)


def run_spi2(session, step, label: str, op: int, divider: int, address: int = 0, length: int = 0,
             mosi: int = GPIO26, miso: int = GPIO25) -> dict:
    session.write(PARAMS, spi2_parameters(op, address, length, divider, mosi, miso))
    started = time.monotonic()
    try:
        session.jump(rom.PROGRAM_ADDRESS, 0)
    except Exception as error:  # noqa: BLE001 - report exactly what the ROM did
        step(label, result="no reply", error=str(error))
        raise RuntimeError("The ROM did not answer after the SPI2 program; power-cycle the radio") from error
    block = session.read(PARAMS, 32)
    magic, _, _, _, _, status, jedec, count = struct.unpack("<4sIIIIIII", block)
    result = {"status": SPI2_STATUS.get(status, f"unknown {status}"), "jedec": f"{jedec:06X}",
              "runs": count, "seconds": round(time.monotonic() - started, 3)}
    step(label, **result)
    if magic != b"H9U2" or count != 1:
        raise RuntimeError(f"Parameter block after the jump is {block.hex()}")
    return {"status": status, "jedec": jedec}


SPI2_OPS = {"jedec": 1, "pins": 4, "bitbang": 5, "bitbang-swap": 5, "sample": 6, "sample-swap": 6}
DATA_PINS = {"bitbang-swap": (GPIO25, GPIO26), "sample-swap": (GPIO25, GPIO26)}
SAMPLES = 0x13600


def describe_samples(words: list[int]) -> dict:
    """Op-6 bank-1 input words: baseline, 8 command clocks, 24 response clocks."""
    baseline, command, response = words[0], words[1:9], words[9:33]
    changing = 0
    for word in command + response:
        changing |= word ^ baseline
    pins_changing = [16 + bit for bit in range(16) if changing >> bit & 1]
    return {
        "baseline": f"{baseline:04X}",
        "command_clocks": [f"{w:04X}" for w in command],
        "response_clocks": [f"{w:04X}" for w in response],
        "gpio_changing": pins_changing,
        "per_pin_response_bits": {str(pin): "".join(str(w >> (pin - 16) & 1) for w in response)
                                  for pin in pins_changing},
    }
PIN_REGISTERS = ("out", "in", "dir", "ie", "pu", "pd")


SNAPSHOT_WORDS = 64 + 8 + 6


def decode_registers(words: list[int]) -> dict:
    """Op-3 snapshot: GPIO banks 0-3, 8 words at 0x1E5100, SPI0/1/2 control and baud."""
    banks = [words[16 * n:16 * n + 16] for n in range(4)]
    pins = {}
    for pin in (23, 24, 25, 26, 53):
        bank, bit = banks[pin >> 4], 1 << (pin & 15)
        pins[str(pin)] = {name: int(bool(bank[i] & bit)) for i, name in enumerate(PIN_REGISTERS)}
    spi = {f"spi{n}": {"control": f"{words[72 + 2 * n]:08X}", "baud": f"{words[73 + 2 * n]:08X}",
                       "width_bits_10_11": (words[72 + 2 * n] >> 10) & 3}
           for n in range(3)}
    return {
        "pins": pins,
        **{f"bank{n}": [f"{w:08X}" for w in banks[n]] for n in range(4)},
        "0x1E5100": [f"{w:08X}" for w in words[64:72]],
        "iomap_bit16_port_b": int(bool(words[71] & 0x10000)),
        **spi,
    }


def command_u2(args) -> int:
    code = program("spi2_rom")

    def body(session, step):
        upload(session, step, code)
        if args.op == "led":
            outcome = run_spi2(session, step, "led", 7, args.divider)
            if outcome["status"] != 0:
                raise RuntimeError(f"LED op reported {SPI2_STATUS.get(outcome['status'])}")
            return ("LED program ran (two on/off pulses of 4,000,000 loop iterations each); "
                    "record whether the top LED blinked twice")
        if args.op == "regs":
            outcome = run_spi2(session, step, "snapshot", 3, args.divider)
            if outcome["status"] != 0:
                raise RuntimeError(f"Snapshot reported {SPI2_STATUS.get(outcome['status'])}")
            raw = session.read(BUFFER, SNAPSHOT_WORDS * 4)
            registers = decode_registers(list(struct.unpack(f"<{SNAPSHOT_WORDS}I", raw)))
            for pin, bits in registers["pins"].items():
                step(f"gpio {pin}", **bits)
            for n in range(3):
                step(f"spi{n}", **registers[f"spi{n}"])
            step("registers", **{k: v for k, v in registers.items()
                                 if k != "pins" and not k.startswith("spi")})
            return "register snapshot saved (read-only)"
        ids = []
        for run in (1, 2):
            mosi, miso = DATA_PINS.get(args.op, (GPIO26, GPIO25))
            outcome = run_spi2(session, step, f"{args.op} {run}", SPI2_OPS[args.op], args.divider,
                               mosi=mosi, miso=miso)
            if outcome["status"] == 2:
                raise RuntimeError("SPI2 never completed a byte: its clock or port mapping is "
                                   "probably not active in USB Update mode")
            if outcome["status"] != 0:
                raise RuntimeError(f"SPI2 program reported {SPI2_STATUS.get(outcome['status'])}")
            ids.append(outcome["jedec"])
            if SPI2_OPS[args.op] == 6:
                words = list(struct.unpack("<33I", session.read(SAMPLES, 33 * 4)))
                step(f"{args.op} {run} samples", **describe_samples(words))
        if SPI2_OPS[args.op] == 6:
            return f"samples recorded (diagnostic); JEDEC reads {ids[0]:06X}, {ids[1]:06X}"
        if ids[0] != ids[1]:
            raise RuntimeError(f"JEDEC ID changed between reads: {ids[0]:06X}, {ids[1]:06X}")
        jedec = ids[0]
        if jedec in (0x000000, 0xFFFFFF):
            raise RuntimeError(f"JEDEC ID {jedec:06X}: nothing answered on the bus "
                               "(chip select, pin mapping, or MISO direction)")
        capacity = jedec & 0xFF
        step("chip", manufacturer=f"{jedec >> 16:02X}", type=f"{(jedec >> 8) & 0xFF:02X}",
             capacity=f"{capacity:02X}", bytes=1 << capacity if 0x10 <= capacity <= 0x20 else None)
        step("inquiry after", device=session.require_rom())
        if capacity != 0x18:
            raise RuntimeError(f"Chip answered {jedec:06X}, but capacity byte {capacity:02X} is not 16 MiB")
        return f"{args.op}: reaches a 16 MiB flash, JEDEC ID {jedec:06X}"

    return run_session(args, f"U2 {args.op}", code, body)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("list")
    sub.add_parser("plan")
    u1 = sub.add_parser("u1")
    u1.add_argument("--stage", choices=("probe", "read", "full"), required=True,
                    help="probe, then read, then full; each only after the previous passed")
    u2 = sub.add_parser("u2")
    u2.add_argument("--op", choices=("jedec", "regs", "bitbang", "bitbang-swap", "sample", "sample-swap", "pins", "led"), default="jedec",
                    help="jedec (stock setup), regs (read-only snapshot), bitbang, pins")
    u2.add_argument("--divider", type=int, default=31, choices=range(0, 256), metavar="0-255",
                    help="SPI2 baud divider (default 31, deliberately slow)")
    for command in (u1, u2):
        target = command.add_mutually_exclusive_group(required=True)
        target.add_argument("--device", help="USB VID:PID of the BR23 update device (macOS/Linux via libusb)")
        target.add_argument("--scsi", help="SCSI path (Windows \\\\.\\PHYSICALDRIVEn or Linux /dev/sgN)")
        command.add_argument("--report", help="save a JSON report of every step")
    args = parser.parse_args()
    return {"list": command_list, "plan": command_plan, "u1": command_u1, "u2": command_u2}[args.command](args)


if __name__ == "__main__":
    raise SystemExit(main())
