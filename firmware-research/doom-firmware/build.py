#!/usr/bin/env python3
"""Build the TD-H9 Doom firmware (first pass): DMC100-Doom's engine with the
TD-H9 platform layer (port/), laid out as proven on hardware 2026-11-09.

Layout:
  - startup (C-hello stage-23 entry.s, section .cave) in the app cave
    app+0xA77F0 (1,704 B), entered by a 4-byte call at my_app's entry
    app+0x88180
  - code and constants, by object file, packed into
      FAR  flash 0xEA000-0xFBFFF (CPU 0x01EE5000, stored plain; write-far)
      R2A  app+0x7D69E-0x88180 \\
      R1   app+0x79E32-0x7D64E  > the my_app-only block (trap-tested free)
      R2B  app+0x88184-0x890E4 /
  - RAM: the engine's fixed core (.data/.bss), frame region and level arena
    in 0x22000-0x2BEFF, taken from the stock heap by lowering its end
    (app+0x05FA9E: r5 = 0x2BF00 -> 0x22000)
Outputs (build/): h9_doom_OFFLINE_EXPERIMENT.fw (main flash, hold-8 flash)
and h9_doom_far_0xEA000.bin (write-far), plus the ELF, map and manifest.

    export JIELI_TOOLCHAIN=~/path/to/jieli-linux-toolchains-20260730.1
    python3 -I build.py --dmc <DMC100-Doom checkout> --stock-image <app.bin> --stock-package <TD-H9-V1.0.38.fw>
The DMC100-Doom checkout must hold the generated tables (gen/) of the pack
on the radio (tools/conv on freedoom-0.13.0, as for E1M1S).
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
RESEARCH = ROOT.parent
TOOLS = RESEARCH / "brick-breaker" / "tools"
ISA = TOOLS / "pi32v2-isa"
STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
# Base app: H9 Toolbox Firmware (stock + Brick Breaker + the S2 resource
# service). Its Bluetooth upload service then works inside the Doom firmware
# (td_main sets the ready marker after setting up the resource flash). The
# startup overwrites Brick Breaker's game in the cave; the service's tables
# right after it (app+0xA7E50) stay. The Brick Breaker launcher hook
# (app+0x88906, in the unused R2B range) is never reached: my_app is replaced.
H9TB_RAW = RESEARCH / "resource-service" / "build" / "h9_toolbox_firmware_NOT_FOR_FLASH.raw"
H9TB_APP_SHA256 = "1334a32c9ec3191ea0c8ca3f657331a5c5ce110a71185db1d075e7cd5d4a5ead"
H9TB_TABLES = 0xA7E50
STOCK_PACKAGE_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
IMAGE_BASE = 0x01E00100
MY_APP = 0x88180
CAVE, CAVE_SIZE = 0xA77F0, 1704
HEAP_END_SITE, HEAP_END_OLD = 0x05FA9E, bytes.fromhex("c5ff00bf0200")
RAM_BASE, RAM_END = 0x22000, 0x2BF00
FAR_FLASH, FAR_CPU, FAR_SIZE = 0xEA000, 0x01EE5000, 0xFC000 - 0xEA000
REGIONS = [  # name, CPU origin, length; filled in this order (largest first)
    ("FAR", FAR_CPU, FAR_SIZE),
    ("R2A", IMAGE_BASE + 0x7D69E, 0x88180 - 0x7D69E),
    ("R1", IMAGE_BASE + 0x79E32, 0x7D64E - 0x79E32),
    ("R2B", IMAGE_BASE + 0x88184, 0x890E4 - 0x88184),
]
FRAME_MAX, TIC_OFFSET, ARENA_MIN = 15208, 6912, 3848
# Start-up stages traced on screen by port/td_trace.c (diagnostic).
WRAPS = ("V_Init", "R_Init", "P_Init", "S_Init", "D_CheckNetGame", "HU_Init", "ST_Init",
         "G_InitNew", "P_SetupLevel", "TryRunTics", "R_RenderPlayerView")
C_FLAGS = ["-target", "pi32v2", "-mcpu=r3", "-Oz", "-integrated-as", "-ffunction-sections", "-fdata-sections",
           "-ffreestanding", "-fno-common", "-fwrapv", "-fsigned-char", "-std=gnu11", "-w"]
INCLUDES = ["engine/stub/include", "engine", "port/data", "port/render", "port/obj", "port/math", "platform",
            "port/game", "port/ovl", "gen"]
EXTRA_API = """
/* int h9_res_read(partition, offset, buffer, length): stock partition read. */
    .globl h9_res_read
