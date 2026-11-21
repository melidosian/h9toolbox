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
import re
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
STOCK_DISPATCH = 0x818F0
STOCK_DISPATCH_PROLOGUE = bytes.fromhex("7f04e292")
BACK_ACTION_CALL, STOCK_GNSS = 0x81FCA, 0x7E078
LOAD, SLOT_END = 0x29F00, 0x2BF00
APRS_BOOT_SITE = 0x88186
APRS_SERVICE_RAM = 0x29C00
RESIDENT_RAM = 0x29A00
CHANNEL_LABEL_RAM = 0x29900
CHANNEL_LABEL_SHIMS, CHANNEL_LABEL_SHIMS_END = 0x87C80, 0x87E38
# Pinned A/B Name + number row calls in all stock layouts.
CHANNEL_NAME_CALLS = ((0x6758E, 0x5A386, "ml_labels_draw_a"),
                      (0x67642, 0x5A44E, "ml_labels_draw_b"),
                      (0x5A59E, 0x5A386, "ml_labels_draw_a"),
                      (0x5A614, 0x5A44E, "ml_labels_draw_b"),
                      (0x59B24, 0x59908, "ml_labels_draw_classic"),
                      (0x59DEC, 0x59908, "ml_labels_draw_classic"),
                      (0x7AEAE, 0x59908, "ml_labels_draw_classic"),
                      (0x7AEDA, 0x59908, "ml_labels_draw_classic"),
                      (0x5A232, 0x5967C, "ml_labels_draw_tx_frequency"))
CHANNEL_CLONE_CALLS = ((0x5D018, 0xA51A8, "ml_labels_read"),
                       (0x5D122, 0xA5200, "ml_labels_write"))
CHANNEL_LABEL_EXPORTS = {
    "LABEL_FAR_TICK": "radio_label_display_tick", "LABEL_FAR_DRAW_A": "radio_label_draw_a",
    "LABEL_FAR_DRAW_B": "radio_label_draw_b", "LABEL_FAR_READ": "radio_label_clone_read",
    "LABEL_FAR_DRAW_CLASSIC": "radio_label_draw_classic",
    "LABEL_FAR_DRAW_TX_FREQUENCY": "radio_label_draw_tx_frequency",
    "LABEL_FAR_WRITE": "radio_label_clone_write",
}
RESIDENT_FLASH, RESIDENT_CPU = 0xFB000, 0x01EF6000
RESIDENT_TX_FLASH, RESIDENT_TX_CPU = 0xFA000, 0x01EF5000
RESIDENT_TX_HOOKS = 0x87B8C   # retired Morse code after the other dev helpers
# Normal my_app timer, native APRS key/payload/airtime, and asynchronous
# stock AF01 / external PTT entries to the same transmitter.
RESIDENT_TX_CALLS = (
    (0x87494, 0x82EB2, "ml_auto_timed"),
    (0x5C39C, 0x5AC68, "ml_auto_prepare"),
    (0x5C3A6, 0x5BE2C, "ml_auto_emit"),
    (0x5C3AC, 0x5C26C, "ml_auto_airtime"),
    (0x5DCDE, 0x5DACC, "ml_guarded_kiss"),
    (0x5DD0A, 0x5C44C, "ml_guarded_ptt"),
    (0x5E1BC, 0x5C44C, "ml_guarded_ptt"),
    (0x5DD10, 0x5CDF8, "ml_guarded_end"),
    (0x5E1C2, 0x5CDF8, "ml_guarded_end"),
)
# S2 control allowlist and pre-program erased-block check in the pinned bb-s2 base.
S2_WRITE_GUARDS = ((0xA52A4, 0xA542C, "ml_s2_allowed"),
                   (0xA535A, 0xA548E, "ml_s2_program_check"))
KISS_MANIFEST = RESEARCH / "kiss-tnc" / "main_kiss_patch.json"
KISS_MANIFEST_SHA256 = "2f9b8b2f155f762c2e4a5f13a8892429fcc0ab09dd755f2aa941e1abe1e58c74"
KISS_HELPER = 0xA7C7C
KISS_RESIDENT_HELPER = 0x84F88
KISS_CALLS = {0x5DB40: 0xA7CAA, 0x5DB6A: 0xA7CB6, 0x5DB86: 0xA7CE8, 0x870E0: 0xA7CF8}
FLASH_HELPERS = 0x85080   # after the relocated KISS helpers in the retired Morse code
FLASH_HELPERS_API = 5
FAR_MENU_FLASH, FAR_MENU_CPU = 0xEA000, 0x01EE5000
FAR_MENU_END = 0xFA000     # resident TX starts here in the dev build
# API 5 assigns one four-byte jump per export, in this order. Keep these
# addresses fixed for ABI 5 even when the implementation's layout changes.
FLASH_EXPORTS = (
    ("gfx_fill", 5), ("gfx_text", 7), ("gfx_wait_release", 0), ("gfx_wait_key", 0),
    ("h9_lcd_window", 4), ("h9_lcd_write", 2), ("h9_key", 0), ("h9_delay", 1),
    ("h9_led", 1), ("h9_usb_loader", 0), ("h9_reset", 0), ("memcpy", 3), ("memset", 3),
)
SLOT_DIRECTORY, SLOT_DATA, SLOT_LIMIT = 0xCC0000, 0xCC1000, 0xCE0000

