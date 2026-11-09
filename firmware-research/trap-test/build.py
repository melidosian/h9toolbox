#!/usr/bin/env python3
"""Build the trap test of the my_app-only block (the Doom firmware condition).

Two outputs, flashed in hold-8 mode:
  - h9_trap_monitor_0xEA000.bin (write-far): the monitor (monitor.c), started
    as the my_app task in place of stock my_app, with the full C-hello
    startup, a power-on recovery key (3 -> ROM USB loader) and the trap
    handler far_trap.
  - h9_trap_test_OFFLINE_EXPERIMENT.fw: stock v1.0.38 with
      * the my_app task entry (immediate at app+0x5FFD0, 0x01E88280)
        pointed at the monitor (0x01EE5000 header jump), and
      * the my_app-only block (app+0x79E32-0x7D64E and 0x7D69E-0x890E4,
        takeover/callgraph.py --needed) filled with traps: every 1 KiB a
        14-byte stub (r0 = its own address; r3 = far_trap; goto r3), every
        other halfword a 2-byte goto to the next stub.
far_trap stores "TRAP" and the stub address at RAM 0x1E4F0 and calls ROM
0x02000D68, so a trap puts the radio in the USB loader (4C4A:2342).

    export JIELI_TOOLCHAIN=~/path/to/jieli-linux-toolchains-20260730.1
    python3 -I build.py --stock-image <app.bin> --stock-package <TD-H9-V1.0.38.fw> [--mode M]

Modes (run 1, "redirect+traps", gave a black screen with no LED):
  redirect+traps  task entry redirected, block trapped (run 1)
  trampoline      no redirect: my_app's own entry (app+0x88180) gets
                  `r3 = 0x01EE5008; goto r3`, the rest of the block is
                  trapped (C-hello hooked the same entry and ran)
  redirect-only   task entry redirected, no traps
  trampoline-only the trampoline without traps: does the far monitor run
                  when started at boot (C-hello ran from the app cave)?
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
RESEARCH = ROOT.parent
TOOLS = RESEARCH / "brick-breaker" / "tools"
ISA = TOOLS / "pi32v2-isa"
H9CC = RESEARCH / "toolchain" / "h9cc"
STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
STOCK_PACKAGE_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
IMAGE_BASE = 0x01E00100
FAR_ADDRESS = 0x01EE5000
FAR_LIMIT = 0xFC000 - 0xEA000
MAGIC = 0x31583948
TASK_ENTRY = 0x5FFCE          # r0 = 0x01E88280 (my_app) before the task launch
TASK_ORIGINAL = bytes.fromhex("c0ff8082e801")
MY_APP = 0x88180
MODES = ("redirect+traps", "trampoline", "redirect-only", "trampoline-only")
BLOCK = ((0x79E32, 0x7D64E), (0x7D69E, 0x890E4))
STUB_EVERY = 1024
STUB_BYTES = 14
C_FLAGS = ["-target", "pi32v2", "-mcpu=r3", "-Oz", "-integrated-as",
           "-ffreestanding", "-fno-builtin", "-fno-common", "-Wall", "-Werror"]
HEADER = f"""    .section .far_header,"ax",@progbits
    .globl far_header
far_header:
    .long 0x{MAGIC:08x}
    .long 0
    r3 = hello_display_hook
    goto r3
"""
FAR_API = """
    .section .text,"ax",@progbits
/* void h9_enter_usb_loader(void): ROM 0x02000D68 (proven 2026-11-09). */
    .globl h9_enter_usb_loader
h9_enter_usb_loader:
    r3 = 0x02000d68
    call r3
h9_usb_loader_returned:
    goto h9_usb_loader_returned

/* Trap handler: r0 = the stub's address. Light the GPIO9 LED solid (so a
 * trap is visible even if the USB loader cannot start in this context),
 * record the address and enter the USB loader. */
    .globl far_trap
far_trap:
    r7 = 0x001e5000
    r1 = [r7+0]
    r1 = r1 | 0x200
    [r7+0] = r1
    r1 = [r7+12]
    r1 = r1 | 0x200
    [r7+12] = r1
    r1 = [r7+8]
    r1 = r1 & ~0x200
    [r7+8] = r1
    r1 = 0x0001e4f0
    r2 = 0x50415254
    [r1+0] = r2
    [r1+4] = r0
    r3 = 0x02000d68
    call r3
