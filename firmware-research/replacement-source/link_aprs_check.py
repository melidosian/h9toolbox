#!/usr/bin/env python3
"""Whole-archive APRS link check against the observed stock address envelope.

This links first-pass startup to a partial board entry. An optional raw export
is for offline layout inspection only; it is not a validated update image.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import shutil
import struct
import sys

from build_target_objects import ROOT, build, find_compiler, run, verify_arm_object


IMAGE_BASE = 0x08002000
STOCK_IMAGE_SIZE = 17_304
STOCK_STACK_TOP = 0x20001A50
STOCK_SHA256 = "1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33"


def sibling_tool(compiler: str, name: str) -> str:
    suffix = ".exe" if sys.platform == "win32" else ""
    candidate = Path(compiler).parent / (name + suffix)
    found = shutil.which(str(candidate)) or shutil.which(name)
    if found is None:
        raise ValueError(f"Tool not found: {name}")
    return found


def verify_stock_envelope(path: Path) -> None:
    data = path.read_bytes()
    if sha256(data).hexdigest() != STOCK_SHA256 or len(data) != STOCK_IMAGE_SIZE:
        raise ValueError("Stock APRS image hash or size differs from pinned evidence")
    stack_pointer, reset_vector = struct.unpack_from("<II", data)
    if stack_pointer != STOCK_STACK_TOP or reset_vector != 0x080020D5:
        raise ValueError("Stock APRS vectors differ from pinned evidence")
    if struct.unpack_from("<I", data, 28 * 4)[0] != 0x08002235:
        raise ValueError("Stock APRS ADC IRQ vector differs from pinned evidence")


def read_stack_usage(paths: list[Path]) -> list[dict]:
    records = []
    for path in paths:
        if not path.is_file():
            raise ValueError(f"Missing compiler stack-usage report: {path}")
        for line in path.read_text(encoding="utf-8").splitlines():
            fields = line.split("\t")
            if len(fields) != 3 or not fields[1].isdigit():
                raise ValueError(f"Unrecognized stack-usage record in {path}: {line}")
            records.append({
                "function": fields[0].rsplit(":", 1)[-1],
                "source_location": fields[0],
                "frame_bytes": int(fields[1]),
                "kind": fields[2],
            })
    return sorted(records, key=lambda item: item["frame_bytes"], reverse=True)


def link(compiler: str, output: Path, stock_image: Path | None = None,
         emit_raw: bool = False) -> dict:
    if stock_image is not None:
        verify_stock_envelope(stock_image)
    output.mkdir(parents=True, exist_ok=True)
    object_manifest = build("aprs", compiler, output / "objects",
                            stack_usage=True)
    source_dir = ROOT / "aprs"
    compiler_flags = [
        "-mcpu=cortex-m0", "-mthumb", "-Os", "-ffreestanding",
        "-fno-builtin", "-Wall", "-Wextra", "-Werror",
        "-fstack-usage",
    ]
    runtime = output / "link_check_runtime.o"
    startup_source = output / "h9_aprs_startup.o"
    vectors = output / "link_check_startup.o"
    run([compiler, *compiler_flags, "-std=c11", "-c",
         f"-I{ROOT / 'common'}", f"-I{ROOT / 'aprs'}",
         str(source_dir / "link_check_runtime.c"), "-o", str(runtime)])
    run([compiler, *compiler_flags, "-std=c11", "-c",
         str(source_dir / "h9_aprs_startup.c"), "-o", str(startup_source)])
    run([compiler, *compiler_flags, "-c",
         str(source_dir / "link_check_startup.S"), "-o", str(vectors)])
    stack_usage = read_stack_usage([
        *(output / "objects" / Path(item["object"]).with_suffix(".su")
          for item in object_manifest["objects"]),
        runtime.with_suffix(".su"), startup_source.with_suffix(".su"),
    ])

    elf = output / "h9_aprs_link_check.elf"
    map_file = output / "h9_aprs_link_check.map"
    script = source_dir / "link_check.ld"
    archive = output / "objects" / object_manifest["archive"]
    run([compiler, "-mcpu=cortex-m0", "-mthumb", "-nostdlib",
         "-Wl,--build-id=none", f"-Wl,-Map={map_file}",
         "-T", str(script), "-o", str(elf), str(vectors), str(startup_source),
         str(runtime),
         "-Wl,--whole-archive", str(archive), "-Wl,--no-whole-archive",
         "-lgcc"])
    elf_data = elf.read_bytes()
    verify_arm_object(elf_data, elf.name)

    vector_bytes = output / "vectors.bin"
    objcopy = sibling_tool(compiler, "arm-none-eabi-objcopy")
    run([objcopy, "--dump-section", f".isr_vector={vector_bytes}", str(elf)])
    table = vector_bytes.read_bytes()
    if len(table) != 192:
        raise ValueError("Linked vector table must have exactly 48 entries")
    stack_pointer, reset_vector = struct.unpack_from("<II", table)
    if stack_pointer != STOCK_STACK_TOP:
        raise ValueError(f"Linked stack pointer differs: 0x{stack_pointer:08X}")
    if reset_vector & 1 == 0 or not (
        IMAGE_BASE <= (reset_vector & ~1) < IMAGE_BASE + STOCK_IMAGE_SIZE
    ):
        raise ValueError(f"Linked reset vector is outside APRS image: 0x{reset_vector:08X}")
    for index in range(1, 48):
        vector = struct.unpack_from("<I", table, index * 4)[0]
        if vector & 1 == 0 or not (
            IMAGE_BASE <= (vector & ~1) < IMAGE_BASE + STOCK_IMAGE_SIZE
        ):
            raise ValueError(f"Vector {index} is not a Thumb code address")

    size_tool = sibling_tool(compiler, "arm-none-eabi-size")
    size_output = run([size_tool, str(elf)])
    symbols = run([sibling_tool(compiler, "arm-none-eabi-nm"), str(elf)])
    symbol_values = {}
    required_symbols = {
        "__bss_end", "__stack_top", "Default_Handler",
        "h9_aprs_systick_handler",
        "h9_aprs_dma1_channel1_handler", "h9_aprs_adc1_handler",
        "h9_aprs_dma1_channel3_handler",
        "h9_aprs_usart1_handler", "h9_aprs_usart2_handler",
    }
    for line in symbols.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in required_symbols:
            symbol_values[parts[2]] = int(parts[0], 16)
    if set(symbol_values) != required_symbols:
        raise ValueError("Linked ELF lacks RAM-boundary or IRQ symbols")
    for slot, handler in ((15, "h9_aprs_systick_handler"),
                          (25, "h9_aprs_dma1_channel1_handler"),
                          (26, "h9_aprs_dma1_channel3_handler"),
                          (28, "h9_aprs_adc1_handler"),
                          (43, "h9_aprs_usart1_handler"),
                          (44, "h9_aprs_usart2_handler")):
        vector = struct.unpack_from("<I", table, slot * 4)[0]
        if vector != (symbol_values[handler] | 1):
            raise ValueError(f"Vector {slot} does not point to {handler}")
    stack_headroom = symbol_values["__stack_top"] - symbol_values["__bss_end"]
    if stack_headroom < 1024:
        raise ValueError(f"APRS diagnostic stack headroom is only {stack_headroom} bytes")
    raw_file = None
    raw_digest = None
    if emit_raw:
        raw_file = output / "h9_aprs_diagnostic_NOT_FOR_FLASH.raw"
        run([objcopy, "-O", "binary", "--gap-fill=0xFF",
             f"--pad-to=0x{IMAGE_BASE + STOCK_IMAGE_SIZE:X}",
             str(elf), str(raw_file)])
        raw_data = raw_file.read_bytes()
        if len(raw_data) != STOCK_IMAGE_SIZE or raw_data[:len(table)] != table:
            raise ValueError("Raw APRS export differs from linked vectors or stock size")
        raw_digest = sha256(raw_data).hexdigest()
    manifest = {
        "kind": "aprs_whole_archive_link_check",
        "stock_envelope": {
            "image_base": IMAGE_BASE,
            "image_bytes": STOCK_IMAGE_SIZE,
            "stack_top": STOCK_STACK_TOP,
            "stock_sha256": STOCK_SHA256,
            "private_stock_image_checked": stock_image is not None,
        },
        "compiler": compiler,
        "source_objects": len(object_manifest["objects"]),
        "startup_source_object": startup_source.name,
        "vector_entries": 48,
        "irq_vectors": {
            "systick": 15,
            "dma1_channel1": 25,
            "dma1_channel3": 26,
            "adc1": 28,
            "usart1": 43,
            "usart2": 44,
        },
        "board_initialization_implemented": False,
        "uart1_receive_initialization_included": True,
        "uart1_transmit_callback_included": True,
        "tx_service_callbacks_included": True,
        "tx_rf_authorized_in_diagnostic": False,
        "linker_script_sha256": sha256(script.read_bytes()).hexdigest(),
        "elf": elf.name,
        "elf_sha256": sha256(elf_data).hexdigest(),
        "map": map_file.name,
        "map_sha256": sha256(map_file.read_bytes()).hexdigest(),
        "stack_pointer": stack_pointer,
        "reset_vector": reset_vector,
        "size_output": size_output,
        "bss_end": symbol_values["__bss_end"],
        "stack_headroom_bytes": stack_headroom,
        "largest_compiler_reported_frame_bytes": stack_usage[0]["frame_bytes"],
        "stack_usage_records": stack_usage,
        "stack_usage_limit": "per-function frames only; no call-chain or ISR nesting proof",
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
    parser.add_argument("--compiler", help="arm-none-eabi-gcc executable path")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build" / "link-check-aprs")
    parser.add_argument("--stock-image", type=Path,
                        help="optionally recheck the private pinned APRS-1.0.15.bin")
    parser.add_argument("--emit-raw", action="store_true",
                        help="export a stock-size raw layout for offline inspection only")
    args = parser.parse_args()
    try:
        compiler = find_compiler("aprs", args.compiler)
        manifest = link(compiler, args.output, args.stock_image, args.emit_raw)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Link check failed: {error}", file=sys.stderr)
        return 1
    print(f"Linked {manifest['source_objects']} APRS source objects: "
          f"{args.output / manifest['elf']}")
    print(f"Vector: stack 0x{manifest['stack_pointer']:08X}, "
          f"reset 0x{manifest['reset_vector']:08X}")
    print(manifest["size_output"])
    print(f"RAM headroom above BSS: {manifest['stack_headroom_bytes']:,} bytes")
    print("Largest compiler-reported stack frame: "
          f"{manifest['largest_compiler_reported_frame_bytes']:,} bytes "
          "(call chains not included)")
    if manifest["raw_export"]:
        print(f"Offline raw layout: {args.output / manifest['raw_export']}")
        print(f"Raw SHA-256: {manifest['raw_export_sha256']}")
    print("Diagnostic only; no validated bootable or flashable image was produced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