BUNDLE_MAGIC, BUNDLE_VERSION = b"H9MODS01", 1
HEADER, ENTRY, NAME = 16, 32, 16
C_FLAGS = ["-target", "pi32v2", "-mcpu=r3", "-Oz", "-integrated-as", "-ffreestanding", "-fno-builtin",
           "-fno-common", "-ffunction-sections", "-Wall", "-Werror"]
# Extra compiler flags for test builds, e.g. H9_CFLAGS=-DAPRS_SPEAKER_TEST.
C_FLAGS += os.environ.get("H9_CFLAGS", "").split()


def firmware_version_word(version: str) -> int:
    """Keep the loader's three version bytes and use the high byte for RC."""
    match = re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-rc\.([1-9][0-9]*))?", version)
    if not match:
        raise ValueError(f"Invalid firmware version {version!r}")
    major, minor, patch, rc = (int(part or 0) for part in match.groups())
    if max(major, minor, patch, rc) >= 256:
        raise ValueError(f"Firmware version components must fit in one byte: {version!r}")
    return rc << 24 | major << 16 | minor << 8 | patch


def crc16(data: bytes) -> int:
    """CRC-16/XMODEM, as the stub, the S2 service and the uploader compute it."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


MENU_METADATA = b"H9MENU01"
MENU_CATEGORIES = ("", "apps", "games", "tools", "diagnostics", "setup", "aprs")

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
    categories = bytes(MENU_CATEGORIES.index(m.get("category", "")) for m in modules)
    metadata = MENU_METADATA + categories if any(categories) else b""
    offset += len(metadata)
    data = BUNDLE_MAGIC + struct.pack("<HHI", BUNDLE_VERSION, len(modules), offset) + entries + bodies + metadata
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
    trailer = length - len(MENU_METADATA) - count
    has_metadata = trailer >= HEADER + ENTRY * count and data[trailer:trailer + len(MENU_METADATA)] == MENU_METADATA
    for i in range(count):
        e = data[HEADER + ENTRY * i:HEADER + ENTRY * (i + 1)]
        offset, size, bss, entry, crc = struct.unpack_from("<IIIHH", e, NAME)
        body = data[offset:offset + size]
        if not (0 < size and size + bss <= SLOT_END - LOAD and entry < size and offset + size <= len(data)):
            raise ValueError(f"bad entry {i}")
        if crc16(body) != crc:
            raise ValueError(f"CRC mismatch in entry {i}")
        category = data[trailer + len(MENU_METADATA) + i] if has_metadata else 0
        if category >= len(MENU_CATEGORIES) or (has_metadata and offset + size > trailer):
            raise ValueError(f"bad menu metadata in entry {i}")
        out.append({"name": e[:NAME].rstrip(b"\0").decode("ascii"), "offset": offset, "size": size, "bss": bss,
                    "entry": entry, "body": body, **({"category": MENU_CATEGORIES[category]} if category else {})})
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


def link(objs: list[Path], origin: int, entry: str, out: Path, name: str,
         *, keep: tuple[str, ...] = ()) -> elf.Program:
    script = out / f"{name}.ld"
    script.write_text('OUTPUT_FORMAT("elf32-pi32v2")\nOUTPUT_ARCH(pi32v2)\n'
                      f"ENTRY({entry})\nSECTIONS {{\n    . = 0x{origin:08X};\n"
                      "    .text : { *(.text .text.*) *(.rodata .rodata.*) }\n"
                      "    /DISCARD/ : { *(.comment) *(.note*) }\n}\n")
    path = out / f"{name}.elf"
    state, data = ld.link(ld.parse_args(["--gc-sections", *(f"--undefined={name}" for name in keep),
                                         "-T", str(script), "-o", str(path), *map(str, objs)]))
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


def compile_c(source: Path, out: Path, toolchain: Path, *, flags: tuple[str, ...] = ()) -> Path:
    obj = out / (source.stem + ".o")
    rel = lambda p: "/w/" + p.relative_to(RESEARCH).as_posix()
    run(["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{toolchain}:/opt/jieli:ro",
         "-v", f"{RESEARCH}:/w", "-w", "/w", "h9-jieli", "/opt/jieli/pi32v2/bin/clang", *C_FLAGS, *flags,
         # vendored third-party code is used unchanged: its `(T){0}` initialiser trips -Wmissing-braces
         *(["-Wno-missing-braces"] if "third_party" in source.parts else []),
         "-I", rel(ROOT / "modules" / "common"), "-c", rel(source), "-o", rel(obj)])
    return obj


def build_flash_helpers(lib: list[Path], out: Path) -> tuple[bytes, Path]:
    origin = IMAGE_BASE + FLASH_HELPERS
    jumps = out / "flash_helper_table.s"
    jumps.write_text('    .section .text,"ax",@progbits\n' +
                     "".join(f"    .globl jump_{name}\njump_{name}:\n    goto {name}\n"
                             for name, _ in FLASH_EXPORTS))
    entry = "jump_" + FLASH_EXPORTS[0][0]
    helpers = link([assemble(jumps, out), *lib], origin, entry, out, "flash_helpers")
    body = text_of(helpers, origin)
    if any(s.length and s.flags & 2 and s.name != ".text" for s in helpers.sections):
        raise ValueError("Flash helpers need an executable body without other loaded sections")

    calls = out / "module_flash_calls.s"
    source = [f".equ MODULE_FLASH_API, {FLASH_HELPERS_API}\n",
              (ROOT / "src" / "flash_module.s").read_text(), "\n"]
    for index, (name, arguments) in enumerate(FLASH_EXPORTS):
        target = origin + 4 * index
        if symbol(helpers, "jump_" + name) != target:
            raise ValueError(f"Flash entry for {name} moved from {target:#x}")
        bridge = "flash_call_r3" if arguments >= 4 else "flash_call"
        source.append(f"    {bridge} {name}, {target:#x}\n")
    calls.write_text("".join(source))
    return body, assemble(calls, out)


def far_menu_trampoline(out: Path) -> bytes:
    jump = out / "far_menu_jump.s"
    jump.write_text('    .section .text,"ax",@progbits\n'
                    '    .globl module_flash_entry\nmodule_flash_entry:\n'
                    f'    r3 = {FAR_MENU_CPU:#x}\n    goto r3\n')
    program = link([assemble(jump, out)], LOAD, "module_flash_entry", out, "far_menu_jump")
    return text_of(program, LOAD)


def build_modules(out: Path, toolchain: Path, resident: bool = False, *,
                  flash_shared: bool = False, auto_tx: bool = False,
                  far_menu: bool = False, radio_menu_preview: bool = False,
                  unified_menu: bool = False) -> tuple[list[dict], bytes | None, bytes | None]:
    if far_menu and not flash_shared:
        raise ValueError("The far menu needs the stable API 5 flash helpers")
    if radio_menu_preview and not far_menu:
        raise ValueError("The radio menu preview needs far-flash placement")
    if unified_menu and (not far_menu or radio_menu_preview):
        raise ValueError("Unified menu requires far flash and excludes the label preview")
    common = ROOT / "modules" / "common"
    run([sys.executable, "-I", str(common / "font5x7.py")])
    lib = [assemble(common / "h9lib.s", out), compile_c(common / "gfx.c", out, toolchain)]
    entry = "module_main"
    flash = None
    flags = ("-DH9_RESIDENT_TX",) if auto_tx else ()
    if flash_shared:
        flash, calls = build_flash_helpers(lib, out)
        lib = [calls]
        entry = "module_flash_entry"
        flags += ("-DH9_FLASH_HELPERS",)

    def c_module(name: str, source: Path, *extra: Path, extra_flags: tuple[str, ...] = (),
                 category: str = "") -> dict:
        objs = [compile_c(src, out, toolchain, flags=flags + extra_flags) for src in (source, *extra)]
        prog = link([*objs, *lib], LOAD, entry, out, "mod_" + source.stem)
        return {"name": name, "body": text_of(prog, LOAD), "bss": 0, "entry": prog.entry - LOAD,
                **({"category": category} if category else {})}

    menu_dir = ROOT / "modules" / "menu"
    aprs_dir = ROOT / "modules" / "aprs"
    menu_sources = (common / "vfo_backup.c",)
    if flash_shared:
        menu_sources += (menu_dir / "toolbox_menu.c", aprs_dir / "aprs_resident_config.c",
                         common / "toolbox_settings_pair.c")
    far_menu_body = None
    if far_menu:
        if unified_menu:
            radio_dir = ROOT / "menu-replacement"
            run(["node", str(radio_dir / "generate_settings.mjs")])
            run(["node", str(radio_dir / "generate_channel_choices.mjs")])
            menu_sources += tuple(radio_dir / name for name in (
                "radio_settings.c", "radio_storage.c", "radio_editor.c", "radio_channels.c", "radio_channel_editor.c",
                "radio_actions.c", "radio_actions_catalog.c", "radio_bluetooth.c", "radio_scan.c",
                "radio_system.c", "radio_system_native.c", "radio_aprs.c", "channel_labels.c",
                "channel_labels_native.c", "channel_name_scroll.c", "channel_name_view.c", "radio_quick.c"))
        sources = (menu_dir / "menu.c", *menu_sources)
        menu_flags = flags + ("-DH9_GROUPED_MENU",)
        if radio_menu_preview:
            menu_flags += ("-DH9_RADIO_MENU_PREVIEW",)
        if unified_menu:
            menu_flags += ("-DH9_RADIO_SETTINGS",)
        objects = [compile_c(src, out, toolchain, flags=menu_flags) for src in sources]
        if unified_menu:
            objects.extend(assemble(radio_dir / name, out) for name in (
                "radio_io.s", "radio_bluetooth_io.s", "radio_scan_io.s", "radio_system_io.s"))
        # The RAM trampoline targets the first far byte. Put the API gate
        # before the C objects and reject any linker layout that moves it.
        program = link([*lib, *objects], FAR_MENU_CPU, entry, out, "far_menu",
                       keep=(*CHANNEL_LABEL_EXPORTS.values(), "radio_quick_menu") if unified_menu else ())
        if program.entry != FAR_MENU_CPU:
            raise ValueError("Far menu API gate is not at the trampoline target")
        far_menu_body = text_of(program, FAR_MENU_CPU)
        if len(far_menu_body) > FAR_MENU_END - FAR_MENU_FLASH:
            raise ValueError("Unified menu exceeds the reserved far-flash window")
        menu = {"name": "MENU", "body": far_menu_trampoline(out), "bss": 0, "entry": 0}
    else:
        menu = c_module("MENU", menu_dir / "menu.c", *menu_sources,
                        extra_flags=("-DH9_GROUPED_MENU",) if flash_shared else ())
    if len(menu["body"]) > 0x2B880 - LOAD:
        raise SystemExit("MENU code overlaps the shared satellite state")
    about = c_module("ABOUT", ROOT / "modules" / "about" / "about.c")
    maze = c_module("MAZE CHASE", ROOT / "modules" / "maze" / "maze.c", ROOT / "modules" / "common" / "snd.c",
                    category="games")
    aprs = c_module("APRS MONITOR", ROOT / "modules" / "aprs_monitor" / "aprs_monitor.c")
    if len(aprs["body"]) > 0x2000 - 0x800:
        raise SystemExit("APRS MONITOR code overlaps its state in the top 2 KiB of the RAM slot")
    # APRS screens share state at 0x2BEE0 - 0x5B0 (aprs_core.h APRS_ADDR); each
    # screen's code must end below it. ".APRS INFO" is hidden from the menu.
    aprs_limit = 0x2BEE0 - 0x5B0 - LOAD            # aprs_core.h APRS_SLOT_BYTES
    core = aprs_dir / "aprs_core.c"
    text = aprs_dir / "aprs_text.c"
    screens = [c_module("APRS", aprs_dir / "aprs_load.c", core, aprs_dir / "aprs_stored.c"),
               c_module(".APRS LIST", aprs_dir / "aprs_list.c", core),
               c_module(".APRS INFO", aprs_dir / "aprs_info.c", core, text),
               c_module("APRS MAP", aprs_dir / "aprs_map.c", core),
               c_module("APRS MSG", aprs_dir / "aprs_msg.c", core, text),
               c_module("APRS SEND", aprs_dir / "aprs_send.c", aprs_dir / "aprs_core_tx.c", aprs_dir / "aprs_tx.c",
                        aprs_dir / "aprs_edit.c", text),
               c_module(".APRS STORE", aprs_dir / "aprs_mail.c", aprs_dir / "aprs_core_tx.c",
                        common / "mail_pair.c", common / "toolbox_settings_pair.c")]
    if flash_shared:
        status_sources = (aprs_dir / "aprs_tx.c",) if auto_tx else ()
        screens.append(c_module("APRS BEACON", aprs_dir / "aprs_resident_status.c", *status_sources))
    elif resident:
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
    # the original cave), then relink it for RAM. Its entry re-runs the panel
    # set-up, as the original launcher did.
    game_source = RESEARCH / "brick-breaker" / "src" / "game.s"
    game = assemble(game_source, out)
    original = text_of(link([game], IMAGE_BASE + CAVE, "hello_display_hook", out, "bb_cave_check"),
                       IMAGE_BASE + CAVE)
    if sha256(original).hexdigest() != BB_GAME_SHA256:
        raise ValueError("game.s differs from the hardware-tested Brick Breaker game")
    if flash_shared:
        # The LCD receives the high RGB565 byte first.
        accent = next(line.split()[2] for line in (common / "app_theme.h").read_text().splitlines()
                      if line.split()[:2] == ["#define", "UI_ACCENT"])
        ink = int.from_bytes(int(accent.rstrip("uU"), 0).to_bytes(2, "big"), "little")
        themed = out / "game_themed.s"
        themed.write_text(f".equ TOOLBOX_SCORE_INK, {ink:#06x}\n" + game_source.read_text())
        game = assemble(themed, out)
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
    return [menu, bb, *screens, aprs, satpass, satplot, sattrack, satauto, satdata, gps, vfo_probe, maze, about], flash, far_menu_body


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
    # Git may check this text file out with CRLF on Windows. Pin the repository
    # content, whose SHA-256 is over LF bytes, without weakening the patch check.
    if sha256(raw.replace(b"\r\n", b"\n")).hexdigest() != KISS_MANIFEST_SHA256:
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


def build_resident(out: Path, toolchain: Path, *, auto_tx: bool = False) -> bytes:
    service = ROOT / "resident-service"
    aprs = ROOT / "modules" / "aprs"
    common = ROOT / "modules" / "common"
    sources = (service / "runtime.c", service / "modem_input.c", service / "beacon_service.c",
               aprs / "beacon_core.c", aprs / "third_party" / "smartbeacon" / "smartbeacon.c",
               common / "toolbox_settings_pair.c", common / "gfx.c")  # compiler's freestanding memcpy
    flags = ()
    if auto_tx:
        sources += (service / "beacon_tx.c", aprs / "aprs_tx.c")
        flags = ("-DH9_RESIDENT_TX",)
    objects = [assemble(service / "entry.s", out)]
    objects += [compile_c(src, out, toolchain, flags=flags) for src in sources]
    origin = RESIDENT_TX_CPU if auto_tx else RESIDENT_CPU
    limit = 0x2000 if auto_tx else 0x1000
    program = link(objects, origin, "resident_entry", out, "resident_beacon")
    body = text_of(program, origin)
    extra = [s.name for s in program.sections if s.flags & 2 and s.length and s.name != ".text"]
    if program.entry != origin or len(body) > limit or extra:
        raise ValueError(f"Resident service must fit its {limit}-byte executable window ({len(body)} B, {extra})")
    return body


def apply_resident_tx(app: bytearray, stock: bytes, resident: bytes, out: Path, retired_end: int) -> dict:
    program = elf.read((out / "resident_beacon.elf").read_bytes())
    api = symbol(program, "api")
    if not RESIDENT_TX_CPU <= api <= RESIDENT_TX_CPU + len(resident) - 32:
        raise ValueError("TX API is outside the verified resident body")
    if struct.unpack_from("<I", resident, api - RESIDENT_TX_CPU) != (2,):
        raise ValueError("Main-flash TX hooks require resident ABI 2")
    source = out / "resident_tx_hooks.s"
    source.write_text(f".equ RESIDENT_TX_API, {api:#x}\n" + (ROOT / "src/resident_tx_hooks.s").read_text())
    origin = IMAGE_BASE + RESIDENT_TX_HOOKS
    shim = link([assemble(source, out)], origin, "ml_auto_timed", out, "resident_tx_hooks")
    code = text_of(shim, origin)
    end = RESIDENT_TX_HOOKS + len(code)
    if end > retired_end or app[RESIDENT_TX_HOOKS:end] != bytes(len(code)):
        raise ValueError("TX shims overlap existing instructions")
    for site, original, name in RESIDENT_TX_CALLS:
        expected = call_bytes(original, site)
        if app[site:site + 4] != expected or stock[site:site + 4] != expected:
            raise ValueError(f"Stock TX hook at {site:#x} differs from the pinned call")
        app[site:site + 4] = call_bytes(symbol(shim, name) - IMAGE_BASE, site)
    app[RESIDENT_TX_HOOKS:end] = code
    return {"api": hex(api), "shims_app_offset": hex(RESIDENT_TX_HOOKS),
            "shims_bytes": len(code), "call_sites": [hex(site) for site, _, _ in RESIDENT_TX_CALLS]}


def apply_channel_labels(app: bytearray, stock: bytes, base: bytes, far: bytes, crc_entry: int, out: Path) -> dict:
    far_program = elf.read((out / "far_menu.elf").read_bytes())
    addresses = {key: symbol(far_program, entry) for key, entry in CHANNEL_LABEL_EXPORTS.items()}
    if any(not FAR_MENU_CPU <= address < FAR_MENU_CPU + len(far) for address in addresses.values()):
        raise ValueError("Channel label exports exceed the verified far-menu body")
    constants = {**addresses, "LABEL_FAR_BASE": FAR_MENU_CPU, "LABEL_FAR_LENGTH": len(far),
                 "LABEL_FAR_CRC": crc16(far), "LABEL_CRC_HELPER": crc_entry}
    source = out / "channel_label_hooks.s"
    source.write_text("".join(f".equ {name}, {value:#x}\n" for name, value in constants.items())
                      + (ROOT / "src/channel_label_hooks.s").read_text())
    origin = IMAGE_BASE + CHANNEL_LABEL_SHIMS
    shim = link([assemble(source, out)], origin, "ml_labels_tick", out, "channel_label_hooks")
    code = text_of(shim, origin)
    end = CHANNEL_LABEL_SHIMS + len(code)
    if shim.entry != origin or end > CHANNEL_LABEL_SHIMS_END or app[CHANNEL_LABEL_SHIMS:end] != bytes(len(code)):
        raise ValueError("Channel label hooks overlap existing instructions")
    for image, hooks in ((stock, CHANNEL_NAME_CALLS), (base, CHANNEL_CLONE_CALLS)):
        for site, original, name in hooks:
            expected = call_bytes(original, site)
            if image[site:site + 4] != expected or app[site:site + 4] != expected:
                raise ValueError(f"Channel label hook at {site:#x} differs from the pinned call")
            app[site:site + 4] = call_bytes(symbol(shim, name) - IMAGE_BASE, site)
    app[CHANNEL_LABEL_SHIMS:end] = code
    return {"display_calls": [hex(site) for site, _, _ in CHANNEL_NAME_CALLS],
            "clone_calls": [hex(site) for site, _, _ in CHANNEL_CLONE_CALLS],
            "shims_app_offset": hex(CHANNEL_LABEL_SHIMS), "shims_bytes": len(code),
            "storage_start": "0x3f6000", "storage_end_exclusive": "0x3fa000",
            "heap_end": hex(CHANNEL_LABEL_RAM)}


def build_firmware(stock: bytes, stock_package: Path, out: Path, aprs_mute: bool = False,
                   disable_morse: bool = False, dev_kiss: bool = False,
                   resident: bytes | None = None, resident_screen_crc: int | None = None, *,
                   flash: bytes | None = None, auto_tx: bool = False,
                   unified_menu: bool = False, far_menu_body: bytes | None = None) -> dict:
    if resident and not (aprs_mute and disable_morse and dev_kiss):
        raise ValueError("Resident APRS requires --aprs-mute, --disable-morse and --dev-kiss")
    if resident and resident_screen_crc is None:
        raise ValueError("Resident APRS requires the CRC of its matching screen")
    if flash is not None and not resident:
        raise ValueError("Flash helpers require the resident dev build")
    if auto_tx and (not resident or flash is None):
        raise ValueError("Resident TX requires the shared-flash dev build")
    api_version = FLASH_HELPERS_API if flash is not None else 4 if resident else 3
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
        if unified_menu:
            if not far_menu_body:
                raise ValueError("Unified loader requires its far body")
            far_program = elf.read((out / "far_menu.elf").read_bytes())
            quick_entry = symbol(far_program, "radio_quick_menu")
            if not FAR_MENU_CPU <= quick_entry < FAR_MENU_CPU + len(far_menu_body):
                raise ValueError("Quick Change entry exceeds the verified far-menu body")
            definitions += (f".equ ML_QUICK_ENTRY, {quick_entry:#x}\n"
                            f".equ ML_STOCK_GNSS, {IMAGE_BASE + STOCK_GNSS:#x}\n"
                            f".equ ML_STOCK_DISPATCH, {IMAGE_BASE + STOCK_DISPATCH + len(STOCK_DISPATCH_PROLOGUE):#x}\n"
                            f".equ ML_LABELS_LOOP, {IMAGE_BASE + CHANNEL_LABEL_SHIMS:#x}\n"
                            f".equ ML_UNIFIED_MENU, 1\n.equ ML_UNIFIED_CPU, {FAR_MENU_CPU:#x}\n"
                            f".equ ML_UNIFIED_BYTES, {len(far_menu_body)}\n.equ ML_UNIFIED_CRC, {crc16(far_menu_body)}\n")
        if resident:
            definitions += (f".equ ML_RUNTIME_BOOT, {IMAGE_BASE + OLD_LAUNCHER:#x}\n"
                           f".equ ML_RESIDENT_SCREEN_CRC, {resident_screen_crc}\n"
                           f".equ ML_API_VERSION, {api_version}\n")
        combined.write_text(definitions + stub_source.read_text() + "\n" + (ROOT / "src" / "aprs_mute.s").read_text())
        stub_source = combined
    stub_prog = link([assemble(stub_source, out)], IMAGE_BASE + CAVE, "ml_hotkey", out, "stub")
    stub = bytearray(text_of(stub_prog, IMAGE_BASE + CAVE))
    version_text = (ROOT / "VERSION").read_text().strip()
    version_word = firmware_version_word(version_text)
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
    resident_flash = RESIDENT_TX_FLASH if auto_tx else RESIDENT_FLASH
    resident_cpu = RESIDENT_TX_CPU if auto_tx else RESIDENT_CPU
    resident_file = f"resident_0x{resident_flash:X}_OFFLINE.bin"
    if resident:
        if len(resident) > (0x2000 if auto_tx else 0x1000):
            raise ValueError("Resident blob exceeds its flash window")
        boot = out / "runtime_boot.s"
        boot.write_text(f".equ RUNTIME_ENTRY, {resident_cpu:#x}\n"
                        f".equ RUNTIME_LENGTH, {len(resident)}\n"
                        f".equ RUNTIME_CRC, {crc16(resident)}\n"
                        f".equ LOADER_CRC, {symbol(stub_prog, 'ml_crc'):#x}\n"
                        + (ROOT / "src" / "runtime_boot.s").read_text())
        addr = IMAGE_BASE + OLD_LAUNCHER
        verifier = text_of(link([assemble(boot, out)], addr, "ml_runtime_boot", out, "runtime_boot"), addr)
        if len(verifier) > SVC - OLD_LAUNCHER:
            raise ValueError("Resident verifier would overlap the S2 service")
        patched[OLD_LAUNCHER:OLD_LAUNCHER + len(verifier)] = verifier
        (out / resident_file).write_bytes(resident)
    patched[CAVE:CAVE + len(stub)] = stub
    api_at = symbol(stub_prog, "ml_api") - IMAGE_BASE
    if (struct.unpack_from("<4sI", patched, api_at) != (b"H9AP", api_version)
            or struct.unpack_from("<I", patched, api_at + 24)[0] != version_word):
        raise ValueError("the image's API table disagrees with the build")
    patched[HOOK:HOOK + 4] = call_bytes(CAVE, HOOK)
    heap_end = CHANNEL_LABEL_RAM if unified_menu else RESIDENT_RAM if resident else APRS_SERVICE_RAM
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
            retired = next((KISS_RESIDENT_HELPER, end) for start, end, _ in morse_disable.REMOVED
                           if start <= KISS_RESIDENT_HELPER < end)
    kiss, kiss_sectors = apply_dev_kiss(patched, CAVE + len(stub), retired) if dev_kiss else (None, set())
    if flash is not None:
        end = FLASH_HELPERS + len(flash)
        kiss_end = int(kiss["helper_app_offset"], 16) + kiss["helper_bytes"]
        if not kiss_end <= FLASH_HELPERS < end <= retired[1]:
            raise ValueError("Flash helpers must fit after KISS in the retired Morse range")
        if patched[FLASH_HELPERS:end] != bytes(len(flash)):
            raise ValueError("Flash helpers overlap existing instructions")
        patched[FLASH_HELPERS:end] = flash
    tx_hooks = None
    if auto_tx:
        tx_limit = next(end for start, end, _ in morse_disable.REMOVED if start == RESIDENT_TX_HOOKS)
        tx_hooks = apply_resident_tx(patched, stock, resident, out, tx_limit)
    menu_patch = label_patch = None
    if unified_menu:
        if not far_menu_body:
            raise ValueError("Unified menu needs its matching far body")
        label_patch = apply_channel_labels(patched, stock, base, far_menu_body, symbol(stub_prog, "ml_crc"), out)
        sys.path.insert(0, str(ROOT / "menu-replacement"))
        import radio_menu_patch
        def raw_assembly(source):
            return bytes(next(s for s in elf.read(asm.assemble_source(source)).sections if s.name == ".text").data)
        site = STOCK_DISPATCH
        end = site + len(STOCK_DISPATCH_PROLOGUE)
        if stock[site:end] != STOCK_DISPATCH_PROLOGUE or patched[site:end] != STOCK_DISPATCH_PROLOGUE:
            raise ValueError("Stock keypad dispatch prologue differs from the pinned bytes")
        quick_dispatch = symbol(stub_prog, "ml_quick_dispatch") - IMAGE_BASE
        redirect = raw_assembly(f"goto {quick_dispatch - end}\n")
        if len(redirect) != len(STOCK_DISPATCH_PROLOGUE):
            raise ValueError("Quick Change redirect must replace exactly the stock prologue")
        patched[site:end] = redirect
        stock_back = call_bytes(STOCK_GNSS, BACK_ACTION_CALL)
        if stock[BACK_ACTION_CALL:BACK_ACTION_CALL + 4] != stock_back or patched[BACK_ACTION_CALL:BACK_ACTION_CALL + 4] != stock_back:
            raise ValueError("Stock Back action call differs from pinned call")
        quick_back = symbol(stub_prog, "ml_quick_back") - IMAGE_BASE
        patched[BACK_ACTION_CALL:BACK_ACTION_CALL + 4] = call_bytes(quick_back, BACK_ACTION_CALL)

        menu_patch = radio_menu_patch.apply(stock, patched, symbol(stub_prog, "ml_main"),
                                           symbol(stub_prog, "ml_crc"), FAR_MENU_CPU,
                                           len(far_menu_body), crc16(far_menu_body), raw_assembly,
                                           preceding_patches=morse_disable.patches(stock) if disable_morse else ())
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
    if auto_tx:
        expected |= {(site + 0x5100) & ~0xFFF for site, _, _ in RESIDENT_TX_CALLS}
    if menu_patch:
        expected |= {(site + 0x5100) & ~0xFFF for site in (STOCK_DISPATCH, BACK_ACTION_CALL)}
        for _, start, end in radio_menu_patch.REMOVED_UI:
            expected.update(range((start + 0x5100) & ~0xFFF, ((end - 1 + 0x5100) & ~0xFFF) + 0x1000, 0x1000))
    if label_patch:
        expected |= {(site + 0x5100) & ~0xFFF for site, _, _ in CHANNEL_NAME_CALLS + CHANNEL_CLONE_CALLS}
    if changed != expected:
        raise ValueError(f"changed sectors {sorted(map(hex, changed))}, expected {sorted(map(hex, expected))}")
    return {
        "app_sha256": sha256(patched).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target).hexdigest(),
        "changed_flash_sectors": [hex(c) for c in sorted(changed)],
        "stub_bytes": len(stub), "stub_sha256": sha256(stub).hexdigest(),
        "firmware_version": version_text,
        "loader_api": api_version,
        "stub_symbols": {n: hex(symbol(stub_prog, n)) for n in ("ml_hotkey", "ml_main", "ml_api", "ml_read")},
        "heap_end": hex(heap_end),
        "aprs_service_ram": {"start": hex(APRS_SERVICE_RAM), "end": hex(LOAD)},
        "serial_write": hex(serial_write),
        "resource_write_protection": {"start": "0x3f6000", "end_exclusive": "0x400000"},
        "morse_disabled": {"enabled": disable_morse, "menu_entry_hidden": disable_morse,
                           "code_bytes_removed": sum(end - start for start, end, _ in morse_disable.REMOVED)
                           if disable_morse else 0, "hardware_tested": False},
        "aprs_mute": {"enabled": aprs_mute,
                      "setting": "journal" if resident else "build-time", "hardware_tested": False},
        "resident_beacon": {"flash_offset": hex(resident_flash), "start_ram": hex(RESIDENT_RAM),
                            "body_bytes": len(resident), "crc16": f"{crc16(resident):04x}",
                            "sha256": sha256(resident).hexdigest(),
                            "screen_crc16": f"{resident_screen_crc:04x}", "auto_tx": auto_tx}
                           if resident else None,
        "resident_tx_hooks": tx_hooks,
        "unified_menu": menu_patch,
        "channel_labels": label_patch,
        "dev_kiss": kiss,
        "flash_helpers": {"app_offset": hex(FLASH_HELPERS), "bytes": len(flash),
                          "sha256": sha256(flash).hexdigest(), "exports": [name for name, _ in FLASH_EXPORTS]}
                         if flash is not None else None,
    }


def build(stock_image: Path, stock_package: Path, toolchain: Path, out: Path,
          aprs_mute: bool = False, disable_morse: bool = False, dev_kiss: bool = False,
          resident_beacon: bool = False, *, flash_shared: bool = False, auto_tx: bool = False,
          far_menu: bool = False, radio_menu_preview: bool = False,
          unified_menu: bool = False) -> dict:
    stock = stock_image.read_bytes()
    if sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected the exact official v1.0.38 main application")
    if sha256(stock_package.read_bytes()).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the exact official v1.0.38 .fw package")
    if flash_shared and not resident_beacon:
        raise ValueError("--flash-shared needs --resident-beacon")
    if far_menu and not flash_shared:
        raise ValueError("--far-menu needs --flash-shared")
    if radio_menu_preview and not far_menu:
        raise ValueError("--radio-menu-preview needs --far-menu")
    if unified_menu and not (aprs_mute and disable_morse):
        raise ValueError("Unified menu uses the dev feature set: --aprs-mute --disable-morse")
    if auto_tx and not (resident_beacon and flash_shared and aprs_mute and disable_morse and dev_kiss):
        raise ValueError("--resident-tx needs the full shared-flash dev build")
    out.mkdir(parents=True, exist_ok=True)
    if unified_menu:
        run([sys.executable, "-I", str(ROOT / "menu-replacement" / "generate_manuals.py"), str(stock_image)])
    resident = build_resident(out, toolchain, auto_tx=auto_tx) if resident_beacon else None
    modules, flash, far_menu_body = build_modules(out, toolchain, resident_beacon,
                                                 flash_shared=flash_shared, auto_tx=auto_tx,
                                                 far_menu=far_menu, radio_menu_preview=radio_menu_preview,
                                                 unified_menu=unified_menu)
    screen_crc = crc16(next(m["body"] for m in modules if m["name"] == "APRS BEACON")) if resident else None
    firmware = build_firmware(stock, stock_package, out, aprs_mute, disable_morse, dev_kiss, resident, screen_crc,
                              flash=flash, auto_tx=auto_tx, unified_menu=unified_menu,
                              far_menu_body=far_menu_body)
    data = bundle(modules)
    parse_bundle(data)
    coverage_report = None
    if unified_menu:
        sys.path.insert(0, str(ROOT / "menu-replacement"))
        import coverage as menu_coverage
        coverage_report = menu_coverage.validate(module_ids={m["name"] for m in modules})
        (out / "native-menu-coverage.json").write_text(json.dumps(coverage_report, indent=2) + "\n")
    (out / "h9_modules.bin").write_bytes(data)
    if far_menu_body is not None:
        (out / "menu_0xEA000_OFFLINE.bin").write_bytes(far_menu_body)
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
    if far_menu_body is not None:
        manifest["far_menu"] = {"flash_offset": hex(FAR_MENU_FLASH), "cpu_address": hex(FAR_MENU_CPU),
                                "bytes": len(far_menu_body), "sha256": sha256(far_menu_body).hexdigest(),
                                "required_with_bundle": True, "hardware_tested": False}
    manifest["radio_menu_preview"] = radio_menu_preview
    if coverage_report:
        manifest["native_menu_coverage"] = {"active_rows": coverage_report["active_rows"],
                                           "implemented_rows": coverage_report["implemented_rows"],
                                           "retired_rf_sms_rows": len(coverage_report["retired_rf_sms_rows"]),
                                           "report": "native-menu-coverage.json", "hardware_tested": False}
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
                        help="Include the reboot-persistent resident scheduler and mute setting")
    parser.add_argument("--flash-shared", action="store_true",
                        help="Put common app routines in the retired Morse space and build the matching API 5 bundle")
    parser.add_argument("--resident-tx", action="store_true",
                        help="Enable resident beacon TX at the stock timed-beacon boundary")
    parser.add_argument("--far-menu", action="store_true",
                        help="Build the grouped menu in far flash with an 8-byte RAM trampoline")
    parser.add_argument("--radio-menu-preview", action="store_true",
                        help="Show stock radio labels in the grouped menu without setting handlers; offline only")
    parser.add_argument("--unified-menu", action="store_true",
                        help="Replace native Menu entry and include saved radio setting editors")
    args = parser.parse_args()
    if args.resident_beacon and not (args.aprs_mute and args.disable_morse and args.dev_kiss):
        parser.error("--resident-beacon needs --aprs-mute, --disable-morse and --dev-kiss")
    if args.flash_shared and not args.resident_beacon:
        parser.error("--flash-shared needs --resident-beacon")
    if args.resident_tx and not (args.resident_beacon and args.flash_shared):
        parser.error("--resident-tx needs --resident-beacon and --flash-shared")
    if args.far_menu and not args.flash_shared:
        parser.error("--far-menu needs --flash-shared")
    if args.radio_menu_preview and not args.far_menu:
        parser.error("--radio-menu-preview needs --far-menu")
    if args.unified_menu and (not args.far_menu or args.radio_menu_preview):
        parser.error("--unified-menu needs --far-menu and excludes --radio-menu-preview")
    bundled = RESEARCH / "toolchain" / "jieli" / "jieli-linux-toolchains-20260730.1"
    toolchain = Path(os.environ.get("JIELI_TOOLCHAIN", bundled)).expanduser().resolve()
    if not (toolchain / "pi32v2").is_dir():
        raise SystemExit(f"No JieLi toolchain at {toolchain}: run sh firmware-research/toolchain/jieli/setup.sh")
    print(json.dumps(build(args.stock_image.resolve(), args.stock_package.resolve(), toolchain,
                           args.output.resolve(), args.aprs_mute, args.disable_morse,
                           args.dev_kiss, args.resident_beacon, flash_shared=args.flash_shared,
                           auto_tx=args.resident_tx, far_menu=args.far_menu,
                           radio_menu_preview=args.radio_menu_preview,
                           unified_menu=args.unified_menu), indent=1))
    print("Offline build only; not hardware tested")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