far_trap_returned:
    goto far_trap_returned
"""


def section(program, name):
    return next(s for s in program.sections if s.name == name)


def assemble(asm, elf, source: str) -> bytes:
    return bytes(section(elf.read(asm.assemble_source("    .section .text,\"ax\",@progbits\n" + source)), ".text").data)


def trap_fill(asm, elf, objdump, table, start: int, end: int, handler: int) -> bytes:
    """Bytes for app range [start, end): gotos to stubs every STUB_EVERY bytes."""
    out = bytearray()
    gotos: dict[int, bytes] = {}
    pos = start
    while pos < end:
        stub = min(pos + STUB_EVERY, end) - STUB_BYTES
        if stub < pos:
            raise ValueError("range too short for a stub")
        while pos < stub:
            disp = stub - (pos + 2)
            if disp not in gotos:
                gotos[disp] = assemble(asm, elf, f"    goto {disp}\n")
                if len(gotos[disp]) != 2:
                    raise ValueError(f"goto {disp} is not 2 bytes")
            out += gotos[disp]
            pos += 2
        code = assemble(asm, elf, f"    r0 = 0x{IMAGE_BASE + stub:08x}\n    r3 = 0x{handler:08x}\n    goto r3\n")
        if len(code) != STUB_BYTES:
            raise ValueError("stub size changed")
        out += code
        pos = stub + STUB_BYTES
    # Check: every halfword start decodes to a goto that lands on a stub, or is inside a stub.
    decoder = objdump.decoder_for(table.load(), mcpu="r3")
    stubs = set()
    pos = start
    while pos < end:
        s = min(pos + STUB_EVERY, end) - STUB_BYTES
        stubs.add(s)
        pos = s + STUB_BYTES
    for off in range(start, end, 2):
        if any(s <= off < s + STUB_BYTES for s in stubs):
            continue
        hit = decoder.decode(bytes(out), off - start)
        if not hit or not hit[3].startswith("goto "):
            raise ValueError(f"fill at app+0x{off:X} does not decode as goto")
        target = off + 2 + int(hit[3].split()[1])
        if target not in stubs:
            raise ValueError(f"goto at app+0x{off:X} misses the stub")
    return bytes(out)


def build(stock_image: Path, stock_package: Path, output: Path, mode: str = "redirect+traps") -> dict:
    stock = stock_image.read_bytes()
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected the exact official v1.0.38 main application")
    if sha256(stock_package.read_bytes()).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the exact official v1.0.38 .fw package")
    if stock[TASK_ENTRY:TASK_ENTRY + 6] != TASK_ORIGINAL:
        raise ValueError("my_app task entry differs from the pinned instruction")
    output.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm, elf, ld, objdump, table  # noqa: E402

    entry = HEADER + (RESEARCH / "c-hello" / "entry.s").read_text(encoding="utf-8") + FAR_API
    entry_obj = output / "monitor_entry.o"
    entry_obj.write_bytes(asm.assemble_source(entry, filename="monitor_entry.s"))
    c_obj = output / "monitor.o"
    result = subprocess.run([str(H9CC), "clang", *C_FLAGS, "-c", "trap-test/monitor.c",
                             "-o", str(c_obj.relative_to(RESEARCH))], cwd=RESEARCH, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"JieLi clang failed:\n{result.stdout}{result.stderr}")
    c_program = elf.read(c_obj.read_bytes())
    if [s.name for s in c_program.sections if s.name.startswith((".data", ".bss", ".sdata", ".sbss")) and len(s.data or b"")]:
        raise ValueError("C code has data sections")
    linked = output / "h9_trap_monitor.elf"
    state, data = ld.link(ld.parse_args(["-T", str(RESEARCH / "far-hello" / "far.ld"), "-o", str(linked),
                                         str(entry_obj), str(c_obj)]))
    if data is None or state.errors:
        raise RuntimeError(f"Pi32 linker failed: {state.errors}")
    linked.write_bytes(data)
    far = elf.read(data)
    header, text = section(far, ".far_header"), section(far, ".text")
    if header.addr != FAR_ADDRESS or text.addr != FAR_ADDRESS + 16 or far.entry != FAR_ADDRESS:
        raise ValueError("Far layout differs from header + text at 0x01EE5000")
    plain = bytes(header.data) + bytes(text.data)
    if len(plain) > FAR_LIMIT:
        raise ValueError("Monitor too large")
    symbols = {s.name: s.value for s in far.symbols if s.name}
    blob = output / "h9_trap_monitor_0xEA000.bin"
    blob.write_bytes(plain)

    patched = bytearray(stock)
    entry_address = FAR_ADDRESS + 8  # header jump to hello_display_hook
    ranges = list(BLOCK)
    if mode in ("redirect+traps", "redirect-only"):
        patched[TASK_ENTRY + 2:TASK_ENTRY + 6] = entry_address.to_bytes(4, "little")
    if mode in ("trampoline", "trampoline-only"):
        jump = assemble(asm, elf, f"    r3 = 0x{entry_address:08x}\n    goto r3\n")
        patched[MY_APP:MY_APP + len(jump)] = jump
        first, last = BLOCK[1]
        ranges = [BLOCK[0], (first, MY_APP), (MY_APP + len(jump), last)]
    fills = []
    if mode not in ("redirect-only", "trampoline-only"):
        for start, end in ranges:
            data_fill = trap_fill(asm, elf, objdump, table, start, end, symbols["far_trap"])
            patched[start:end] = data_fill
            fills.append({"start": hex(start), "end": hex(end), "stubs": -(-(end - start) // STUB_EVERY)})
    tag = {"redirect+traps": "", "trampoline": "_trampoline", "redirect-only": "_redirect_only",
           "trampoline-only": "_trampoline_only"}[mode]
    raw = output / f"h9_trap_test{tag}_NOT_FOR_FLASH.raw"
    raw.write_bytes(patched)

    package = output / f"h9_trap_test{tag}_OFFLINE_EXPERIMENT.fw"
    rebuilt = output / f"h9_trap_test{tag}_REBUILD_CHECK.fw"
    destination = rebuilt if package.exists() else package
    if destination.exists():
        raise ValueError(f"Refusing to overwrite {destination}")
    result = subprocess.run([sys.executable, str(TOOLS / "h9-firmware-research" / "firmware.py"),
                             "repack", str(stock_package), "--app", str(raw), "--output", str(destination)],
                            capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"Repack failed:\n{result.stdout}{result.stderr}")
    if destination == rebuilt:
        same = rebuilt.read_bytes() == package.read_bytes()
        rebuilt.unlink()
        if not same:
            raise ValueError("Rebuilt package differs (not reproducible)")
    sys.path.insert(0, str(RESEARCH / "h9-flasher"))
    from h9_flasher import parse_firmware  # noqa: E402
    original = parse_firmware(stock_package).image
    target = parse_firmware(package).image
    changed = [o for o in range(0, len(original), 4096) if original[o:o + 4096] != target[o:o + 4096]]
    expected = {0x5000, 0xD9000, (TASK_ENTRY + 0x5100) & ~0xFFF, (MY_APP + 0x5100) & ~0xFFF}
    for start, end in BLOCK:
        expected |= set(range((start + 0x5100) & ~0xFFF, end + 0x5100, 0x1000))
    if set(changed) - expected:
        raise ValueError(f"Unexpected sectors changed: {[hex(x) for x in sorted(set(changed) - expected)]}")
    manifest = {
        "kind": "trap_test_my_app_only_block", "mode": mode,
        "task_entry_patch": ({"offset": hex(TASK_ENTRY + 2), "old": "0x01e88280", "new": hex(entry_address)}
                             if not mode.startswith("trampoline") else None),
        "trapped": fills, "far_trap": hex(symbols["far_trap"]), "monitor_main": hex(symbols["h9_main"]),
        "monitor_bytes": len(plain), "monitor_sha256": sha256(plain).hexdigest(),
        "app_sha256": sha256(patched).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target).hexdigest(),
        "changed_flash_sectors": [hex(o) for o in changed],
        "hardware_observation": "Not yet flashed",
    }
    (output / f"h9_trap_test{tag}-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stock-image", type=Path, required=True)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    parser.add_argument("--mode", choices=MODES, default="redirect+traps")
    args = parser.parse_args()
    print(json.dumps(build(args.stock_image.resolve(), args.stock_package.resolve(), args.output.resolve(), args.mode), indent=2))
    print("Offline build only; not hardware tested")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
