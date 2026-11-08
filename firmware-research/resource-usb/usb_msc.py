"""USB Mass Storage Bulk-Only Transport for the BR23 USB Update device.

macOS gives no SCSI pass-through to a mass-storage disk, so this talks to the
device directly through libusb (pyusb). It exposes the same
``execute(cdb, data_out, data_in) -> residue`` call as the vendored Windows
and Linux ``scsiio`` ports, so ``jltech.uboot`` works unchanged on top of it.

On macOS the kernel mass-storage driver claims the device first; libusb can
detach it only when the process runs as root (``sudo``). The device is
reattached when the transport closes. Only the device named by an explicit
VID:PID is ever detached, so other disks are never touched.
"""

from __future__ import annotations

import errno
import struct
from dataclasses import dataclass
from pathlib import Path

CBW_SIGNATURE = b"USBC"
CSW_SIGNATURE = b"USBS"
MSC_CLASS = 0x08
BULK_ONLY = 0x50
TIMEOUT_MS = 5000
LIBUSB_PATHS = ("/opt/homebrew/lib/libusb-1.0.dylib", "/usr/local/lib/libusb-1.0.dylib")


class TransportError(RuntimeError):
    """A USB or bulk-only protocol failure."""


class CommandFailed(TransportError):
    """The device completed the command with a failed CSW status."""


@dataclass(frozen=True)
class Candidate:
    vid: int
    pid: int
    bus: int
    address: int
    manufacturer: str
    product: str

    def __str__(self) -> str:
        return (f"{self.vid:04X}:{self.pid:04X} bus {self.bus} address {self.address} "
                f"{self.manufacturer!r} {self.product!r}")


def backend():
    import usb.backend.libusb1 as libusb1

    def find_library(name):
        for path in LIBUSB_PATHS:
            if Path(path).exists():
                return path
        return None

    found = libusb1.get_backend(find_library=find_library) or libusb1.get_backend()
    if found is None:
        raise TransportError("libusb not found; install it with 'brew install libusb'")
    return found


def _msc_interface(device):
    for config in device:
        for interface in config:
            if (interface.bInterfaceClass == MSC_CLASS and
                    interface.bInterfaceProtocol == BULK_ONLY):
                return config, interface
    return None, None


def _string(device, index) -> str:
    import usb.util
    if not index:
        return ""
    try:
        return usb.util.get_string(device, index) or ""
    except Exception:  # noqa: BLE001 - descriptors can be unreadable without a claim
        return "?"


def list_candidates() -> list[Candidate]:
    """USB mass-storage devices, read from descriptors only (nothing is claimed)."""
    import usb.core
    candidates = []
    for device in usb.core.find(find_all=True, backend=backend()):
        _, interface = _msc_interface(device)
        if interface is not None:
            candidates.append(Candidate(device.idVendor, device.idProduct, device.bus,
                                        device.address, _string(device, device.iManufacturer),
                                        _string(device, device.iProduct)))
    return candidates


def parse_device(text: str) -> tuple[int, int]:
    try:
        vid, pid = (int(part, 16) for part in text.split(":"))
    except ValueError as error:
        raise ValueError("Device must be VID:PID in hex, for example 4C4A:4155") from error
    if not (0 <= vid <= 0xFFFF and 0 <= pid <= 0xFFFF):
        raise ValueError("VID and PID must be 16-bit")
    return vid, pid


def build_cbw(tag: int, length: int, direction_in: bool, cdb: bytes, lun: int = 0) -> bytes:
    if not 1 <= len(cdb) <= 16:
        raise ValueError("CDB must be 1-16 bytes")
    return struct.pack("<4sIIBBB16s", CBW_SIGNATURE, tag, length,
                       0x80 if direction_in else 0x00, lun, len(cdb), cdb.ljust(16, b"\0"))


def parse_csw(data: bytes, tag: int) -> tuple[int, int]:
    if len(data) != 13:
        raise TransportError(f"CSW is {len(data)} bytes, expected 13")
    signature, got_tag, residue, status = struct.unpack("<4sIIB", data)
    if signature != CSW_SIGNATURE or got_tag != tag:
        raise TransportError(f"Invalid CSW {data.hex()} for tag {tag}")
    return residue, status


