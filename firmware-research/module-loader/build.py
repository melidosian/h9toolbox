#!/usr/bin/env python3
"""Build H9 Toolbox Firmware with the RAM module loader (offline experiment).

Outputs (build/):
  h9_toolbox_modules_OFFLINE_EXPERIMENT.fw  main flash: H9 Toolbox Firmware
      (S2 resource service) with Brick Breaker removed, the loader stub in its
      cave, the hold-Back hook pointed at the stub, and 8 KiB of RAM
      (0x29F00-0x2BEFF) taken from the stock heap for modules
  h9_modules.bin   the module bundle to upload to the "modules" slot
      (dist/doom/upload.mjs SLOTS.modules: directory 0xCC0000)
  h9_toolbox_modules-manifest.json

Modules: 0 = menu (C, JieLi clang in Docker), 1 = Brick Breaker (the
hardware-tested game.s, unchanged, relinked for RAM).

    python3 -I build.py --stock-image <app.bin> --stock-package <TD-H9-V1.0.38.fw>
JIELI_TOOLCHAIN selects the extracted JieLi toolchain (default: the bundled
toolchain/jieli/ folder after setup.sh).
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
RESEARCH = ROOT.parent
TOOLS = RESEARCH / "brick-breaker" / "tools"
ISA = TOOLS / "pi32v2-isa"
sys.path.insert(0, str(ISA))
from pi32v2isa import asm, elf, ld  # noqa: E402

STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
STOCK_PACKAGE_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
# Base: H9 Toolbox Firmware (Brick Breaker + S2), resource-service --stage bb-s2.
H9TB_RAW = RESEARCH / "resource-service" / "build" / "h9_toolbox_firmware_NOT_FOR_FLASH.raw"
H9TB_APP_SHA256 = "1334a32c9ec3191ea0c8ca3f657331a5c5ce110a71185db1d075e7cd5d4a5ead"
H9TB_SECTORS = {0x5000, 0x62000, 0x8D000, 0xAA000, 0xAC000, 0xAE000, 0xAF000, 0xD9000}
BB_GAME_SHA256 = "19ab1b31a386634d04c964d748ceb838b773265e06ba842de0c3df8c25d0b536"

IMAGE_BASE = 0x01E00100
HOOK = 0x88906                       # my_app loop call, hooked by Brick Breaker
CAVE, TABLES = 0xA77F0, 0xA7E50      # Brick Breaker's game cave; S2 tables follow
OLD_LAUNCHER, SVC = 0xA9D9E, 0xA9E00  # Brick Breaker's launcher; S2 .svc follows
HEAP_END_SITE, HEAP_END_OLD = 0x05FA9E, bytes.fromhex("c5ff00bf0200")
# The serial / USB clone transport's write routine pointer (the literal 0x01E78348 loaded into the ops table in
# app+0x78A9E, "c2ff 4883 e701"): retargeted to the stub's ml_serial_write (src/stub.s explains why).
SERIAL_WRITE_SITE, SERIAL_WRITE_OLD = 0x78AA0, bytes.fromhex("4883e701")
LOAD, SLOT_END = 0x29F00, 0x2BF00
APRS_BOOT_SITE = 0x88186
APRS_SERVICE_RAM = 0x29C00
RESIDENT_RAM = 0x29A00
RESIDENT_FLASH, RESIDENT_CPU = 0xFB000, 0x01EF6000
# S2 control allowlist and pre-program erased-block check in the pinned bb-s2 base.
S2_WRITE_GUARDS = ((0xA52A4, 0xA542C, "ml_s2_allowed"),
                   (0xA535A, 0xA548E, "ml_s2_program_check"))
KISS_MANIFEST = RESEARCH / "kiss-tnc" / "main_kiss_patch.json"
KISS_MANIFEST_SHA256 = "2f9b8b2f155f762c2e4a5f13a8892429fcc0ab09dd755f2aa941e1abe1e58c74"
KISS_HELPER = 0xA7C7C
KISS_CALLS = {0x5DB40: 0xA7CAA, 0x5DB6A: 0xA7CB6, 0x5DB86: 0xA7CE8, 0x870E0: 0xA7CF8}
SLOT_DIRECTORY, SLOT_DATA, SLOT_LIMIT = 0xCC0000, 0xCC1000, 0xCE0000

BUNDLE_MAGIC, BUNDLE_VERSION = b"H9MODS01", 1
HEADER, ENTRY, NAME = 16, 32, 16
C_FLAGS = ["-target", "pi32v2", "-mcpu=r3", "-Oz", "-integrated-as", "-ffreestanding", "-fno-builtin",
           "-fno-common", "-ffunction-sections", "-Wall", "-Werror"]
# Extra compiler flags for test builds, e.g. H9_CFLAGS=-DAPRS_SPEAKER_TEST.
C_FLAGS += os.environ.get("H9_CFLAGS", "").split()


def crc16(data: bytes) -> int:
    """CRC-16/XMODEM, as the stub, the S2 service and the uploader compute it."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def bundle(modules: list[dict]) -> bytes:
    """H9MODS01 bundle: header, 32-byte entries, then 4-byte aligned bodies."""
    if not 1 <= len(modules) <= 255:
        raise ValueError("a bundle holds 1-255 modules")
    offset = HEADER + ENTRY * len(modules)
    entries, bodies = b"", b""
    for m in modules:
        body, name = m["body"], m["name"].encode("ascii")
        if len(name) > NAME or not body:
            raise ValueError(f"bad module {m['name']!r}")
        if len(body) + m["bss"] > SLOT_END - LOAD or not 0 <= m["entry"] < len(body):
            raise ValueError(f"module {m['name']!r} does not fit the {SLOT_END - LOAD}-byte slot")
        pad = (-offset) % 4
        bodies += b"\xff" * pad
        offset += pad
        entries += name.ljust(NAME, b"\0") + struct.pack("<IIIHH", offset, len(body), m["bss"], m["entry"],
                                                         crc16(body))
        bodies += body
        offset += len(body)
    data = BUNDLE_MAGIC + struct.pack("<HHI", BUNDLE_VERSION, len(modules), offset) + entries + bodies
    if len(data) != offset or len(data) > SLOT_LIMIT - SLOT_DATA:
        raise ValueError("bundle does not fit the modules slot")
    return data