h9_res_read:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e52cb2
    call r4
    {pc, r14-r4} = [sp++]

/* void h9_spi2_setup(void): stock bus setup with index 2 (SPI2, the resource
 * flash), as stock my_app does at app+0x0884BA. */
    .globl h9_spi2_setup
h9_spi2_setup:
    [--sp] = {rets, r14-r4}
    r0 = 2
    r4 = 0x01e4c2da
    call r4
    {pc, r14-r4} = [sp++]

/* void h9_nor_read_raw(buffer, chip_address, length): stock SPI2 read
 * 0x01E4FF22 (ROM lock, CS GPIO 23, 03 + 24-bit address, DMA, unlock). The
 * partition read 0x01E52CB2 calls it after a ready-marker check (RAM 0x12234
 * == 0x5AA5) that only stock my_app's start-up satisfies. */
    .globl h9_nor_read_raw
h9_nor_read_raw:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e4ff22
    call r4
    {pc, r14-r4} = [sp++]

/* void h9_post_core_event(unsigned code, unsigned value): stock app+0x2D77A
 * posts a 24-byte event (type 1, code, value) to the app_core task. Any
 * message makes app_core's mode loop apply a pending mode request; code
 * 0xFFFF is ignored by its event handler (app+0x6315C). */
    .globl h9_post_core_event
h9_post_core_event:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e2d87a
    call r4
    {pc, r14-r4} = [sp++]

/* void h9_irq_off(void) / h9_irq_on(void): ROM 0x02001634 / 0x02001650, the
 * interrupt guard the S2 resource service uses around SPI access. */
    .globl h9_irq_off
h9_irq_off:
    [--sp] = {rets, r14-r4}
    r4 = 0x02001634
    call r4
    {pc, r14-r4} = [sp++]
    .globl h9_irq_on
h9_irq_on:
    [--sp] = {rets, r14-r4}
    r4 = 0x02001650
    call r4
    {pc, r14-r4} = [sp++]

/* void h9_speaker_gate(unsigned source, unsigned on): stock app+0x5224C,
 * the speaker amplifier arbiter (source 1 uses the request directly). */
    .globl h9_speaker_gate
h9_speaker_gate:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e5234c
    call r4
    {pc, r14-r4} = [sp++]

/* Stock DAC driver on its handle (0x11AD0+6636), as the key-tone set-up at
 * app+0x611C8 uses it: rate (app+0x433C8, from the table at 0x01EB0BB0),
 * start (app+0x43434), write 128 bytes (app+0x43598). */
    .globl h9_dac_rate
h9_dac_rate:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e434c8
    call r4
    {pc, r14-r4} = [sp++]
    .globl h9_dac_start
h9_dac_start:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e43534
    call r4
    {pc, r14-r4} = [sp++]
    .globl h9_dac_write128
h9_dac_write128:
    [--sp] = {rets, r14-r4}
    r4 = 0x01e43698
    call r4
    {pc, r14-r4} = [sp++]

/* void h9_enter_usb_loader(void): ROM 0x02000D68 (proven 2026-11-09). */
    .globl h9_enter_usb_loader
h9_enter_usb_loader:
    r3 = 0x02000d68
    call r3
h9_usb_loader_returned:
    goto h9_usb_loader_returned
"""


def sources(dmc: Path) -> list[Path]:
    """DMC100's size-gate set with our platform instead of its stubs."""
    out = [p for p in sorted((dmc / "engine").glob("*.c")) if p.name not in ("w_wad.c", "w_file.c")]
    out += [p for p in sorted((dmc / "port/data").glob("*.c")) if p.name not in ("fat12.c", "fat_write.c")]
    out += sorted((dmc / "port/render").glob("*.c")) + [dmc / "port/math/trig.c"]
    out += sorted((dmc / "port/obj").glob("*.c")) + [dmc / "engine/stub/stub_libc.c"]
    out += [dmc / "gen" / n for n in ("info.c", "colormap.c", "trig_tables.c", "view.c")]
    out += sorted((ROOT / "port").glob("*.c"))
    return out


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        raise RuntimeError(f"failed: {' '.join(map(str, cmd))}\n{r.stdout}{r.stderr}")
    return r.stdout


def docker(mount: Path, toolchain: Path, script: str) -> str:
    return run(["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{toolchain}:/opt/jieli:ro",
                "-v", f"{mount}:/w", "-w", "/w", "h9-jieli", "sh", "-c", script])


