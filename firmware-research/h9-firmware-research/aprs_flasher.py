#!/usr/bin/env python3
"""TD-H9 APRS controller serial updater (vendor-protocol reconstruction).

The offline commands need only Python's standard library. ``flash`` needs
pyserial and an APRS controller already placed in its separate update mode.
No hardware write has been validated with this implementation yet.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
from hashlib import sha256
import json
import os
from pathlib import Path
import struct
import sys
from typing import Callable, TextIO


STOCK_SHA256 = "1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33"
IMAGE_SIZE = 17_304
IMAGE_BASE = 0x08002000
FRAME_PREFIX = b"TID\x00"
BLOCK_SIZE = 128
EXIT_FRAME = b"TID\x00\x01\xff\x00"


class AprsFlashError(Exception):
    """Rejected image, protocol reply, or transfer."""


@dataclass(frozen=True)
class Image:
    data: bytes
    digest: str
    stack_pointer: int
    reset_vector: int


def default_journal_path(image_path: Path) -> Path:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    return image_path.with_name(
        f"{image_path.stem}.aprs-flash-{stamp}-{os.getpid()}.jsonl"
    )


class FlashJournal:
    """Append-only record of acknowledged blocks; no firmware bytes are saved."""

    def __init__(self, path: Path, image: Image, port: str, baud: int):
        self.path = path
        self.image = image
        self.port = port
        self.baud = baud
        self.output: TextIO | None = None
        self.acknowledged_end = 0
        self.exit_acked = False

    def _record(self, event: str, **fields: object) -> None:
        if self.output is None:
            raise RuntimeError("Flash journal is not open")
        row = {
            "event": event,
            "utc": datetime.now(timezone.utc).isoformat(),
            **fields,
        }
        self.output.write(json.dumps(row, sort_keys=True) + "\n")
        self.output.flush()
        os.fsync(self.output.fileno())

    def __enter__(self) -> FlashJournal:
        self.output = self.path.open("x", encoding="utf-8")
        try:
            self._record("attempt_started", image_sha256=self.image.digest,
                         image_bytes=len(self.image.data), port=self.port,
                         baud=self.baud)
        except OSError:
            self.output.close()
            self.output = None
            raise
        return self

    def block_acknowledged(self, offset: int, length: int) -> None:
        acknowledged_end = offset + length
        self._record("block_acknowledged", offset=offset, length=length,
                     acknowledged_end=acknowledged_end)
        self.acknowledged_end = acknowledged_end

    def exit_acknowledged(self) -> None:
        self._record("exit_acknowledged",
                     acknowledged_end=self.acknowledged_end)
        self.exit_acked = True

    def __exit__(self, exception_type, exception, traceback) -> None:
        if self.output is None:
            return
        try:
            self._record("attempt_failed" if exception_type else
                         ("transfer_acknowledged" if self.exit_acked else
                          "attempt_incomplete"),
                         acknowledged_end=self.acknowledged_end,
                         error=str(exception) if exception_type else None)
        except OSError:
            if exception_type is None:
                raise
        finally:
            self.output.close()
            self.output = None


def inspect_image(path: Path) -> Image:
    data = path.read_bytes()
    if len(data) != IMAGE_SIZE:
        raise AprsFlashError(f"Expected {IMAGE_SIZE} APRS image bytes, got {len(data)}")
    stack, reset = struct.unpack_from("<II", data)
    if not 0x20000000 <= stack < 0x20010000:
        raise AprsFlashError(f"Implausible Cortex-M stack pointer 0x{stack:08X}")
    if not (reset & 1) or not IMAGE_BASE <= (reset & ~1) < IMAGE_BASE + len(data):
        raise AprsFlashError(f"Reset vector 0x{reset:08X} is outside the APRS image")
    return Image(data, sha256(data).hexdigest(), stack, reset)


def validate_release_manifest(path: Path, image: Image) -> None:
    """Require an explicit, matching release record for a non-stock image."""
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeError) as error:
        raise AprsFlashError(f"Invalid APRS release manifest: {error}") from error
    if not isinstance(manifest, dict):
        raise AprsFlashError("APRS release manifest must be a JSON object")
    required = {
        "kind": "aprs_release_image",
        "flashable": True,
        "image_sha256": image.digest,
        "image_bytes": len(image.data),
        "image_base": IMAGE_BASE,
    }
    for key, expected in required.items():
        value = manifest.get(key)
        if type(value) is not type(expected) or value != expected:
            raise AprsFlashError(
                f"APRS release manifest {key} must match {expected!r}; got {value!r}"
            )


def checksum(data: bytes) -> int:
    """Vendor Form4.CalculateChecksum: sum inclusive bytes, modulo 256."""
    return sum(data) & 0xFF


def write_frame(address: int, payload: bytes) -> bytes:
    if not 0 <= address <= 0xFFFFFFFF:
        raise AprsFlashError("Invalid image offset")
    if not 1 <= len(payload) <= BLOCK_SIZE:
        raise AprsFlashError(f"Payload must contain 1..{BLOCK_SIZE} bytes")
    header = FRAME_PREFIX + bytes((len(payload) + 5, 3)) + address.to_bytes(4, "big")
    body = header + payload
    return body + bytes((checksum(body[3:]),))


def frames(image: Image):
    for address in range(0, len(image.data), BLOCK_SIZE):
        yield address, write_frame(address, image.data[address:address + BLOCK_SIZE])


def check_reply(reply: bytes, expected_status: int) -> None:
    if len(reply) != 7:
        raise AprsFlashError(
            f"Expected 7-byte APRS acknowledgment, got {len(reply)} bytes: "
            f"{reply.hex(' ')}"
        )
    # Vendor Form4 checks the received length and status at byte 5. It does
    # not validate a TID prefix, length byte, or checksum on acknowledgments;
    # their actual device values have not been captured. Requiring those
    # fields here would reject a reply that the supplied updater accepts.
    if reply[5] != expected_status:
        raise AprsFlashError(
            f"APRS reply status 0x{reply[5]:02X}, expected "
            f"0x{expected_status:02X}: {reply.hex(' ')}"
        )


def read_exact(port, size: int) -> bytes:
    result = bytearray()
    while len(result) < size:
        chunk = port.read(size - len(result))
        if not chunk:
            break
        result.extend(chunk)
    return bytes(result)


def transfer(port, image: Image, progress=print,
             on_block_ack: Callable[[int, int], None] | None = None,
             on_exit_ack: Callable[[], None] | None = None) -> None:
    """Write every block once; stop on any unexpected reply and leave IAP active."""
    for address, packet in frames(image):
        if port.write(packet) != len(packet):
            raise AprsFlashError(f"Short serial write at image offset 0x{address:X}")
        reply = read_exact(port, 7)
        try:
            check_reply(reply, 0x13)
        except AprsFlashError as error:
            raise AprsFlashError(
                f"At image offset 0x{address:X}: {error}"
            ) from error
        if on_block_ack is not None:
            on_block_ack(address, min(BLOCK_SIZE, len(image.data) - address))
        progress(f"Written {min(address + BLOCK_SIZE, len(image.data)):,}/{len(image.data):,} bytes")
    if port.write(EXIT_FRAME) != len(EXIT_FRAME):
        raise AprsFlashError("Short serial write while exiting APRS update mode")
    check_reply(read_exact(port, 7), 0x10)
    if on_exit_ack is not None:
        on_exit_ack()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for command in ("inspect", "plan", "flash"):
        option = sub.add_parser(command)
        option.add_argument("image", type=Path)
        if command == "flash":
            option.add_argument("--port", required=True, help="APRS serial COM port or /dev/tty* path")
            option.add_argument("--baud", type=int, default=38400)
            option.add_argument("--allow-modified", action="store_true")
            option.add_argument("--release-manifest", type=Path,
                                help="matching release record required for modified images")
            option.add_argument("--yes", action="store_true", help="Authorize writing the APRS controller")
            option.add_argument("--journal", type=Path,
                                help="New JSONL transfer journal (default: beside image)")
    args = parser.parse_args(argv)
    try:
        image = inspect_image(args.image)
        print(f"APRS image: {len(image.data):,} bytes, SHA-256 {image.digest}")
        print(f"Stack 0x{image.stack_pointer:08X}, reset 0x{image.reset_vector:08X}")
        if args.command == "plan":
            print(f"Serial plan: {sum(1 for _ in frames(image))} blocks of up to {BLOCK_SIZE} bytes")
            print(f"First frame: {write_frame(0, image.data[:BLOCK_SIZE])[:10].hex(' ')} ...")
            print("No device was opened or written.")
        elif args.command == "flash":
            if image.digest != STOCK_SHA256 and not args.allow_modified:
                raise AprsFlashError("Modified image requires --allow-modified")
            if image.digest != STOCK_SHA256:
                if args.release_manifest is None:
                    raise AprsFlashError(
                        "Modified image requires --release-manifest; "
                        "diagnostic link-check exports are not flashable"
                    )
                validate_release_manifest(args.release_manifest, image)
            if not args.yes:
                raise AprsFlashError("Add --yes to write the APRS controller")
            try:
                import serial  # type: ignore[import-not-found]
            except ImportError as error:
                raise AprsFlashError("Install pyserial for the flash command") from error
            journal_path = args.journal or default_journal_path(args.image)
            print(f"Transfer journal: {journal_path}")
            with FlashJournal(journal_path, image, args.port, args.baud) as journal:
                with serial.Serial(
                    args.port, baudrate=args.baud, bytesize=8, parity="N", stopbits=1,
                    timeout=3, write_timeout=3, dsrdtr=False, rtscts=False,
                ) as port:
                    port.dtr = True
                    port.rts = True
                    port.reset_input_buffer()
                    transfer(port, image, on_block_ack=journal.block_acknowledged,
                             on_exit_ack=journal.exit_acknowledged)
            print("APRS transfer acknowledged. Power-cycle and verify module version/behavior.")
        return 0
    except (AprsFlashError, OSError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