def parse_bundle(data: bytes) -> list[dict]:
    """The checks the stub makes (ml_check, ml_load), for tests and tools."""
    if data[:8] != BUNDLE_MAGIC:
        raise ValueError("not an H9MODS01 bundle")
    version, count, length = struct.unpack_from("<HHI", data, 8)
    if version != BUNDLE_VERSION or not 1 <= count <= 255 or length != len(data):
        raise ValueError("bad bundle header")
    out = []
    for i in range(count):
        e = data[HEADER + ENTRY * i:HEADER + ENTRY * (i + 1)]
        offset, size, bss, entry, crc = struct.unpack_from("<IIIHH", e, NAME)
        body = data[offset:offset + size]
        if not (0 < size and size + bss <= SLOT_END - LOAD and entry < size and offset + size <= len(data)):
            raise ValueError(f"bad entry {i}")
        if crc16(body) != crc:
            raise ValueError(f"CRC mismatch in entry {i}")
        out.append({"name": e[:NAME].rstrip(b"\0").decode("ascii"), "offset": offset, "size": size, "bss": bss,
                    "entry": entry, "body": body})
    return out


def run(cmd: list, **kw) -> str:
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        raise RuntimeError(f"failed: {' '.join(map(str, cmd))}\n{r.stdout}{r.stderr}")
    return r.stdout


def assemble(source: Path, out: Path) -> Path:
    obj = out / (source.stem + ".o")
    obj.write_bytes(asm.assemble_source(source.read_text(encoding="utf-8"), filename=str(source)))
    return obj


def link(objs: list[Path], origin: int, entry: str, out: Path, name: str) -> elf.Program:
    script = out / f"{name}.ld"
    script.write_text('OUTPUT_FORMAT("elf32-pi32v2")\nOUTPUT_ARCH(pi32v2)\n'
                      f"ENTRY({entry})\nSECTIONS {{\n    . = 0x{origin:08X};\n"
                      "    .text : { *(.text .text.*) *(.rodata .rodata.*) }\n"
                      "    /DISCARD/ : { *(.comment) *(.note*) }\n}\n")
    path = out / f"{name}.elf"
    state, data = ld.link(ld.parse_args(["--gc-sections", "-T", str(script), "-o", str(path), *map(str, objs)]))
    if data is None or state.errors:
        raise RuntimeError(f"Pi32 linker failed for {name}: {state.errors}")
    path.write_bytes(data)
    program = elf.read(data)
    # The loader copies .text only and zeroes nothing: globals (even zeroed
    # ones in .bss) would land past the code, unloaded and unzeroed, possibly
    # inside the shared state at the top of the slot. Modules use fixed
    # addresses for state instead.
    loaded = [s for s in program.sections if s.name.startswith((".data", ".bss", ".sdata", ".sbss")) and s.length]
    if loaded:
        raise ValueError(f"{name}: data/bss sections are not supported: {[(s.name, s.length) for s in loaded]}")
    undefined = sorted({s.name for s in program.symbols if s.name and s.shndx == 0})
    if undefined:
        raise ValueError(f"{name}: undefined symbols {undefined}")
    return program


