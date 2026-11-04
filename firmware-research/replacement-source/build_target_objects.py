#!/usr/bin/env python3
"""Cross-compile the current replacement source into target object files.

This deliberately produces no firmware image. Neither target has complete
startup, board drivers, or a verified linker script yet.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT = Path(__file__).resolve().parent
SOURCES = {
    "aprs": (
        "common/h9_aprs_wire.c",
        "aprs/h9_aprs_adc_board.c",
        "aprs/h9_aprs_bitstream.c",
        "aprs/h9_aprs_boot_banner.c",
        "aprs/h9_aprs_clock.c",
        "aprs/h9_aprs_controller.c",
        "aprs/h9_aprs_demod.c",
        "aprs/h9_aprs_frame.c",
        "aprs/h9_aprs_gpio_board.c",
        "aprs/h9_aprs_oscillator.c",
        "aprs/h9_aprs_ring.c",
        "aprs/h9_aprs_runtime_status.c",
        "aprs/h9_aprs_sample_timer.c",
        "aprs/h9_aprs_status.c",
        "aprs/h9_aprs_systick.c",
        "aprs/h9_aprs_tx.c",
        "aprs/h9_aprs_tx_gate.c",
        "aprs/h9_aprs_tx_packet.c",
        "aprs/h9_aprs_tx_runtime.c",
        "aprs/h9_aprs_tx_service.c",
        "aprs/h9_aprs_uart1_board.c",
        "aprs/h9_aprs_uart1_dma.c",
        "aprs/h9_aprs_uart1_tx.c",
        "aprs/h9_aprs_uart2_board.c",
        "aprs/h9_aprs_uart2_maintenance.c",
        "aprs/h9_aprs_uart2_dma.c",
        "aprs/h9_aprs_uart2_rx.c",
        "aprs/h9_aprs_uart_receive.c",
        "aprs/h9_aprs_wave_output.c",
    ),
    "main": (
        "common/h9_aprs_wire.c",
        "main/h9_main_aprs.c",
        "main/h9_main_aprs_control.c",
        "main/h9_main_aprs_metadata.c",
        "main/h9_main_aprs_receive.c",
        "main/h9_main_aprs_session.c",
        "main/h9_main_aprs_uart1_link.c",
        "main/h9_main_aprs_wire.c",
        "main/h9_main_ax25.c",
        "main/h9_main_beacon.c",
        "main/h9_main_gps_fix.c",
        "main/h9_main_audio_arbiter.c",
        "main/h9_main_board_markers.c",
        "main/h9_main_clock_probe.c",
        "main/h9_main_clock_apply.c",
        "main/h9_main_clock_rom_mode.c",
        "main/h9_main_clock_timing.c",
        "main/h9_main_heap_bitmap.c",
        "main/h9_main_memory_window.c",
        "main/h9_main_post_clock_rom.c",
        "main/h9_main_rf_output.c",
        "main/h9_main_rf_state.c",
        "main/h9_main_speaker_gate.c",
        "main/h9_main_rf_registers.c",
        "main/h9_main_boot_record.c",
        "main/h9_main_boot_validator.c",
        "main/h9_main_serial_profile.c",
        "main/h9_main_serial_bus.c",
        "main/h9_main_serial_commands.c",
        "main/h9_main_serial_gate.c",
        "main/h9_main_serial_port.c",
        "main/h9_main_serial_dma.c",
        "main/h9_main_uart1_irq.c",
        "main/h9_main_uart1_board.c",
        "main/h9_main_uart1_baud.c",
        "main/h9_main_uart1_tx.c",
        "main/h9_main_uart1_tx_queue.c",
        "main/h9_main_tx_service.c",
        "main/h9_main_tx_frequency.c",
        "main/h9_main_uart_ring.c",
    ),
}
FLAGS = {
    "aprs": ("-mcpu=cortex-m0", "-mthumb", "-Os"),
    "main": ("-target", "pi32v2", "-mcpu=r3", "-flto", "-Oz"),
}


def find_compiler(target: str, specified: str | None) -> str:
    if target == "aprs":
        default = "arm-none-eabi-gcc"
    elif os.name == "nt":
        default = "C:/JL/pi32/bin/clang.exe"
    else:
        default = "/opt/jieli/pi32v2/bin/clang"
    candidate = specified or os.environ.get(
        "H9_ARM_GCC" if target == "aprs" else "H9_PI32_CLANG"
    ) or default
    path = shutil.which(candidate)
    if path is None:
        raise ValueError(f"Compiler not found: {candidate}")
    return path


def run(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}{result.stderr}"
        )
    return result.stdout.strip()


def find_archiver(target: str, compiler: str) -> str:
    name = ("arm-none-eabi-ar" if target == "aprs" else
            ("llvm-ar" if os.name == "nt" else "lto-ar"))
    sibling = Path(compiler).parent / (name + (".exe" if os.name == "nt" else ""))
    path = shutil.which(str(sibling)) or shutil.which(name)
    if path is None:
        raise ValueError(f"Archiver not found: {name}")
    return path


def verify_arm_object(data: bytes, source: str) -> None:
    if len(data) < 20 or data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ValueError(f"{source}: expected little-endian ELF32 object")
    if int.from_bytes(data[18:20], "little") != 40:
        raise ValueError(f"{source}: object is not for ARM")


def verify_main_object(data: bytes, source: str, lto: bool) -> None:
    if lto:
        if not data.startswith(b"BC\xc0\xde"):
            raise ValueError(f"{source}: expected LLVM bitcode for Pi32 LTO")
        return
    if len(data) < 20 or data[:6] != b"\x7fELF\x01\x01":
        raise ValueError(f"{source}: expected little-endian ELF32 object")
    if int.from_bytes(data[18:20], "little") != 0xF1:
        raise ValueError(f"{source}: object is not for Pi32v2")


def build(target: str, compiler: str, output: Path, no_lto: bool = False,
          stack_usage: bool = False) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    target_flags = [flag for flag in FLAGS[target]
                    if not (no_lto and flag == "-flto")]
    common_flags = [
        "-std=c11", "-ffreestanding", "-fno-builtin", "-Wall", "-Wextra",
        "-Werror", *target_flags,
        *(f"-I{ROOT / directory}" for directory in ("common", "aprs", "main")),
    ]
    if stack_usage:
        if target != "aprs":
            raise ValueError("Stack-usage reports are supported for APRS only")
        common_flags.append("-fstack-usage")
    if target == "main":
        compiler_root = Path(compiler).resolve().parent.parent
        for include_dir in (compiler_root / "pi32v2-include",
                            compiler_root / "include"):
            if include_dir.is_dir():
                common_flags.append(f"-I{include_dir}")
                break
    version = run([compiler, "--version"]).splitlines()[0]
    objects = []
    object_paths = []
    for source_name in SOURCES[target]:
        source = ROOT / source_name
        object_path = output / (source.parent.name + "_" + source.stem + ".o")
        run([compiler, *common_flags, "-c", str(source), "-o", str(object_path)])
        data = object_path.read_bytes()
        if target == "aprs":
            verify_arm_object(data, source_name)
        else:
            verify_main_object(data, source_name, not no_lto)
        objects.append({
            "source": source_name,
            "source_sha256": sha256(source.read_bytes()).hexdigest(),
            "object": object_path.name,
            "object_bytes": len(data),
            "object_sha256": sha256(data).hexdigest(),
        })
        object_paths.append(object_path)
    archiver = find_archiver(target, compiler)
    archive_path = output / f"libh9_{target}_replacement.a"
    run([archiver, "rcs", str(archive_path),
         *(str(path) for path in object_paths)])
    archive = archive_path.read_bytes()
    if not archive.startswith(b"!<arch>\n"):
        raise ValueError("Archiver did not produce a Unix archive")
    manifest = {
        "target": target,
        "object_format": "ELF32-pi32v2" if no_lto else
                         ("LLVM bitcode" if target == "main" else "ELF32-ARM"),
        "compiler": compiler,
        "compiler_version": version,
        "flags": common_flags,
        "objects": objects,
        "archive": archive_path.name,
        "archive_sha256": sha256(archive).hexdigest(),
        "firmware_image_created": False,
        "flashable": False,
    }
    (output / "build-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target", choices=SOURCES)
    parser.add_argument("--compiler", help="target compiler executable path")
    parser.add_argument("--output", type=Path, help="object output directory")
    parser.add_argument("--no-lto", action="store_true",
                        help="main only: emit native Pi32v2 ELF objects")
    args = parser.parse_args()
    try:
        if args.no_lto and args.target != "main":
            raise ValueError("--no-lto is only supported for the main target")
        compiler = find_compiler(args.target, args.compiler)
        directory = "target-main-native" if args.no_lto else f"target-{args.target}"
        output = args.output or ROOT / "build" / directory
        manifest = build(args.target, compiler, output, args.no_lto)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Build failed: {error}", file=sys.stderr)
        return 1
    print(f"Compiled {len(manifest['objects'])} {args.target} object files and "
          f"{manifest['archive']} in {output}")
    print("No linked or flashable firmware image was produced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