class BulkOnlyDevice:
    """SCSI commands over USB bulk-only transport to one VID:PID."""

    def __init__(self, vid: int, pid: int, usb_device=None):
        self.vid, self.pid = vid, pid
        self.device = usb_device
        self.interface = None
        self.detached = False
        self.tag = 0
        self.csw_notes = []
        self.is_open = False
        self.open()

    def open(self) -> None:
        import usb.core
        import usb.util
        if self.device is None:
            matches = list(usb.core.find(find_all=True, idVendor=self.vid,
                                         idProduct=self.pid, backend=backend()))
            if len(matches) != 1:
                raise TransportError(f"Expected one {self.vid:04X}:{self.pid:04X} device, found {len(matches)}")
            self.device = matches[0]
        config, interface = _msc_interface(self.device)
        if interface is None:
            raise TransportError("Device has no bulk-only mass-storage interface")
        number = interface.bInterfaceNumber
        try:
            if self.device.is_kernel_driver_active(number):
                self.device.detach_kernel_driver(number)
                self.detached = True
        except usb.core.USBError as error:
            raise TransportError(
                "Could not detach the macOS mass-storage driver; run this tool with sudo") from error
        except NotImplementedError:
            pass
        try:
            self.device.get_active_configuration()
        except usb.core.USBError:
            self.device.set_configuration(config.bConfigurationValue)
        usb.util.claim_interface(self.device, number)
        self.interface = interface
        endpoints = {usb.util.endpoint_direction(ep.bEndpointAddress): ep for ep in interface
                     if usb.util.endpoint_type(ep.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK}
        try:
            self.ep_in = endpoints[usb.util.ENDPOINT_IN]
            self.ep_out = endpoints[usb.util.ENDPOINT_OUT]
        except KeyError as error:
            raise TransportError("Mass-storage interface lacks bulk IN/OUT endpoints") from error
        self.is_open = True

    def describe(self) -> dict:
        """USB facts worth recording from a hardware run."""
        return {
            "csw_residue_differences": self.csw_notes[:20],
            "vid_pid": f"{self.vid:04X}:{self.pid:04X}",
            "manufacturer": _string(self.device, self.device.iManufacturer),
            "product": _string(self.device, self.device.iProduct),
            "usb_version": f"{self.device.bcdUSB >> 8:x}.{self.device.bcdUSB & 0xFF:02x}",
            "interface": self.interface.bInterfaceNumber,
            "subclass": self.interface.bInterfaceSubClass,
            "endpoints": {"in": hex(self.ep_in.bEndpointAddress), "out": hex(self.ep_out.bEndpointAddress),
                          "max_packet": self.ep_in.wMaxPacketSize},
            "macos_driver_detached": self.detached,
        }

    def close(self) -> None:
        if not self.is_open:
            return
        import usb.util
        number = self.interface.bInterfaceNumber
        try:
            usb.util.release_interface(self.device, number)
        finally:
            if self.detached:
                try:
                    self.device.attach_kernel_driver(number)
                except Exception:  # noqa: BLE001 - reattach is best effort
                    pass
            usb.util.dispose_resources(self.device)
            self.is_open = False

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def _read_csw(self, tag: int) -> tuple[int, int]:
        import usb.core
        try:
            data = bytes(self.ep_in.read(13, timeout=TIMEOUT_MS))
        except usb.core.USBError as error:
            if error.errno != errno.EPIPE:
                raise
            self.device.clear_halt(self.ep_in)
            data = bytes(self.ep_in.read(13, timeout=TIMEOUT_MS))
        return parse_csw(data, tag)

    def execute(self, cdb: bytes, data_out, data_in, max_sense_len=32, return_sense_buffer=False):
        import usb.core
        if data_out and data_in:
            raise TransportError("Bidirectional transfers are not supported")
        self.tag = (self.tag + 1) & 0xFFFFFFFF
        length = len(data_out) if data_out else len(data_in) if data_in else 0
        self.ep_out.write(build_cbw(self.tag, length, data_in is not None and length > 0, bytes(cdb)),
                          timeout=TIMEOUT_MS)
        transferred = 0
        try:
            if data_out:
                transferred = self.ep_out.write(bytes(data_out), timeout=TIMEOUT_MS)
            elif data_in:
                chunk = bytes(self.ep_in.read(len(data_in), timeout=TIMEOUT_MS))
                data_in[:len(chunk)] = chunk
                transferred = len(chunk)
        except usb.core.USBError as error:
            if error.errno != errno.EPIPE:
                raise
            self.device.clear_halt(self.ep_in if data_in else self.ep_out)
        csw_residue, status = self._read_csw(self.tag)
        if status != 0:
            raise CommandFailed(f"Command {bytes(cdb[:2]).hex()} failed with CSW status {status}")
        # Residue is what the bus did not carry, as the Windows SCSI port reports
        # it. The BR23 ROM's CSW residue field is recorded but not trusted.
        residue = length - transferred
        if csw_residue != residue:
            self.csw_notes.append({"command": bytes(cdb[:2]).hex(), "length": length,
                                   "transferred": transferred, "csw_residue": csw_residue})
        return (residue, b"") if return_sense_buffer else residue