def text_of(program: elf.Program, origin: int) -> bytes:
    text = next(s for s in program.sections if s.name == ".text")
    if text.addr != origin:
        raise ValueError(f".text at 0x{text.addr:X}, expected 0x{origin:X}")
    return bytes(text.data)


def symbol(program: elf.Program, name: str) -> int:
    values = [s.value for s in program.symbols if s.name == name]
    if len(values) != 1:
        raise ValueError(f"symbol {name} not found exactly once")
    return values[0]


def compile_c(source: Path, out: Path, toolchain: Path) -> Path:
    obj = out / (source.stem + ".o")
    rel = lambda p: "/w/" + p.relative_to(RESEARCH).as_posix()
    run(["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{toolchain}:/opt/jieli:ro",
         "-v", f"{RESEARCH}:/w", "-w", "/w", "h9-jieli", "/opt/jieli/pi32v2/bin/clang", *C_FLAGS,
         # vendored third-party code is used unchanged: its `(T){0}` initialiser trips -Wmissing-braces
         *(["-Wno-missing-braces"] if "third_party" in source.parts else []),
         "-I", rel(ROOT / "modules" / "common"), "-c", rel(source), "-o", rel(obj)])
    return obj


def build_modules(out: Path, toolchain: Path, resident: bool = False) -> list[dict]:
    common = ROOT / "modules" / "common"
    run([sys.executable, "-I", str(common / "font5x7.py")])
    lib = [assemble(common / "h9lib.s", out), compile_c(common / "gfx.c", out, toolchain)]

    def c_module(name: str, source: Path, *extra: Path) -> dict:
        objs = [compile_c(source, out, toolchain), *(compile_c(e, out, toolchain) for e in extra)]
        prog = link([*objs, *lib], LOAD, "module_main", out, "mod_" + source.stem)
        return {"name": name, "body": text_of(prog, LOAD), "bss": 0, "entry": symbol(prog, "module_main") - LOAD}

    menu = c_module("MENU", ROOT / "modules" / "menu" / "menu.c", ROOT / "modules" / "common" / "vfo_backup.c")
    about = c_module("ABOUT", ROOT / "modules" / "about" / "about.c")
    maze = c_module("MAZE CHASE", ROOT / "modules" / "maze" / "maze.c", ROOT / "modules" / "common" / "snd.c")
    aprs = c_module("APRS MONITOR", ROOT / "modules" / "aprs_monitor" / "aprs_monitor.c")
    if len(aprs["body"]) > 0x2000 - 0x800:
        raise SystemExit("APRS MONITOR code overlaps its state in the top 2 KiB of the RAM slot")
    # APRS screens share state at 0x2BEE0 - 0x570 (aprs_core.h APRS_ADDR); each
    # screen's code must end below it. ".APRS INFO" is hidden from the menu.
    aprs_dir = ROOT / "modules" / "aprs"
    aprs_limit = 0x2BEE0 - 0x570 - LOAD            # aprs_core.h APRS_SLOT_BYTES
    core = aprs_dir / "aprs_core.c"
    text = aprs_dir / "aprs_text.c"
    screens = [c_module("APRS", aprs_dir / "aprs_load.c", core, aprs_dir / "aprs_stored.c"),
               c_module(".APRS LIST", aprs_dir / "aprs_list.c", core),
               c_module(".APRS INFO", aprs_dir / "aprs_info.c", core, text),
               c_module("APRS MAP", aprs_dir / "aprs_map.c", core),
               c_module("APRS MSG", aprs_dir / "aprs_msg.c", core, text),
               c_module("APRS SEND", aprs_dir / "aprs_send.c", aprs_dir / "aprs_core_tx.c", aprs_dir / "aprs_tx.c",
                        aprs_dir / "aprs_edit.c", text)]
    if resident:
        screens.append(c_module("APRS BEACON", aprs_dir / "aprs_resident_config.c",
                                common / "toolbox_settings_pair.c"))
    else:
        beacon_common = (aprs_dir / "aprs_tx.c", aprs_dir / "beacon_core.c",
                         aprs_dir / "third_party" / "smartbeacon" / "smartbeacon.c")
        beacon = c_module("APRS BEACON", aprs_dir / "aprs_beacon.c", aprs_dir / "aprs_core_tx.c", *beacon_common,
                          common / "fix_store.c")
        beacon_background = c_module(".APRS BG", aprs_dir / "aprs_beacon_resident.c",
                                     aprs_dir / "aprs_core_tx.c", *beacon_common)
        if len(beacon_background["body"]) > 0x2B700 - LOAD:
            raise SystemExit(".APRS BG code overlaps its resident beacon state")
        screens.extend((beacon, beacon_background))
    for m in screens:
        if len(m["body"]) > aprs_limit:
            raise SystemExit(f"{m['name']} code ({len(m['body'])} B) overlaps the shared APRS state ({aprs_limit} B max)")
    # SAT PASS reads APRS's last GPS fix from the shared state: stay below it
    # so the fix survives.
    # Both SAT PASS screens also stay below satshare.h's 96 bytes (just under APRS's state).
    sat_limit = 0x2BEE0 - 0x600 - 0x60 - LOAD       # satshare.h SAT_SHARED: fixed (the stub hard-codes it)
    sat_dir = ROOT / "modules" / "satpass"
    satpass = c_module("SAT PASS", sat_dir / "satpass.c", sat_dir / "orbit.c", ROOT / "modules" / "common" / "fix_store_save.c")
    satplot = c_module(".SAT PLOT", sat_dir / "satplot.c", sat_dir / "orbit.c")
    sattrack = c_module(".SAT TRACK", sat_dir / "sattrack.c", sat_dir / "orbit.c")
    # The satellite table is data, not code: the hidden module ".SAT DATA" (satpack.py's body from the
    # bundled TLE snapshot; the App Manager replaces it with fresh elements and the chosen satellites).
    sys.path.insert(0, str(ROOT))   # python -I does not add the script folder
    import satpack
    sat_body, sat_info = satpack.pack(satpack.parse_tles((sat_dir / "default.tle").read_text()), satpack.load_satellites())
    (out / "satdata.bin").write_bytes(sat_body)
    satdata = {"name": ".SAT DATA", "body": sat_body, "bss": 0, "entry": 0}
    satauto = c_module(".SAT TRACKER", sat_dir / "satauto.c", sat_dir / "orbit.c", ROOT / "modules" / "common" / "vfo_backup.c")
    for m in (satpass, satplot, sattrack, satauto):
        if len(m["body"]) > sat_limit:
            raise SystemExit(f"{m['name']} code ({len(m['body'])} B) overlaps the shared SAT PASS / APRS state ({sat_limit} B max)")
    # GPS information app (reads the saved fix; stays below APRS's state too).
    gps = c_module("GPS", ROOT / "modules" / "gps" / "gps.c", ROOT / "modules" / "common" / "fix_store.c")
    if len(gps["body"]) > aprs_limit:
        raise SystemExit(f"GPS code ({len(gps['body'])} B) overlaps the shared APRS state ({aprs_limit} B max)")
    # Research probe (read-only): stock's tuning state; stays below APRS's state.
    vfo_probe = c_module("VFO PROBE", ROOT / "modules" / "vfo_probe" / "vfo_probe.c")
    if len(vfo_probe["body"]) > aprs_limit:
        raise SystemExit("VFO PROBE overlaps the shared APRS state")

    # Brick Breaker: prove game.s is the hardware-tested source (its bytes at
    # the original cave), then relink it unchanged for RAM. Its entry re-runs
    # the panel set-up, as the original launcher did.
    game = assemble(RESEARCH / "brick-breaker" / "src" / "game.s", out)
    original = text_of(link([game], IMAGE_BASE + CAVE, "hello_display_hook", out, "bb_cave_check"),
                       IMAGE_BASE + CAVE)
    if sha256(original).hexdigest() != BB_GAME_SHA256:
        raise ValueError("game.s differs from the hardware-tested Brick Breaker game")
    bb_prog = link([game], LOAD, "brick_breaker_after_stock_setup", out, "mod_brick_breaker")
    bb = {"name": "BRICK BREAKER", "body": text_of(bb_prog, LOAD), "bss": 0,
          "entry": symbol(bb_prog, "brick_breaker_after_stock_setup") - LOAD}
    # Hardware test for the stub's return-to-radio path (not shipped in the catalog): a
    # one-module bundle whose "menu" returns 0 at once, so holding Back should flash and
    # leave the radio running (older stubs reset here instead).
    reset_test = c_module("MENU", ROOT / "modules" / "reset_test" / "reset_test.c")
    test_bundle = bundle([reset_test])
    parse_bundle(test_bundle)
    (out / "test-ml-reset-bundle.bin").write_bytes(test_bundle)
    return [menu, bb, *screens, aprs, satpass, satplot, sattrack, satauto, satdata, gps, vfo_probe, maze, about]


