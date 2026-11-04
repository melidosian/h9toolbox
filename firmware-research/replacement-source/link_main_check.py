#!/usr/bin/env python3
"""Whole-archive Pi32v2 link check with first-pass stock entry reconstruction.

The board entry stops after the post-clock descriptor and MMIO gate stage.
Optional raw output is for offline inspection only.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import re
import shutil
import struct
import sys

from build_target_objects import ROOT, build, find_compiler, run


IMAGE_BASE = 0x01E00100
STOCK_IMAGE_SIZE = 0xD3C2C
STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
RAM_START = 0xE2E0
RAM_END = 0x1E4D4
STOCK_BOOT_ARGS = bytes.fromhex(
    "50 ec 04 20 c1 ff a0 e2 00 00 50 ec 11 21 02 60 "
    "23 62 93 66 2a 4d 9a 40 0a 66 9a 6e 0a 67 9a 6f "
    "50 ec 00 21 50 ec 11 22 80 00"
)
STOCK_ICFG_AFTER_RAM_COPY = bytes.fromhex("64 e0 00 2b 72 e1 40 2f 64 e0 80 2b")
STOCK_POST_CLOCK_BITMAP_BRANCH = bytes.fromhex(
    "005070208dea4f01011681fc071e81fc053e81fc035e08ff37108f018880bfeab8fe"
)
STOCK_POST_CLOCK_FALLBACK_MODE = bytes.fromhex("42212797")
STOCK_POST_CLOCK_SPECIAL_SELECTOR = bytes.fromhex(
    "01f8067001ff39106ffe49e00100378d69164621378a"
)
STOCK_POST_CLOCK_ROM_HANDOFF = bytes.fromhex(
    "6916133f34ea010043e03bfe8dea4a0601168dea9707"
)
STOCK_POST_CLOCK_DESCRIPTOR_BRANCH = bytes.fromhex(
    "014b80f8110840e0ff1f8319c5ff0000ffff04e133501499"
    "80f82008c5ff8000ffff34e1bf5f1491402fbdead1010216"
    "0242c4ef002c4835bdeaca010042c4ef2000c1ffff9f0000"
    "0242c5ef002c9319c5ff0000ffff5c93805604974037bdea"
    "b70102160242c4ef80204835bdeab0010042c4ef20000242"
    "c5ef8020c5ff0000ffff5c9f0042c5ef2000"
)
STOCK_DESCRIPTOR_LOOKUP = bytes.fromhex(
    "01164020b1ec38c090a400a2c1ff5406e001d8ec02018000"
)
STOCK_DESCRIPTOR_TABLE = bytes.fromhex(
    "00501e0040501e0080501e00c0501e00"
)
STOCK_POST_CLOCK_COMMON_MASKS = bytes.fromhex(
    "b819c7ff0000ffff90e100174020bdea8403d8e900008919"
    "4020bdeac40340305116bdea7a03d8e9020089194030"
    "bdeaba03"
)
STOCK_DESCRIPTOR_WORD5_HELPER = bytes.fromhex(
    "bfea1afe004264e816035404"
)
STOCK_DESCRIPTOR_WORD8_HELPER = bytes.fromhex(
    "bfeab7fd004264e822035404"
)
STOCK_POST_CLOCK_DESCRIPTOR32 = bytes.fromhex(
    "4820bdea87010316035a3063c1ff00021e00116061e10f10"
    "90e1025701fc0e0430e180af80e80a30c4ef4000c0ef4000"
    "c2ef40004020bfea52273526b563d8e9040089194820bdea"
    "950348304116bdea4b03d8e9060089194830bdea8b03"
)
STOCK_ZERO_STATE = bytes.fromhex(
    "1004c1ff75d9000054ee102000e82c209840c2ffc0191e00"
    "005803ef802b00ef032048ea402bcaef80290aef802ccaef"
    "7827206a70e1400b3031a06accef002b0cef802b206c3034"
    "048ebfeaedee8044c3ef802bc0ef0320c8ef402bcaef802c"
    "206c70e18809a06c"
)
STOCK_ZERO_STATE_GUARD = bytes.fromhex(
    "c0ff0cd90000016040218141101680"
)
STOCK_POST_CLOCK_GATE = bytes.fromhex(
    "c0ff00181e0086534120816000ef10ae81608161816000ef"
    "10ae00ef04a0c0ef10a0c0ef40a0c0ef00afc0ef00ad89f8"
    "13004120816000ef10ae81608161816000ef10ae00ef08a0"
    "c0ef20a0c0ef80a0c0ef80aec0ef80ac"
)
STOCK_POST_CLOCK_TIMER_LOCAL = bytes.fromhex(
    "1004c2ff6c13e30140234121bfea749ac0ffd8d900000060"
    "c1ffecf0100041ea00105a24f4e10102f83f906240ea4110"
    "0004f786"
)
STOCK_POST_CLOCK_TIMER_BOARD = bytes.fromhex(
    "c0ff9b80000041208dea9c00beea9489c0ffc0001e000060"
    "48318dea6f0050e8023a08170487483151208deaba044032"
    "8dea640052ee8007"
)
STOCK_TIMER3_IRQ = bytes.fromhex(
    "e904d8e81f00c0ffa4c00000c0eb0100c0ffecf0100000ef"
    "4000c0ffb0d80100c0eb01008eeacd84005a8eea4d812000"
    "8eea468500524020c1ffe8d50000c2ffbcd500000488d8ec"
    "1a30d8ec2a4003e80140a000c02180f8f6038eea4381d4"
    "e81f00a9048100"
)
STOCK_IRQ_REGISTRATION = bytes.fromhex(
    "75047b3fb0ec3f40c3ff222bea01d8ee0033c4ff00bf"
    "0200d8ec4b2002a262e11c20452f80a3c4ff00f110"
    "00a4e11401251a8eea52e7406090e10305b3e8ff00"
    "311611a161e10e103120211a1019c0608eea51e75504"
)
STOCK_IRQ_MAP = bytes.fromhex(
    "000700010503050301030003020000000303030300000000"
    "010102020200020101010303030002020000000000010004"
    "01010102010100000000000000000000"
)
STOCK_RAM_EXCEPTION_ENTRY = bytes.fromhex(
    "58e92f38d8e8ffff64e0000eb4ea8b56f79f"
)
STOCK_BITMAP_CLEAR_HELPER = bytes.fromhex(
    "71f80afe92a422a1d8ed003261e10f1094e13311d8ed01128000"
)
STOCK_AUDIO_GATE_GUARD = bytes.fromhex(
    "7504c5ffd01a010050ee5d2773e1fe20141604e85b3041210441412000f8041a"
    "b3e10421b119a15231e1f50f81f9120400f8100200f8061a00f80c104016"
    "bfea3aff048b70e1042041a261e10410101952ee5d07402184414016"
)


def sibling_tool(compiler: str, name: str) -> str:
    suffix = ".exe" if sys.platform == "win32" else ""
    candidate = Path(compiler).parent / (name + suffix)
    found = shutil.which(str(candidate)) or shutil.which(name)
    if found is None:
        raise ValueError(f"Tool not found: {name}")
    return found


def verify_stock_image(path: Path) -> None:
    data = path.read_bytes()
    if len(data) != STOCK_IMAGE_SIZE or sha256(data).hexdigest() != STOCK_SHA256:
        raise ValueError("Stock main app hash or size differs from pinned evidence")
    if data[4:16] != bytes.fromhex("ee ff 90 e2 00 00 ed ff 90 e2 00 00") or \
            data[0x2D82E:0x2D858] != STOCK_BOOT_ARGS:
        raise ValueError("Stock Pi32 entry or boot-argument copy differs")
    if data[0x7E:0x8A] != STOCK_ICFG_AFTER_RAM_COPY:
        raise ValueError("Stock post-RAM-copy interrupt configuration differs")
    if data[0xC564E:0xC5652] != bytes((7, 8, 10, 14)):
        raise ValueError("Stock clock ROM mode band table differs")
    if data[0x5FCB2:0x5FCD4] != STOCK_POST_CLOCK_BITMAP_BRANCH or \
            data[0x5FFE8:0x5FFEC] != STOCK_POST_CLOCK_FALLBACK_MODE or \
            data[0x5FFEC:0x60002] != STOCK_POST_CLOCK_SPECIAL_SELECTOR or \
            data[0x5FCD4:0x5FCEA] != STOCK_POST_CLOCK_ROM_HANDOFF or \
            data[0x5FCEA:0x5FD74] != STOCK_POST_CLOCK_DESCRIPTOR_BRANCH or \
            data[0x5FD74:0x5FDA6] != STOCK_POST_CLOCK_COMMON_MASKS or \
            data[0x5FDA6:0x5FE04] != STOCK_POST_CLOCK_DESCRIPTOR32 or \
            data[0x44C84:0x44CEC] != STOCK_ZERO_STATE or \
            data[0x42AAC:0x42ABB] != STOCK_ZERO_STATE_GUARD or \
            data[0x5FE04:0x5FE5C] != STOCK_POST_CLOCK_GATE or \
            data[0x31162:0x31196] != STOCK_POST_CLOCK_TIMER_LOCAL or \
            data[0x5FE5C:0x5FE94] != STOCK_POST_CLOCK_TIMER_BOARD or \
            data[0x3126C:0x312D2] != STOCK_TIMER3_IRQ or \
            data[0x2465A:0x246B0] != STOCK_IRQ_REGISTRATION or \
            data[0xA2A22:0xA2A62] != STOCK_IRQ_MAP or \
            data[0xC618C + 0x1622:0xC618C + 0x1634] != \
                STOCK_RAM_EXCEPTION_ENTRY or \
            data[0xBA:0xD2] != STOCK_DESCRIPTOR_LOOKUP or \
            data[0x554:0x564] != STOCK_DESCRIPTOR_TABLE or \
            data[0x482:0x48E] != STOCK_DESCRIPTOR_WORD5_HELPER or \
            data[0x548:0x554] != STOCK_DESCRIPTOR_WORD8_HELPER or \
            data[0x5FA44:0x5FA5E] != STOCK_BITMAP_CLEAR_HELPER:
        raise ValueError("Stock post-clock branch, selector, or clear helper differs")
    if data[0x5224C:0x522A6] != STOCK_AUDIO_GATE_GUARD:
        raise ValueError("Stock audio gate guard differs")


def verify_linked_elf(path: Path) -> int:
    data = path.read_bytes()
    if len(data) < 52 or data[:6] != b"\x7fELF\x01\x01":
        raise ValueError("Linker did not emit little-endian ELF32")
    file_type, machine = struct.unpack_from("<HH", data, 16)
    entry = struct.unpack_from("<I", data, 24)[0]
    if file_type != 2 or machine != 0xF1:
        raise ValueError("Linker did not emit an executable Pi32v2 ELF")
    if entry != IMAGE_BASE:
        raise ValueError(f"Diagnostic entry differs from main app base: 0x{entry:08X}")
    return entry


def link(compiler: str, output: Path, stock_image: Path | None = None,
         emit_raw: bool = False) -> dict:
    if stock_image is not None:
        verify_stock_image(stock_image)
    output.mkdir(parents=True, exist_ok=True)
    object_manifest = build("main", compiler, output / "objects", no_lto=True)
    source_dir = ROOT / "main"
    runtime = output / "link_check_runtime.o"
    startup = output / "h9_main_startup.o"
    run([compiler, "-target", "pi32v2", "-mcpu=r3", "-c",
         str(source_dir / "h9_main_startup.S"), "-o", str(startup)])
    run([compiler, "-target", "pi32v2", "-mcpu=r3", "-Oz", "-std=c11",
         "-ffreestanding", "-fno-builtin", "-Wall", "-Wextra", "-Werror",
         "-c", str(source_dir / "link_check_runtime.c"), "-o", str(runtime)])
    linker = sibling_tool(compiler, "pi32v2-ld")
    if "elf32pi32v2" not in run([linker, "-V"]):
        raise ValueError("Linker lacks elf32pi32v2 emulation")
    archive = output / "objects" / object_manifest["archive"]
    elf = output / "h9_main_link_check.elf"
    map_file = output / "h9_main_link_check.map"
    script = source_dir / "link_check.ld"
    run([linker, "-m", "elf32pi32v2", "-T", str(script),
         "-Map", str(map_file), "-o", str(elf), str(startup), str(runtime),
         "--whole-archive", str(archive), "--no-whole-archive"])
    entry = verify_linked_elf(elf)
    text_section = output / "main-text.bin"
    run([sibling_tool(compiler, "llvm-objcopy"), "-O", "binary", "-j=.text",
         str(elf), str(text_section)])
    text_data = text_section.read_bytes()
    first = text_data[:14]
    if len(first) != 14 or first[2:8] != bytes.fromhex("ee ff 90 e2 00 00") or \
            first[8:14] != bytes.fromhex("ed ff 90 e2 00 00"):
        raise ValueError("Linked main entry lacks the stock stack setup bytes")
    symbols = run([sibling_tool(compiler, "llvm-nm"), str(elf)])
    match = re.search(r"^([0-9a-fA-F]+) [tT] h9_main_copy_boot_args$",
                      symbols, re.M)
    if match is None:
        raise ValueError("Linked main entry lacks boot-argument copy symbol")
    boot_offset = int(match.group(1), 16) - IMAGE_BASE
    if text_data[boot_offset:boot_offset + len(STOCK_BOOT_ARGS)] != STOCK_BOOT_ARGS:
        raise ValueError("Linked boot-argument copy differs from stock bytes")
    def symbol_address(name: str) -> int:
        symbol = re.search(rf"^([0-9a-fA-F]+) [A-Za-z] {re.escape(name)}$",
                           symbols, re.M)
        if symbol is None:
            raise ValueError(f"Linked main ELF lacks {name}")
        return int(symbol.group(1), 16)

    irq_map_offset = symbol_address("irq_map") - IMAGE_BASE
    if text_data[irq_map_offset:irq_map_offset + 64] != STOCK_IRQ_MAP:
        raise ValueError("Linked IRQ mapping table differs from stock")

    icfg_offset = symbol_address("h9_main_interrupt_config") - IMAGE_BASE
    if text_data[icfg_offset:icfg_offset + len(STOCK_ICFG_AFTER_RAM_COPY)] != \
            STOCK_ICFG_AFTER_RAM_COPY:
        raise ValueError("Linked interrupt configuration differs from stock bytes")

    disassembly = run([sibling_tool(compiler, "llvm-objdump"), "-d",
                       str(elf)])
    runtime_match = re.search(
        r"(?ms)^h9_main_runtime_init:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if runtime_match is None:
        raise ValueError("Linked main ELF lacks a disassembled reset runtime")
    runtime_body = runtime_match.group(1)
    startup_order = (
        "__ram_code_load_start", "h9_main_interrupt_config",
        "h9_main_boot_validator_bind_target", "2ff88",
        "h9_main_boot_validate", "h9_main_consume_boot_record",
        "11c3c", "[r1+0] = r0",
    )
    startup_positions = [runtime_body.find(item) for item in startup_order]
    if -1 in startup_positions or startup_positions != sorted(startup_positions):
        raise ValueError("Linked reset path lost retained-record call order")
    board_match = re.search(
        r"(?ms)^h9_main_board_entry:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if board_match is None:
        raise ValueError("Linked main ELF lacks a disassembled board entry")
    board_body = board_match.group(1)
    board_order = (
        "20000a0", "r0 = 128", "call r2", "2003174", "call r0",
        "h9_main_heap_bitmap_bind_target",
        "h9_main_heap_bitmap_initialize",
        "h9_main_memory_window_bind_target",
        "h9_main_memory_window_initialize",
        "h9_main_board_markers_bind_target",
        "h9_main_board_markers_initialize",
        "h9_main_clock_probe_bind_target",
        "h9_main_clock_probe_run",
        "h9_main_clock_selector_bind_target",
        "h9_main_clock_selector_run",
        "h9_main_clock_band_bind_target",
        "h9_main_clock_band_select",
        "h9_main_clock_calculation_bind_target",
        "h9_main_clock_calculate",
        "55780", "h9_main_clock_match_bind_target",
        "h9_main_clock_match",
        "h9_main_clock_timing_bind_target",
        "h9_main_clock_timing_calculate",
        "h9_main_clock_apply_bind_target",
        "h9_main_clock_apply_first",
        "h9_main_clock_rom_mode_bind_target",
        "h9_main_clock_rom_mode_apply",
        "h9_main_clock_registers_bind_target",
        "h9_main_clock_aux_bind_target",
        "h9_main_clock_finish_bind_target",
        "h9_main_clock_finish_run",
        "h9_main_post_clock_rom_bind_target",
        "h9_main_post_clock_rom_run",
        "h9_main_post_clock_classifier_bind_target",
        "h9_main_post_clock_classify",
        "h9_main_post_clock_bitmap_step",
        "h9_main_post_clock_handoff_bind_target",
        "h9_main_post_clock_handoff_run",
        "h9_main_post_clock_descriptor_route_bind_target",
        "h9_main_post_clock_descriptor_route_run",
        "h9_main_post_clock_apply_initial_masks",
        "h9_main_post_clock_descriptor32_bind_target",
        "h9_main_post_clock_descriptor32_run",
        "h9_main_post_clock_gate_bind_target",
        "h9_main_post_clock_gate_run",
        "h9_main_post_clock_timer_stage_bind_target",
        "h9_main_post_clock_timer_stage_run",
    )
    board_positions = [board_body.find(item) for item in board_order]
    if -1 in board_positions or board_positions != sorted(board_positions):
        raise ValueError("Linked board entry lost ROM and bitmap call order")
    if board_body.count("h9_main_clock_selector_run") < 2 or \
            board_body.rfind("h9_main_clock_selector_run") < \
            board_body.find("h9_main_clock_rom_mode_apply"):
        raise ValueError("Linked board entry lost repeated clock selector")
    if board_body.count("h9_main_irq_register") < 2 or \
            "h9_main_default_irq" not in board_body:
        raise ValueError("Linked board entry lacks default IRQ 0/1 registration")
    for name, required in (
        ("target_rom_apply", "2001634"),
        ("target_synchronize", "csync"),
        ("target_delay_nops", "nop"),
        ("target_rom_mode", "20000e2"),
        ("target_rom_finish", "2001650"),
        ("target_read_selector", "2000058"),
        ("target_set_control", "20008fe"),
        ("target_transform_mask", "2000a78"),
        ("target_query_pair", "2000d18"),
    ):
        helper = re.search(
            rf"(?ms)^{name}:\n(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
        )
        if helper is None or required not in helper.group(1):
            raise ValueError(f"Linked clock-apply helper lacks {required}")
    classifier_binder = re.search(
        r"(?ms)^h9_main_post_clock_classifier_bind_target:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    if classifier_binder is None or not all(
        address in classifier_binder.group(1)
        for address in ("1048832", "1986844", "2032128", "72515")
    ) or "r2 = r2 + 768" not in classifier_binder.group(1):
        raise ValueError("Linked post-clock classifier lost target MMIO addresses")
    classifier = re.search(
        r"(?ms)^h9_main_post_clock_classify:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    if classifier is None or not all(
        value in classifier.group(1)
        for value in ("r1 = 148", "0x154", "0x2A8", "0xFFFFFF01")
    ):
        raise ValueError("Linked post-clock classifier lost stock field masks")
    bitmap_step = re.search(
        r"(?ms)^h9_main_post_clock_bitmap_step:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    bitmap_body = bitmap_step.group(1) if bitmap_step is not None else ""
    index_limit = re.search(r"r(\d+) > 55", bitmap_body)
    if "r1 = 192" not in bitmap_body or index_limit is None or \
            "== 56" not in bitmap_body or "!= 57" not in bitmap_body or \
            f"&= ~(1 << r{index_limit.group(1)})" not in bitmap_body:
        raise ValueError("Linked post-clock bitmap step lost selector or bit clear")
    handoff = re.search(
        r"(?ms)^h9_main_post_clock_handoff_run:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    handoff_body = handoff.group(1) if handoff is not None else ""
    if "= -453" not in handoff_body or "= -1" not in handoff_body or \
            re.search(r"d\[r\d+\+8\] = r1_r0", handoff_body) is None:
        raise ValueError("Linked post-clock ROM handoff lost mask or result pair")
    descriptor_lookup = re.search(
        r"(?ms)^target_lookup_descriptor:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    descriptor_body = descriptor_lookup.group(1) if descriptor_lookup else ""
    if "<= 56" not in descriptor_body or "1e5000" not in descriptor_body:
        raise ValueError("Linked descriptor lookup lost stock table bounds or base")
    descriptor_route = re.search(
        r"(?ms)^h9_main_post_clock_descriptor_route_run:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    route_body = descriptor_route.group(1) if descriptor_route else ""
    if "== 0" not in route_body or "!= 4" not in route_body or \
            "= 8191" not in route_body or "= 40959" not in route_body or \
            route_body.count("clear_descriptor_bits") != 2:
        raise ValueError("Linked descriptor route lost ROM-result branches")
    initial_masks = re.search(
        r"(?ms)^h9_main_post_clock_apply_initial_masks:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    masks_body = initial_masks.group(1) if initial_masks else ""
    if masks_body.count("and_descriptor_word") != 4 or \
            "r1 = 16" not in masks_body or \
            "r2 = 5" not in masks_body or "r2 = 8" not in masks_body:
        raise ValueError("Linked common descriptor masks lost ordered writes")
    descriptor32 = re.search(
        r"(?ms)^h9_main_post_clock_descriptor32_run:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    descriptor32_body = descriptor32.group(1) if descriptor32 else ""
    if descriptor32_body.count("and_descriptor_word") != 3 or \
            "r1 = 32" not in descriptor32_body or \
            "r1 = 48" not in descriptor32_body or \
            "0xFFFFFFBF" not in descriptor32_body or \
            "0xFFEEFFFF" not in descriptor32_body:
        raise ValueError("Linked descriptor-32 branch lost stock register writes")
    gate = re.search(
        r"(?ms)^h9_main_post_clock_gate_run:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    gate_body = gate.group(1) if gate else ""
    if gate_body.count("|= 0x900") != 4 or \
            gate_body.count("|= 0x4") != 1 or \
            gate_body.count("|= 0x8") != 1 or \
            gate_body.count("[r3+0] =") < 6:
        raise ValueError("Linked post-clock gate lost repeated stock writes")
    timer_isr = re.search(
        r"(?ms)^h9_main_timer3_isr:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    timer_isr_body = timer_isr.group(1) if timer_isr else ""
    timer_isr_order = (
        "[--sp] = {psr, rets, reti}", "[--sp] = {r4-r0}",
        "c0a4", "10f0ec", "|= 0x40", "1d8b0",
        "h9_rom_timer_poll : 2001d2e",
        "h9_rom_clock_enter : 2001634", "csync",
        "h9_rom_clock_check : 2001e2c", "d5e8", "d5bc",
        "swi 0", "h9_rom_clock_leave : 2001650",
        "{r4-r0} = [sp++]", "{psr, rets, reti} = [sp++]", "rti",
    )
    timer_isr_positions = [timer_isr_body.find(item) for item in timer_isr_order]
    if -1 in timer_isr_positions or timer_isr_positions != sorted(timer_isr_positions):
        raise ValueError("Linked timer-3 interrupt lost stock save/restore order")
    timer_install = re.search(
        r"(?ms)^target_install_timer_irq:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    timer_install_body = timer_install.group(1) if timer_install else ""
    if "h9_main_irq_registration_bind_target" not in timer_install_body or \
            "h9_main_irq_register" not in timer_install_body:
        raise ValueError("Linked timer-3 registration lost common installer")
    irq_binder = re.search(
        r"(?ms)^h9_main_irq_registration_bind_target:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    irq_binder_body = irq_binder.group(1) if irq_binder else ""
    if "2bf00" not in irq_binder_body or "10f100" not in irq_binder_body:
        raise ValueError("Linked IRQ binder lost vector or control base")
    irq_register = re.search(
        r"(?ms)^h9_main_irq_register:\n"
        r"(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
    )
    irq_register_body = irq_register.group(1) if irq_register else ""
    if "irq_map" not in irq_register_body or \
            "r1 = r1 & 0xE" not in irq_register_body or \
            "r1 |= 1" not in irq_register_body:
        raise ValueError("Linked IRQ installer lost stock priority mapping")
    for name, expected in (
        ("target_enter", "2001634"),
        ("target_leave", "2001650"),
    ):
        helper = re.search(
            rf"(?ms)^{name}:\n(.*?)(?=^\S[^\n]*:\n|\Z)", disassembly
        )
        if helper is None or expected not in helper.group(1):
            raise ValueError(f"Linked timer-3 helper lacks {expected}")
    bitmap_match = re.search(
        r"(?ms)^mark_blocks:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if bitmap_match is None or bitmap_match.group(1).count("|= 1 <<") != 2:
        raise ValueError("Linked bitmap stage lost Pi32 bitset instructions")
    heap_match = re.search(
        r"(?ms)^h9_main_heap_bitmap_initialize:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if heap_match is None or not all(
        f"r1 = {first}" in heap_match.group(1)
        for first in (970, 1444)
    ) or not all(
        f"r2 = {last}" in heap_match.group(1)
        for last in (1406, 1535)
    ):
        raise ValueError("Linked bitmap stage lost stock block boundaries")
    metadata_order = ("1e500", "2ff80", "10780", "5ff03ca", "20f0000")
    metadata_positions = [heap_match.group(1).find(value)
                          for value in metadata_order]
    if -1 in metadata_positions or metadata_positions != \
            sorted(metadata_positions):
        raise ValueError("Linked bitmap stage lost stock metadata values")
    window_match = re.search(
        r"(?ms)^h9_main_memory_window_initialize:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if window_match is None:
        raise ValueError("Linked main ELF lacks memory-window stage")
    window_body = window_match.group(1).lower()
    window_order = (r"0x2c000\b", r"184831\b", r"\|= 0x10\b",
                    r"&= 0xffffffef\b", r"\|= 0x1\b", r"!= 480\b")
    window_positions = [(match.start() if (match := re.search(item, window_body))
                         else -1) for item in window_order]
    if -1 in window_positions or window_positions != sorted(window_positions):
        raise ValueError("Linked memory-window setup lost stock order")
    marker_bind_match = re.search(
        r"(?ms)^h9_main_board_markers_bind_target:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    marker_match = re.search(
        r"(?ms)^h9_main_board_markers_initialize:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if marker_bind_match is None or marker_match is None:
        raise ValueError("Linked main ELF lacks early board markers")
    marker_bind = marker_bind_match.group(1).lower()
    marker_body = marker_match.group(1).lower()
    if not all(value in marker_bind for value in
               ("55936", "2064", "-40", "-136")) or \
            not all(value in marker_body for value in
                    ("r3 = 90", "b[r2+r1] = r3", "r1 != 15",
                     "b[r2+0] = r1", "b[r0+0] = r1")):
        raise ValueError("Linked early board markers differ from stock addresses or writes")
    probe_bind_match = re.search(
        r"(?ms)^h9_main_clock_probe_bind_target:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    probe_match = re.search(
        r"(?ms)^h9_main_clock_probe_run:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if probe_bind_match is None or probe_match is None:
        raise ValueError("Linked main ELF lacks early clock probe")
    probe_bind = probe_bind_match.group(1).lower()
    probe_body = probe_match.group(1).lower()
    if not all(value in probe_bind for value in
               ("1f0200", "da08", "1e0400")) or \
            not all(value in disassembly.lower() for value in
                    ("2000058", "2000a28")) or \
            not all(value in probe_body for value in
                    ("50000000", "r1 = 176", "r1 = 177", "r1 = 178",
                     "0x960", "0x82", "r2 = 253", "r7 > 3")):
        raise ValueError("Linked early clock probe differs from stock constants")
    selector_bind_match = re.search(
        r"(?ms)^h9_main_clock_selector_bind_target:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    selector_match = re.search(
        r"(?ms)^h9_main_clock_selector_run:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if selector_bind_match is None or selector_match is None:
        raise ValueError("Linked main ELF lacks early clock selector")
    selector_bind = selector_bind_match.group(1).lower()
    selector_body = selector_match.group(1).lower()
    if not all(value in selector_bind for value in
               ("1972512", "55896")) or \
            not all(value in selector_body for value in
                    ("857559629", "0x4000000", "4354",
                     "0xfff807ff", "20480", "0x50000")):
        raise ValueError("Linked early clock selector differs from stock constants")
    band_bind_match = re.search(
        r"(?ms)^h9_main_clock_band_bind_target:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    band_match = re.search(
        r"(?ms)^h9_main_clock_band_select:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if band_bind_match is None or band_match is None:
        raise ValueError("Linked main ELF lacks early clock band selector")
    band_bind = band_bind_match.group(1).lower()
    band_body = band_match.group(1).lower()
    if not all(value in band_bind for value in
               ("55820", "-60", "-44")) or \
            not all(value in band_body for value in
                    ("r3 = 3", "r6 >= r1", "r6 < 4", "r5++=20")) or \
            "240000000" not in board_body:
        raise ValueError("Linked clock band selector differs from stock flow")
    calculation_bind_match = re.search(
        r"(?ms)^h9_main_clock_calculation_bind_target:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    calculation_match = re.search(
        r"(?ms)^h9_main_clock_calculate:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if calculation_bind_match is None or calculation_match is None:
        raise ValueError("Linked main ELF lacks clock calculation")
    calculation_bind = calculation_bind_match.group(1).lower()
    calculation_body = calculation_match.group(1).lower()
    if not all(value in calculation_bind for value in
               ("55816", "-52")) or \
            not all(value in calculation_body for value in
                    ("r0 = -1", "r2 * 0x14", "[r3+12]",
                     "umax", "/ r1 (u)", "r0 *= r1")):
        raise ValueError("Linked clock calculation differs from stock flow")
    match_bind_match = re.search(
        r"(?ms)^h9_main_clock_match_bind_target:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    match_match = re.search(
        r"(?ms)^h9_main_clock_match:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if match_bind_match is None or match_match is None:
        raise ValueError("Linked main ELF lacks clock source/divider search")
    match_bind = match_bind_match.group(1).lower()
    match_body = match_match.group(1).lower()
    if not all(value in match_bind for value in
               ("clock_source_hz", "clock_primary_dividers",
                "clock_secondary_dividers", "55804", "0xda00")) or \
            not all(value in match_body for value in
                    ("r13 < 4", "r12 < 4", "r6 < 4", "/ r4 (u)",
                     "/ r2 (u)", "0xffffffff")) or \
            "if (r4 == -1)" not in board_body:
        raise ValueError("Linked clock source/divider search differs from stock flow")
    stock_clock_tables = bytes.fromhex(
        "00 b0 71 0b 40 74 2a 08 00 d0 12 13 00 38 9c 1c "
        "01 02 04 08 01 03 05 07"
    )
    if stock_clock_tables not in text_data:
        raise ValueError("Linked clock source/divider tables differ from stock")

    register_bind_match = re.search(
        r"(?ms)^h9_main_clock_registers_bind_target:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    register_program_match = re.search(
        r"(?ms)^h9_main_clock_program_source:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    register_latch_match = re.search(
        r"(?ms)^h9_main_clock_latch_source:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if any(item is None for item in
           (register_bind_match, register_program_match, register_latch_match)):
        raise ValueError("Linked main ELF lacks clock source register helpers")
    register_bind = register_bind_match.group(1).lower()
    register_program = register_program_match.group(1).lower()
    register_latch = register_latch_match.group(1).lower()
    if not all(value in register_bind for value in
               ("1966100", "55804", "r2 = r3 + 4", "r3 += -24")) or \
            not all(value in register_program for value in
                    ("if (r1 == -1)", "insert(r1, p:0, l:2)",
                     "insert(r1, p:2, l:4)")) or \
            not all(value in register_latch for value in
                    ("r2 = 6", "r0 = r1", "[r0++=48]", "r1 = 28",
                     "[r0+0] |= 1 << r1", "r1 > 23")):
        raise ValueError("Linked clock source helpers differ from stock register flow")

    aux_bind_match = re.search(
        r"(?ms)^h9_main_clock_aux_bind_target:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    aux_rom_match = re.search(
        r"(?ms)^target_rom_apply_pair:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    aux_program_match = re.search(
        r"(?ms)^h9_main_clock_aux_program:\n(.*?)(?=^\S[^\n]*:\n|\Z)",
        disassembly,
    )
    if any(item is None for item in
           (aux_bind_match, aux_rom_match, aux_program_match)):
        raise ValueError("Linked main ELF lacks auxiliary clock fields")
    aux_bind = aux_bind_match.group(1).lower()
    aux_rom = aux_rom_match.group(1).lower()
    aux_program = aux_program_match.group(1).lower()
    aux_order = ("insert(r3, p:12, l:3)", "call r6",
                 "insert(r2, p:0, l:8)", "insert(r2, p:8, l:3)")
    aux_positions = [aux_program.find(value) for value in aux_order]
    if not all(value in aux_bind for value in ("1966088", "55784")) or \
            "33554704" not in aux_rom or \
            -1 in aux_positions or aux_positions != sorted(aux_positions):
        raise ValueError("Linked auxiliary clock field order differs from stock")

    data_start = symbol_address("__data_start")
    data_end = symbol_address("__data_end")
    bss_start = symbol_address("__bss_start")
    bss_end = symbol_address("__bss_end")
    data_load = symbol_address("__data_load_start")
    ram_code_start = symbol_address("__ram_code_start")
    ram_code_end = symbol_address("__ram_code_end")
    ram_code_load = symbol_address("__ram_code_load_start")
    serial_bus_entry = symbol_address("h9_main_serial_bus_transfer")
    serial_read_entry = symbol_address("h9_main_serial_command_read")
    serial_write_entry = symbol_address("h9_main_serial_command_write")
    serial_gate_acquire = symbol_address("h9_main_serial_gate_acquire")
    serial_gate_release = symbol_address("h9_main_serial_gate_release")
    default_irq_entry = symbol_address("h9_main_default_irq")
    if symbol_address("__stock_clear_start") != RAM_START or \
            symbol_address("__stock_clear_end") != RAM_END:
        raise ValueError("Linked RAM-clear interval differs from stock startup")
    if not (RAM_START <= data_start <= data_end <= bss_start <= bss_end <= RAM_END):
        raise ValueError("Linked C data/BSS exceed observed stock-cleared RAM interval")
    if not (0 <= ram_code_start <= ram_code_end <= 0xDA78):
        raise ValueError("Linked RAM code exceeds stock relocation interval")
    if ram_code_start != 0 or serial_bus_entry != 0x40 or ram_code_end <= 0x40:
        raise ValueError("Linked RAM serial-bus routine is not at stock RAM address 0x40")
    if not (serial_bus_entry < serial_read_entry < ram_code_end and
            serial_bus_entry < serial_write_entry < ram_code_end and
            serial_bus_entry < serial_gate_acquire < serial_gate_release <
            ram_code_end):
        raise ValueError("Linked serial command handlers are not relocated RAM code")
    if not (0x40 < default_irq_entry < ram_code_end):
        raise ValueError("Linked default IRQ handler is not in RAM code")
    ram_code_section = output / "main-ram-code.bin"
    run([sibling_tool(compiler, "llvm-objcopy"), "-O", "binary",
         "-j=.ram_code", str(elf), str(ram_code_section)])
    ram_code_data = ram_code_section.read_bytes()
    if len(ram_code_data) != ram_code_end - ram_code_start or \
            ram_code_data[:0x40] != bytes(0x40):
        raise ValueError("Linked RAM code has unexpected length or nonzero prefix")
    if ram_code_data[default_irq_entry:default_irq_entry + 12] != \
            STOCK_RAM_EXCEPTION_ENTRY[:12]:
        raise ValueError("Linked default IRQ entry lost stock register saves")
    gate_bytes = ram_code_data[serial_gate_acquire:serial_gate_release]
    if not all(opcode in gate_bytes for opcode in
               (bytes.fromhex("60 00"), bytes.fromhex("20 00"),
                bytes.fromhex("61 00"))):
        raise ValueError("Serial gate lost Pi32 interrupt/synchronization operations")
    if not (IMAGE_BASE <= data_load and data_load + data_end - data_start <=
            IMAGE_BASE + STOCK_IMAGE_SIZE):
        raise ValueError("Linked C data load exceeds stock app address envelope")
    if not (IMAGE_BASE <= ram_code_load and
            ram_code_load + ram_code_end - ram_code_start <=
            IMAGE_BASE + STOCK_IMAGE_SIZE):
        raise ValueError("Linked RAM-code load exceeds stock app address envelope")
    data_bytes = b""
    if data_end != data_start:
        data_section = output / "main-data.bin"
        run([sibling_tool(compiler, "llvm-objcopy"), "-O", "binary",
             "-j=.data", str(elf), str(data_section)])
        data_bytes = data_section.read_bytes()
        if len(data_bytes) != data_end - data_start:
            raise ValueError("Linked initialized data differs from RAM data span")
    raw_file = None
    raw_digest = None
    if emit_raw:
        # The vendor llvm-objcopy accepts only one -j section and has no
        # --gap-fill/--pad-to support, so place verified sections by LMA.
        raw = bytearray(b"\xFF" * STOCK_IMAGE_SIZE)
        used: list[tuple[int, int]] = []

        def place(name: str, load_address: int, payload: bytes) -> None:
            if not payload:
                return
            start = load_address - IMAGE_BASE
            end = start + len(payload)
            if start < 0 or end > STOCK_IMAGE_SIZE or any(
                    start < prior_end and prior_start < end
                    for prior_start, prior_end in used):
                raise ValueError(f"Raw {name} load span overlaps or exceeds stock app")
            raw[start:end] = payload
            used.append((start, end))

        place("text", IMAGE_BASE, text_data)
        place("RAM code", ram_code_load, ram_code_data)
        place("initialized data", data_load, data_bytes)
        raw_file = output / "h9_main_diagnostic_NOT_FOR_FLASH.raw"
        raw_file.write_bytes(raw)
        if len(raw) != STOCK_IMAGE_SIZE or raw[:len(text_data)] != text_data or \
                raw[ram_code_load - IMAGE_BASE:
                    ram_code_load - IMAGE_BASE + len(ram_code_data)] != ram_code_data:
            raise ValueError("Raw main export differs from linked sections or stock size")
        raw_digest = sha256(raw).hexdigest()
    undefined = run([sibling_tool(compiler, "llvm-nm"), "-u", str(elf)])
    if undefined:
        raise ValueError(f"Linked ELF has undefined symbols:\n{undefined}")
    map_data = map_file.read_text(encoding="utf-8", errors="replace")
    archive_section = map_data.split("Memory Configuration", 1)[0]
    included = re.findall(r"libh9_main_replacement\.a\(([^)]+)\)"
                          r"\s+\(--whole-archive\)",
                          archive_section)
    expected = {item["object"] for item in object_manifest["objects"]}
    if len(included) != len(expected) or set(included) != expected:
        raise ValueError("Link map does not include every main source object")
    manifest = {
        "kind": "main_whole_archive_link_check",
        "stock_envelope": {
            "image_base": IMAGE_BASE,
            "image_bytes": STOCK_IMAGE_SIZE,
            "stock_sha256": STOCK_SHA256,
            "private_stock_image_checked": stock_image is not None,
            "ram_layout_verified": False,
        },
        "compiler": compiler,
        "linker": linker,
        "source_objects": len(included),
        "startup_object": startup.name,
        "stock_stack_pointer": 58000,
        "entry_stack_bytes_verified": True,
        "boot_argument_copy_bytes_verified": True,
        "post_ram_copy_icfg_bytes_verified": True,
        "post_copy_boot_record_path_verified": True,
        "early_board_bitmap_path_verified": True,
        "early_board_metadata_values_verified": True,
        "early_board_memory_window_path_verified": True,
        "early_board_markers_path_verified": True,
        "early_board_clock_probe_path_verified": True,
        "early_board_clock_selector_path_verified": True,
        "early_board_clock_band_path_verified": True,
        "early_board_clock_calculation_path_verified": True,
        "early_board_clock_match_path_verified": True,
        "clock_source_register_writer_link_verified": True,
        "clock_source_latch_link_verified": True,
        "clock_aux_field_writer_link_verified": True,
        "early_board_clock_finish_path_verified": True,
        "early_board_post_clock_rom_path_verified": True,
        "early_board_post_clock_classifier_path_verified": True,
        "early_board_post_clock_bitmap_path_verified": True,
        "early_board_post_clock_handoff_path_verified": True,
        "early_board_post_clock_descriptor_route_verified": True,
        "early_board_post_clock_initial_masks_verified": True,
        "early_board_post_clock_descriptor32_verified": True,
        "early_board_post_clock_gate_verified": True,
        "early_board_timer3_setup_and_isr_link_verified": True,
        "early_board_default_irq_snapshot_link_verified": True,
        "c_ram_start": data_start,
        "c_ram_end": bss_end,
        "ram_code_bytes": ram_code_end - ram_code_start,
        "ram_code_load_address": ram_code_load,
        "initialized_data_load_address": data_load,
        "ram_serial_bus_entry": serial_bus_entry,
        "ram_serial_read_entry": serial_read_entry,
        "ram_serial_write_entry": serial_write_entry,
        "ram_serial_gate_acquire_entry": serial_gate_acquire,
        "ram_serial_gate_release_entry": serial_gate_release,
        "ram_code_zero_prefix_verified": True,
        "linker_script_sha256": sha256(script.read_bytes()).hexdigest(),
        "elf": elf.name,
        "elf_sha256": sha256(elf.read_bytes()).hexdigest(),
        "map": map_file.name,
        "map_sha256": sha256(map_file.read_bytes()).hexdigest(),
        "entry": entry,
        "raw_export": raw_file.name if raw_file is not None else None,
        "raw_export_sha256": raw_digest,
        "raw_export_purpose": "offline_layout_inspection_only" if emit_raw else None,
        "firmware_image_created": False,
        "flashable": False,
    }
    (output / "link-check-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", help="JieLi Pi32v2 clang executable path")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build" / "link-check-main")
    parser.add_argument("--stock-image", type=Path,
                        help="optionally recheck the private pinned app.bin")
    parser.add_argument("--emit-raw", action="store_true",
                        help="export a stock-size raw layout for offline inspection only")
    args = parser.parse_args()
    try:
        compiler = find_compiler("main", args.compiler)
        manifest = link(compiler, args.output, args.stock_image, args.emit_raw)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Link check failed: {error}", file=sys.stderr)
        return 1
    print(f"Linked {manifest['source_objects']} main source objects: "
          f"{args.output / manifest['elf']}")
    print(f"Entry: 0x{manifest['entry']:08X}")
    if manifest["raw_export"]:
        print(f"Offline raw layout: {args.output / manifest['raw_export']}")
        print(f"Raw SHA-256: {manifest['raw_export_sha256']}")
    print("Diagnostic only; no bootable or flashable update image was produced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