def compile_all(dmc: Path, mount: Path, toolchain: Path, out: Path) -> list[Path]:
    objs, lines = [], ["set -e"]
    inc = " ".join(f"-I/w/{(dmc / i).relative_to(mount)}" for i in INCLUDES) + f" -I/w/{(ROOT / 'port').relative_to(mount)}"
    for src in sources(dmc):
        tag = "td_" if src.parent == ROOT / "port" else ""
        obj = out / "obj" / (tag + src.relative_to(src.parents[1]).as_posix().replace("/", "_").replace(".c", ".o")
                             if not tag else src.name.replace(".c", ".o"))
        objs.append(obj)
        lines.append(f"/opt/jieli/pi32v2/bin/clang {' '.join(C_FLAGS)} {inc} -c /w/{src.relative_to(mount)} "
                     f"-o /w/{obj.relative_to(mount)}")
    (out / "obj").mkdir(parents=True, exist_ok=True)
    script = out / "compile.sh"
    script.write_text("\n".join(lines) + "\n")
    docker(mount, toolchain, f"sh /w/{script.relative_to(mount)}")
    return objs


def entry_object(out: Path) -> Path:
    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm  # noqa: E402
    text = (RESEARCH / "c-hello" / "entry.s").read_text(encoding="utf-8")
    # Backlight: stock's duty is saved level (RAM 0x12BAA) * 2000 + 2000, and
    # the level is loaded by stock my_app, which never runs here, so the panel
    # came up at the minimum (run 4: "turn the brightness up"). Use the top
    # step, 10000.
    brightness = ("    r1 = 0x12baa\n    r1 = b[r1+0] (u)\n    r1 = r1 * 0x7d0\n    r1 = r1 + 2000\n",
                  "    r1 = 10000\n")
    for old, new in (('.section .text,"ax",@progbits', '.section .cave,"ax",@progbits'),
                     ("    call h9_main\n", "    call td_main\n"), brightness):
        if text.count(old) != 1:
            raise ValueError(f"entry.s: {old!r} not found exactly once")
        text = text.replace(old, new)
    obj = out / "obj" / "td_entry.o"
    obj.write_bytes(asm.assemble_source(text + EXTRA_API, filename="td_entry.s"))
    return obj


def script(placement: dict[str, list[str]] | None) -> str:
    """Linker script. Without a placement: one big region (sizing pass)."""
    lines = ['OUTPUT_FORMAT("elf32-pi32v2")', "ENTRY(hello_display_hook)", "MEMORY {",
             f"  CAVE (rx) : ORIGIN = 0x{IMAGE_BASE + CAVE:08X}, LENGTH = {CAVE_SIZE}"]
    if placement is None:
        lines.append(f"  FAR (rx) : ORIGIN = 0x{FAR_CPU:08X}, LENGTH = 0x100000")
    else:
        lines += [f"  {n} (rx) : ORIGIN = 0x{o:08X}, LENGTH = {l}" for n, o, l in REGIONS]
    lines += [f"  RAM (rwx) : ORIGIN = 0x{RAM_BASE:08X}, LENGTH = {RAM_END - RAM_BASE}", "}", "SECTIONS {",
              "  .cave : { KEEP(*td_entry.o(.cave)) } > CAVE"]
    if placement is None:
        lines.append("  .text : { *(.text .text.* .rodata .rodata.*) } > FAR")
    else:
        for name, _, _ in REGIONS:
            pats = " ".join(f"*{o}(.text .text.* .rodata .rodata.*)" for o in placement.get(name, []))
            lines.append(f"  .{name.lower()} : {{ {pats} }} > {name}")
    data_region = "FAR" if placement is None else "FAR"
    lines += [f"  .data : {{ __data_start = .; *(.data .data.* .sdata .sdata.*) . = ALIGN(4); __data_end = .; }} > RAM AT > {data_region}",
              "  __data_load = LOADADDR(.data);",
              "  .bss (NOLOAD) : { __bss_start = .; *(.bss .bss.* .sbss .sbss.* COMMON) . = ALIGN(16); __bss_end = .; } > RAM",
              "  __frame_start = __bss_end;", f"  __frame_max = {FRAME_MAX};",
              f"  dmc_tic = __frame_start + {TIC_OFFSET};", f"  __ram_end = 0x{RAM_END:08X};",
              "  /DISCARD/ : { *(.comment) *(.note*) }", "}"]
    return "\n".join(lines) + "\n"


