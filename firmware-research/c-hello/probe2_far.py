#!/usr/bin/env python3
"""Far blob for probe2.c: two constant-returning functions at flash 0xEA100
(stored plain, returns 0x11111111) and 0xEA200 (stored in the app-area
cipher, returns 0x22222222). Writes build/probe2_far.bin for write-far."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / "far-hello"))
sys.path.insert(0, str(ROOT.parent / "brick-breaker" / "tools" / "pi32v2-isa"))
from build import flash_form  # noqa: E402  (far-hello/build.py)
from pi32v2isa import asm, elf  # noqa: E402


def code(src: str) -> bytes:
    program = elf.read(asm.assemble_source('    .section .text,"ax",@progbits\n' + src))
    return bytes(next(s for s in program.sections if s.name == ".text").data)


plain = code("    r0 = 0x11111111\n    rts\n")
encrypted_source = code("    r0 = 0x22222222\n    rts\n")
blob = bytearray(b"\xff" * 0x240)
blob[0x100:0x100 + len(plain)] = plain
blob[0x200:0x220] = flash_form(encrypted_source, 0xEA200)
out = ROOT / "build" / "probe2_far.bin"
out.write_bytes(blob)
print(f"plain {plain.hex(' ')}; encrypted source {encrypted_source.hex(' ')}; wrote {out}")