def call_bytes(target: int, site: int) -> bytes:
    code = bytes(next(s for s in elf.read(asm.assemble_source(f"call {target - site - 4}\n")).sections
                      if s.name == ".text").data)
    if len(code) != 4:
        raise ValueError("hook call is not 4 bytes")
    return code


def apply_dev_kiss(app: bytearray, stub_end: int,
                   retired: tuple[int, int] | None = None) -> tuple[dict, set[int]]:
    """Move the pinned position-independent KISS helpers to verified free space."""
    raw = KISS_MANIFEST.read_bytes()
    if sha256(raw).hexdigest() != KISS_MANIFEST_SHA256:
        raise ValueError("KISS manifest differs from the release audited for relocation")
    manifest = json.loads(raw)
    helper_start, helper_limit = retired or ((stub_end + 3) & ~3, TABLES)
    delta = helper_start - KISS_HELPER
    edits = []
    for patch in manifest["patches"]:
        site = int(patch["offset"], 16)
        before = bytes.fromhex(patch["before"])
        code = bytes.fromhex(patch["after"])
        if site == KISS_HELPER:
            site = helper_start
            if site + len(code) > helper_limit or (not retired and not CAVE <= stub_end <= site):
                raise ValueError("KISS helpers do not fit in their reserved space")
            if retired:
                before = bytes(len(before))
            helper_bytes = len(code)
        elif site in KISS_CALLS:
            target = KISS_CALLS[site]
            if code[:4] != call_bytes(target, site):
                raise ValueError(f"KISS call at app+{site:#x} differs from its audited target")
            code = call_bytes(target + delta, site) + code[4:]
        if app[site:site + len(before)] != before:
            raise ValueError(f"KISS patch overlaps changed bytes at app+{site:#x}")
        edits.append((site, code))

    for site, code in edits:
        app[site:site + len(code)] = code
    sectors = {(offset + 0x5100) & ~0xFFF
               for site, code in edits for offset in (site, site + len(code) - 1)}
    return {"source_package_sha256": manifest["package_sha256"],
            "helper_app_offset": hex(helper_start), "helper_bytes": helper_bytes}, sectors


