"""BR23 mask-ROM (UBOOT1.00) session for running small RAM programs.

In USB Update mode (power on holding 8) the BR23 mask ROM answers vendor
SCSI commands: WRITE_MEMORY (0xFB06), READ_MEMORY (0xFD07), and
JUMP_TO_MEMORY (0xFB08). The USB flasher already uses the first and last to
start its RAM helper at 0x12000. Nothing here reads, erases, or writes the
radio's flash; programs and data stay in RAM and vanish on power-off.
"""

from __future__ import annotations

import hashlib
import sys
import types
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TOOLS = ROOT.parent / "brick-breaker" / "tools"
VENDOR_DIR = TOOLS / "h9-flasher" / "vendor"
ISA = TOOLS / "pi32v2-isa"

VENDOR, PRODUCT = "BR23", "UBOOT1.00"
# The flasher uploads its 11,264-byte RAM helper at 0x12000, so the ROM
# leaves this window free. Everything here stays inside its first 0x2C00.
RAM_WINDOW = range(0x12000, 0x14C00)
PROGRAM_ADDRESS = 0x12000
CHUNK = 512

_vendor = None


def vendor():
    """Import jltech.uboot and its cipher on any platform.

    The vendored scsiio package only supports Windows and Linux and raises on
    import elsewhere. On other platforms a stand-in module provides the
    SCSIException name jltech.uboot imports; transport comes from usb_msc.
    """
    global _vendor
    if _vendor is None:
        sys.path.insert(0, str(VENDOR_DIR))
        if sys.platform not in ("win32", "linux") and "scsiio" not in sys.modules:
            package = types.ModuleType("scsiio")
            common = types.ModuleType("scsiio.common")

            class SCSIException(RuntimeError):
                pass

            common.SCSIException = SCSIException
            package.SCSIDev = None
            package.common = common
            sys.modules["scsiio"] = package
            sys.modules["scsiio.common"] = common
        from jltech import uboot, cipher
        _vendor = types.SimpleNamespace(JL_UBOOT=uboot.JL_UBOOT, SCSIException=uboot.SCSIException,
                                        jl_crc_cipher=cipher.jl_crc_cipher,
                                        cipher_bytes=cipher.cipher_bytes)
    return _vendor


def encrypt(data: bytes) -> bytes:
    """JieLi CrcDecode keystream for one WRITE_MEMORY transfer (it is its own inverse)."""
    v = vendor()
    return v.cipher_bytes(v.jl_crc_cipher, data)


def assemble(name: str) -> bytes:
    """Assemble src/<name>.s and link it at PROGRAM_ADDRESS with the pinned toolchain."""
    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm, elf, ld
    import tempfile
    source = ROOT / "src" / f"{name}.s"
    with tempfile.TemporaryDirectory() as temp:
        obj = Path(temp) / f"{name}.o"
        obj.write_bytes(asm.assemble_source(source.read_text(encoding="utf-8"), filename=str(source)))
        state, elf_bytes = ld.link(ld.parse_args(["-T", str(ROOT / "src" / "ram.ld"),
                                                  "-o", str(Path(temp) / f"{name}.elf"), str(obj)]))
    if elf_bytes is None or state.errors:
        raise RuntimeError(f"Pi32 linker failed: {state.errors}")
    program = elf.read(elf_bytes)
    text = next(s for s in program.sections if s.name == ".text")
    if text.addr != PROGRAM_ADDRESS or program.entry != PROGRAM_ADDRESS:
        raise RuntimeError(f"{name} is not linked at 0x{PROGRAM_ADDRESS:X}")
    return bytes(text.data)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def check_window(address: int, length: int) -> None:
    if length <= 0 or address not in RAM_WINDOW or address + length - 1 not in RAM_WINDOW:
        raise ValueError(f"0x{address:X}+{length} is outside RAM 0x{RAM_WINDOW.start:X}-0x{RAM_WINDOW.stop - 1:X}")


class RomSession:
    """Vendor commands to the mask ROM through any execute()-style transport."""

    def __init__(self, device):
        self.device = device
        self.boot = vendor().JL_UBOOT(device)
        # WRITE_MEMORY payloads are always sent encrypted: the flasher's RAM
        # helper is uploaded encrypted and runs, so the ROM decrypts writes.
        # Whether READ_MEMORY returns RAM as-is or encrypted is learned by
        # check_read_cipher(); on this radio it encrypts (2026-11-08).
        self.reads_encrypted = None

    def inquiry(self) -> tuple[str, str, str]:
        reply = bytearray(36)
        residue = self.device.execute(bytes((0x12, 0, 0, 0, 36, 0)), None, reply)
        if residue:
            raise RuntimeError("Short SCSI INQUIRY response")
        return tuple(reply[start:end].decode("ascii", errors="replace").strip()
                     for start, end in ((8, 16), (16, 32), (32, 36)))

    def require_rom(self) -> str:
        found = self.inquiry()
        if found[:2] != (VENDOR, PRODUCT):
            raise RuntimeError(f"Refusing device {found!r}; expected {VENDOR} {PRODUCT}")
        return " ".join(found)

    def read_raw(self, address: int, length: int) -> bytes:
        """READ_MEMORY bytes exactly as the ROM returns them."""
        check_window(address, length)
        data = bytearray()
        for offset in range(0, length, CHUNK):
            data += self.boot.mem_read(address + offset, min(CHUNK, length - offset))
        return bytes(data)

    def read(self, address: int, length: int) -> bytes:
        """RAM contents; requires check_read_cipher() first."""
        if self.reads_encrypted is None:
            raise RuntimeError("Call check_read_cipher() before decoded reads")
        if not self.reads_encrypted:
            return self.read_raw(address, length)
        data = bytearray()
        for offset in range(0, length, CHUNK):
            data += encrypt(self.read_raw(address + offset, min(CHUNK, length - offset)))
        return bytes(data)

    def write(self, address: int, data: bytes) -> None:
        check_window(address, len(data))
        for offset in range(0, len(data), CHUNK):
            self.boot.mem_write(address + offset, encrypt(data[offset:offset + CHUNK]))

    def jump(self, address: int, argument: int) -> None:
        check_window(address, 2)
        self.boot.mem_jump(address, argument & 0xFFFF)

    def check_read_cipher(self, address: int, pattern: bytes) -> str:
        """Learn how READ_MEMORY returns RAM, using one encrypted write.

        The write is decrypted by the ROM, so RAM then holds the pattern. A
        raw read equal to the encrypted pattern means reads are encrypted; equal
        to the pattern means reads are raw. Anything else stops the run.
        """
        if len(pattern) > CHUNK:
            raise ValueError("Pattern must fit one transfer")
        self.write(address, pattern)
        back = self.read_raw(address, len(pattern))
        if back == encrypt(pattern):
            self.reads_encrypted = True
            return "writes decrypted, reads encrypted"
        if back == pattern:
            self.reads_encrypted = False
            return "writes decrypted, reads raw"
        raise RuntimeError(f"RAM write/read-back mismatch at 0x{address:X}: {back.hex()}")