def link(mount: Path, toolchain: Path, out: Path, objs: list[Path], text: str, name: str) -> tuple[Path, str]:
    ld = out / f"{name}.ld"
    ld.write_text(text)
    elf_path, map_path = out / f"{name}.elf", out / f"{name}.map"
    rel = lambda p: f"/w/{p.relative_to(mount)}"
    wraps = " ".join(f"--wrap={w}" for w in WRAPS)
    docker(mount, toolchain, f"/opt/jieli/pi32v2/bin/ld --gc-sections {wraps} -T {rel(ld)} -Map {rel(map_path)} "
                             f"-o {rel(elf_path)} " + " ".join(rel(o) for o in objs))
    undefined = docker(mount, toolchain, f"/opt/jieli/pi32v2/bin/nm -u {rel(elf_path)}").strip()
    if undefined:
        raise RuntimeError(f"undefined symbols:\n{undefined}")
    return elf_path, map_path.read_text()


def object_sizes(map_text: str) -> dict[str, int]:
    """Bytes of .text/.rodata input sections per object kept by the sizing link."""
    sizes: dict[str, int] = {}
    pending = None
    for line in map_text.splitlines():
        m = re.match(r"^ (\.(?:text|rodata)\S*)\s*(?:(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+\.o))?\s*$", line)
        if m and m.group(4):
            sizes[Path(m.group(4)).name] = sizes.get(Path(m.group(4)).name, 0) + int(m.group(3), 16)
            pending = None
        elif m:
            pending = True
        elif pending:
            m2 = re.match(r"^\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+\.o)\s*$", line)
            if m2:
                sizes[Path(m2.group(3)).name] = sizes.get(Path(m2.group(3)).name, 0) + int(m2.group(2), 16)
            pending = None
    return sizes


def pack(sizes: dict[str, int]) -> dict[str, list[str]]:
    """First fit, largest object first, with 2-byte alignment slack per section."""
    left = {n: l for n, _, l in REGIONS}
    placement: dict[str, list[str]] = {n: [] for n, _, _ in REGIONS}
    for obj, size in sorted(sizes.items(), key=lambda kv: -kv[1]):
        need = size + 64  # alignment padding margin
        for name, _, _ in REGIONS:
            if left[name] >= need:
                placement[name].append(obj)
                left[name] -= need
                break
        else:
            raise ValueError(f"{obj} ({size} B) does not fit in any region: {left}")
    return placement