def build_resident(out: Path, toolchain: Path) -> bytes:
    service = ROOT / "resident-service"
    aprs = ROOT / "modules" / "aprs"
    common = ROOT / "modules" / "common"
    sources = (service / "runtime.c", service / "modem_input.c", service / "beacon_service.c",
               aprs / "beacon_core.c", aprs / "third_party" / "smartbeacon" / "smartbeacon.c",
               common / "toolbox_settings_pair.c", common / "gfx.c")  # compiler's freestanding memcpy
    objects = [assemble(service / "entry.s", out)]
    objects += [compile_c(src, out, toolchain) for src in sources]
    program = link(objects, RESIDENT_CPU, "resident_entry", out, "resident_beacon")
    body = text_of(program, RESIDENT_CPU)
    extra = [s.name for s in program.sections if s.flags & 2 and s.length and s.name != ".text"]
    if program.entry != RESIDENT_CPU or len(body) > 0x1000 or extra:
        raise ValueError(f"Resident service must fit its one-sector executable window ({len(body)} B, {extra})")
    return body


def build_firmware(stock: bytes, stock_package: Path, out: Path, aprs_mute: bool = False,
                   disable_morse: bool = False, dev_kiss: bool = False,
                   resident: bytes | None = None, resident_screen_crc: int | None = None) -> dict:
    if resident and not (aprs_mute and disable_morse and dev_kiss):
        raise ValueError("Resident APRS requires --aprs-mute, --disable-morse and --dev-kiss")
    if resident and resident_screen_crc is None:
        raise ValueError("Resident APRS requires the CRC of its matching screen")
    if not H9TB_RAW.is_file():
        raise SystemExit(f"Build H9 Toolbox Firmware first: resource-service/build.py --stage bb-s2 ({H9TB_RAW})")
    base = H9TB_RAW.read_bytes()
    if sha256(base).hexdigest() != H9TB_APP_SHA256:
        raise ValueError("H9 Toolbox Firmware base app differs from the pinned build")
    if base[HOOK:HOOK + 4] != call_bytes(OLD_LAUNCHER, HOOK):
        raise ValueError("hook does not call Brick Breaker's launcher")
    if stock[HEAP_END_SITE:HEAP_END_SITE + 6] != HEAP_END_OLD or base[HEAP_END_SITE:HEAP_END_SITE + 6] != HEAP_END_OLD:
        raise ValueError("heap-end instruction differs from the pinned one")
    if stock[CAVE:TABLES] != b"\xff" * (TABLES - CAVE) or stock[OLD_LAUNCHER:SVC] != b"\xff" * (SVC - OLD_LAUNCHER):
        raise ValueError("Brick Breaker's caves are not erased in stock")

    stub_source = ROOT / "src" / "stub.s"
    if aprs_mute:
        combined = out / "stub_with_mute.s"
        definitions = ""
        if resident:
            definitions = (f".equ ML_RUNTIME_BOOT, {IMAGE_BASE + OLD_LAUNCHER:#x}\n"
                           f".equ ML_RESIDENT_SCREEN_CRC, {resident_screen_crc}\n")
        combined.write_text(definitions + stub_source.read_text() + "\n" + (ROOT / "src" / "aprs_mute.s").read_text())
        stub_source = combined
    stub_prog = link([assemble(stub_source, out)], IMAGE_BASE + CAVE, "ml_hotkey", out, "stub")
    stub = bytearray(text_of(stub_prog, IMAGE_BASE + CAVE))
    version_text = (ROOT / "VERSION").read_text().strip()      # H9 Toolbox Firmware version, "major.minor.patch"
    major, minor, patch_no = (int(part) for part in version_text.split("."))
    if not (major < 256 and minor < 256 and patch_no < 256):
        raise ValueError(f"VERSION {version_text!r} must be three numbers below 256")
    version_word = major << 16 | minor << 8 | patch_no
    version_at = symbol(stub_prog, "ml_fw_version") - (IMAGE_BASE + CAVE)
    if stub[version_at:version_at + 4] != b"\0\0\0\0":
        raise ValueError("ml_fw_version is not the empty word the build expects")
    stub[version_at:version_at + 4] = version_word.to_bytes(4, "little")
    stub = bytes(stub)
    if stub_prog.entry != IMAGE_BASE + CAVE or len(stub) > TABLES - CAVE:
        raise ValueError(f"stub ({len(stub)} B) must start the cave and end before the S2 tables")

    if stock[SERIAL_WRITE_SITE:SERIAL_WRITE_SITE + 4] != SERIAL_WRITE_OLD or base[SERIAL_WRITE_SITE:SERIAL_WRITE_SITE + 4] != SERIAL_WRITE_OLD:
        raise ValueError("the serial clone write pointer differs from the pinned one")
    serial_write = symbol(stub_prog, "ml_serial_write")

    patched = bytearray(base)
    patched[CAVE:TABLES] = stock[CAVE:TABLES]                 # remove the game
    patched[OLD_LAUNCHER:SVC] = stock[OLD_LAUNCHER:SVC]       # remove its launcher
    if resident:
        if len(resident) > 0x1000:
            raise ValueError("Resident blob exceeds its sector")
        boot = out / "runtime_boot.s"
        boot.write_text(f".equ RUNTIME_ENTRY, {RESIDENT_CPU:#x}\n"
                        f".equ RUNTIME_LENGTH, {len(resident)}\n"
                        f".equ RUNTIME_CRC, {crc16(resident)}\n"
                        f".equ LOADER_CRC, {symbol(stub_prog, 'ml_crc'):#x}\n"
                        + (ROOT / "src" / "runtime_boot.s").read_text())
        addr = IMAGE_BASE + OLD_LAUNCHER
        verifier = text_of(link([assemble(boot, out)], addr, "ml_runtime_boot", out, "runtime_boot"), addr)
        if len(verifier) > SVC - OLD_LAUNCHER:
            raise ValueError("Resident verifier would overlap the S2 service")
        patched[OLD_LAUNCHER:OLD_LAUNCHER + len(verifier)] = verifier
        (out / "resident_0xFB000_OFFLINE.bin").write_bytes(resident)
    patched[CAVE:CAVE + len(stub)] = stub
    api_at = patched.find(b"H9AP", CAVE, CAVE + len(stub))        # the loader's API table: magic, version, ..., firmware word
    if api_at < 0 or struct.unpack_from("<I", patched, api_at + 24)[0] != version_word or struct.unpack_from("<I", patched, api_at + 4)[0] < 2:
        raise ValueError("the image's API table does not carry the firmware version")
    patched[HOOK:HOOK + 4] = call_bytes(CAVE, HOOK)
    heap_end = RESIDENT_RAM if resident else APRS_SERVICE_RAM
    patched[HEAP_END_SITE + 2:HEAP_END_SITE + 6] = heap_end.to_bytes(4, "little")
    patched[SERIAL_WRITE_SITE:SERIAL_WRITE_SITE + 4] = serial_write.to_bytes(4, "little")
    for site, original, name in S2_WRITE_GUARDS:
        if base[site:site + 4] != call_bytes(original, site):
            raise ValueError(f"Resource write guard at {site:#x} differs from the pinned S2 call")
        patched[site:site + 4] = call_bytes(symbol(stub_prog, name) - IMAGE_BASE, site)
    mute_sites = ((0x7B7A0, 0x5224C, "ml_mute_squelch"), (0x706A2, 0x52228, "ml_mute_volume"))
    if aprs_mute:
        for site, original, name in mute_sites:
            expected_call = call_bytes(original, site)
            if stock[site:site + 4] != expected_call or base[site:site + 4] != expected_call:
                raise ValueError(f"APRS mute hook at {site:#x} differs from stock")
            patched[site:site + 4] = call_bytes(symbol(stub_prog, name) - IMAGE_BASE, site)
    boot_call = call_bytes(0x515C6, APRS_BOOT_SITE)
    if stock[APRS_BOOT_SITE:APRS_BOOT_SITE + 4] != boot_call or base[APRS_BOOT_SITE:APRS_BOOT_SITE + 4] != boot_call:
        raise ValueError("APRS state boot hook differs from stock")
    patched[APRS_BOOT_SITE:APRS_BOOT_SITE + 4] = call_bytes(symbol(stub_prog, "ml_aprs_boot") - IMAGE_BASE, APRS_BOOT_SITE)
    retired = None
    if disable_morse:
        sys.path.insert(0, str(ROOT))
        import morse_disable
        patched = morse_disable.apply(stock, patched)
        if resident:
            retired = next((start, end) for start, end, _ in morse_disable.REMOVED if start == 0x84F88)
    kiss, kiss_sectors = apply_dev_kiss(patched, CAVE + len(stub), retired) if dev_kiss else (None, set())
    raw = out / "h9_toolbox_modules_NOT_FOR_FLASH.raw"
    raw.write_bytes(patched)

    package = out / "h9_toolbox_modules_OFFLINE_EXPERIMENT.fw"
    if package.exists():
        package.unlink()
    run([sys.executable, str(TOOLS / "h9-firmware-research" / "firmware.py"), "repack", str(stock_package),
         "--app", str(raw), "--output", str(package)])
    sys.path.insert(0, str(RESEARCH / "h9-flasher"))
    from h9_flasher import parse_firmware  # noqa: E402
    original, target = parse_firmware(stock_package).image, parse_firmware(package).image
    changed = {o for o in range(0, len(original), 4096) if original[o:o + 4096] != target[o:o + 4096]}
    expected = H9TB_SECTORS | {(HEAP_END_SITE + 0x5100) & ~0xFFF, (SERIAL_WRITE_SITE + 0x5100) & ~0xFFF}
    expected |= kiss_sectors
    if aprs_mute:
        expected |= {(site + 0x5100) & ~0xFFF for site, _, _ in mute_sites}
    if disable_morse:
        for site, old, _ in morse_disable.patches(stock):
            first = (site + 0x5100) & ~0xFFF
            last = (site + 0x5100 + len(old) - 1) & ~0xFFF
            expected.update(range(first, last + 0x1000, 0x1000))
    if changed != expected:
        raise ValueError(f"changed sectors {sorted(map(hex, changed))}, expected {sorted(map(hex, expected))}")
    return {
        "app_sha256": sha256(patched).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target).hexdigest(),
        "changed_flash_sectors": [hex(c) for c in sorted(changed)],
        "stub_bytes": len(stub), "stub_sha256": sha256(stub).hexdigest(),
        "firmware_version": version_text,
        "stub_symbols": {n: hex(symbol(stub_prog, n)) for n in ("ml_hotkey", "ml_main", "ml_api", "ml_read")},
        "heap_end": hex(heap_end),
        "aprs_service_ram": {"start": hex(APRS_SERVICE_RAM), "end": hex(LOAD)},
        "serial_write": hex(serial_write),
        "resource_write_protection": {"start": "0x3fc000", "end_exclusive": "0x400000"},
        "morse_disabled": {"enabled": disable_morse, "menu_entry_hidden": disable_morse,
                           "code_bytes_removed": sum(end - start for start, end, _ in morse_disable.REMOVED)
                           if disable_morse else 0, "hardware_tested": False},
        "aprs_mute": {"enabled": aprs_mute,
                      "setting": "journal" if resident else "build-time", "hardware_tested": False},
        "resident_beacon": {"flash_offset": hex(RESIDENT_FLASH), "start_ram": hex(RESIDENT_RAM),
                            "body_bytes": len(resident), "crc16": f"{crc16(resident):04x}",
                            "sha256": sha256(resident).hexdigest(),
                            "screen_crc16": f"{resident_screen_crc:04x}", "auto_tx": False}
                           if resident else None,
        "dev_kiss": kiss,
    }