def build(dmc: Path, stock_image: Path, stock_package: Path, toolchain: Path, out: Path) -> dict:
    stock = stock_image.read_bytes()
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected the exact official v1.0.38 main application")
    if sha256(stock_package.read_bytes()).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the exact official v1.0.38 .fw package")
    if stock[HEAP_END_SITE:HEAP_END_SITE + 6] != HEAP_END_OLD:
        raise ValueError("Heap-end instruction differs from the pinned one")
    if stock[CAVE:CAVE + CAVE_SIZE] != b"\xff" * CAVE_SIZE:
        raise ValueError("Cave is not erased in stock")
    base = H9TB_RAW.read_bytes()
    if sha256(base).hexdigest() != H9TB_APP_SHA256:
        raise ValueError("H9 Toolbox Firmware base app differs from the pinned build")
    mount = Path(os.path.commonpath([dmc, ROOT]))
    out.mkdir(parents=True, exist_ok=True)
    objs = compile_all(dmc, mount, toolchain, out) + [entry_object(out)]

    _, sizing_map = link(mount, toolchain, out, objs, script(None), "h9_doom_sizing")
    sizes = object_sizes(sizing_map)
    placement = pack(sizes)
    elf_path, _ = link(mount, toolchain, out, objs, script(placement), "h9_doom")

    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm, elf  # noqa: E402
    program = elf.read(elf_path.read_bytes())
    sec = {s.name: s for s in program.sections}
    sym = {s.name: s.value for s in program.symbols if s.name}
    region_bytes = lambda name: bytes(sec[f".{name.lower()}"].data or b"") if f".{name.lower()}" in sec else b""
    used = {name: len(region_bytes(name)) for name, _, _ in REGIONS}
    data_bytes = bytes(sec[".data"].data or b"")
    bss_end, frame = sym["__bss_end"], sym["__frame_start"]
    arena = RAM_END - (frame + FRAME_MAX)
    if arena < ARENA_MIN:
        raise ValueError(f"Level arena too small: {arena} B")

    # Far blob: the FAR section, then .data's load image right after it.
    far = bytearray(region_bytes("FAR"))
    if sym["__data_load"] != FAR_CPU + len(far):
        raise ValueError(f"unexpected .data load address 0x{sym['__data_load']:X}")
    far += data_bytes
    if len(far) > FAR_SIZE:
        raise ValueError("far region overflow")

    patched = bytearray(base)
    hook = bytes(next(s for s in elf.read(asm.assemble_source(
        f'    .section .text,"ax",@progbits\n    call {CAVE - MY_APP - 4}\n')).sections if s.name == ".text").data)
    patched[MY_APP:MY_APP + 4] = hook
    cave = bytes(sec[".cave"].data)
    if sec[".cave"].addr != IMAGE_BASE + CAVE or program.entry != IMAGE_BASE + CAVE:
        raise ValueError("startup is not at the cave start")
    if CAVE + len(cave) > H9TB_TABLES:
        raise ValueError("startup would overwrite the resource service tables")
    patched[CAVE:CAVE + len(cave)] = cave
    for name, origin, length in REGIONS[1:]:
        data = region_bytes(name)
        if data:
            s = sec[f".{name.lower()}"]
            if not (origin <= s.addr and s.addr + len(data) <= origin + length):
                raise ValueError(f"{name} section at 0x{s.addr:X} (+{len(data)}) is outside its region")
            patched[s.addr - IMAGE_BASE:s.addr - IMAGE_BASE + len(data)] = data
    patched[HEAP_END_SITE + 2:HEAP_END_SITE + 6] = RAM_BASE.to_bytes(4, "little")
    raw = out / "h9_doom_NOT_FOR_FLASH.raw"
    raw.write_bytes(patched)
    (out / "h9_doom_far_0xEA000.bin").write_bytes(far)

    package = out / "h9_doom_OFFLINE_EXPERIMENT.fw"
    if package.exists():
        package.unlink()
    run([sys.executable, str(TOOLS / "h9-firmware-research" / "firmware.py"), "repack", str(stock_package),
         "--app", str(raw), "--output", str(package)])
    sys.path.insert(0, str(RESEARCH / "h9-flasher"))
    from h9_flasher import parse_firmware  # noqa: E402
    original, target = parse_firmware(stock_package).image, parse_firmware(package).image
    changed = [o for o in range(0, len(original), 4096) if original[o:o + 4096] != target[o:o + 4096]]
    allowed = {0x5000, 0xD9000, 0x62000, 0xAA000, 0xAE000, 0xAF000,  # + H9 Toolbox's own sectors
                (HEAP_END_SITE + 0x5100) & ~0xFFF, (CAVE + 0x5100) & ~0xFFF,
               (CAVE + CAVE_SIZE + 0x5100) & ~0xFFF} | set(range((0x79E32 + 0x5100) & ~0xFFF, 0x890E4 + 0x5100, 0x1000))
    if set(changed) - allowed:
        raise ValueError(f"unexpected sectors: {[hex(c) for c in sorted(set(changed) - allowed)]}")
    manifest = {
        "kind": "td_h9_doom_firmware_first_pass",
        "engine": "DMC100-Doom 0cdc78e (GPL-2.0-or-later) size-gate set + TD-H9 port/",
        "base": "H9 Toolbox Firmware app 1334a32c (Bluetooth upload service included)",
        "cave_bytes": len(cave), "regions_bytes": used, "regions_capacity": {n: l for n, _, l in REGIONS},
        "far_blob_bytes": len(far), "data_bytes": len(data_bytes),
        "ram": {"core": hex(RAM_BASE), "bss_end": hex(bss_end), "frame_start": hex(frame),
                "frame_max": FRAME_MAX, "arena_bytes": arena, "ram_end": hex(RAM_END)},
        "td_main": hex(sym["td_main"]), "D_DoomMain": hex(sym["D_DoomMain"]),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target).hexdigest(),
        "far_blob_sha256": sha256(bytes(far)).hexdigest(),
        "changed_flash_sectors": [hex(c) for c in changed],
        "placement": placement,
        "hardware_observation": "Not yet flashed",
    }
    (out / "h9_doom-manifest.json").write_text(json.dumps(manifest, indent=1) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dmc", type=Path, required=True)
    parser.add_argument("--stock-image", type=Path, required=True)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    toolchain = Path(os.environ["JIELI_TOOLCHAIN"]).expanduser().resolve()
    m = build(args.dmc.resolve(), args.stock_image.resolve(), args.stock_package.resolve(), toolchain,
              args.output.resolve())
    print(json.dumps({k: v for k, v in m.items() if k != "placement"}, indent=1))
    print("Offline build only; not hardware tested")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