def build(stock_image: Path, stock_package: Path, toolchain: Path, out: Path,
          aprs_mute: bool = False, disable_morse: bool = False, dev_kiss: bool = False,
          resident_beacon: bool = False) -> dict:
    stock = stock_image.read_bytes()
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected the exact official v1.0.38 main application")
    if sha256(stock_package.read_bytes()).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the exact official v1.0.38 .fw package")
    out.mkdir(parents=True, exist_ok=True)
    resident = build_resident(out, toolchain) if resident_beacon else None
    modules = build_modules(out, toolchain, resident_beacon)
    screen_crc = crc16(next(m["body"] for m in modules if m["name"] == "APRS BEACON")) if resident else None
    firmware = build_firmware(stock, stock_package, out, aprs_mute, disable_morse, dev_kiss, resident, screen_crc)
    data = bundle(modules)
    parse_bundle(data)
    (out / "h9_modules.bin").write_bytes(data)
    # One .h9mod per app (a one-module bundle) for the App Manager's "Add module".
    (out / "modules").mkdir(exist_ok=True)
    for m in modules[1:]:
        single = bundle([m])
        parse_bundle(single)
        (out / "modules" / (m["name"].lstrip(".").lower().replace(" ", "_") + ".h9mod")).write_bytes(single)
    manifest = {
        "kind": "h9_toolbox_module_loader_experiment",
        "base": "H9 Toolbox Firmware app 1334a32c (Brick Breaker removed; S2 service kept)",
        "firmware": firmware,
        "bundle": {"slot_directory": hex(SLOT_DIRECTORY), "data": hex(SLOT_DATA), "bytes": len(data),
                   "sha256": sha256(data).hexdigest(),
                   "modules": [{"name": m["name"], "bytes": len(m["body"]), "entry": hex(m["entry"]),
                                "crc16": hex(crc16(m["body"]))} for m in modules]},
        "hardware_observation": "Not yet flashed",
    }
    (out / "h9_toolbox_modules-manifest.json").write_text(json.dumps(manifest, indent=1) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--stock-image", type=Path, required=True)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    parser.add_argument("--aprs-mute", action="store_true", help="Experimental normal-mode CH-B speaker gate")
    parser.add_argument("--disable-morse", action="store_true", help="Hide stock CW and block its keying and message paths")
    parser.add_argument("--dev-kiss", action="store_true", help="Include the KISS main firmware changes from dev")
    parser.add_argument("--resident-beacon", action="store_true",
                        help="Include the passive reboot-persistent resident scheduler and mute setting")
    args = parser.parse_args()
    if args.resident_beacon and not (args.aprs_mute and args.disable_morse and args.dev_kiss):
        parser.error("--resident-beacon needs --aprs-mute, --disable-morse and --dev-kiss")
    bundled = RESEARCH / "toolchain" / "jieli" / "jieli-linux-toolchains-20260730.1"
    toolchain = Path(os.environ.get("JIELI_TOOLCHAIN", bundled)).expanduser().resolve()
    if not (toolchain / "pi32v2").is_dir():
        raise SystemExit(f"No JieLi toolchain at {toolchain}: run sh firmware-research/toolchain/jieli/setup.sh")
    print(json.dumps(build(args.stock_image.resolve(), args.stock_package.resolve(), toolchain,
                           args.output.resolve(), args.aprs_mute, args.disable_morse,
                           args.dev_kiss, args.resident_beacon), indent=1))
    print("Offline build only; not hardware tested")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
