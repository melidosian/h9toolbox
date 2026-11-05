#!/usr/bin/env python3
"""Report direct-call stack paths in the diagnostic APRS ELF.

This is an incomplete estimate: selected board callbacks are mapped, but
other indirect calls, multiple interrupt nesting, and compiler helper
functions without .su records are not resolved. A value below available
RAM is therefore not proof of stack safety.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess


FUNCTION = re.compile(r"^[0-9a-fA-F]+ <([^>]+)>:$")
DIRECT_CALL = re.compile(r"\b(?:bl|blx)\s+[0-9a-fA-F]+\s+<([^>]+)>")
INDIRECT_CALL = re.compile(r"\bblx\s+(?:r\d+|ip|lr)\b")
ROOTS = (
    "h9_aprs_boot", "h9_aprs_board_main", "h9_aprs_systick_handler",
    "h9_aprs_dma1_channel1_handler", "h9_aprs_dma1_channel3_handler",
    "h9_aprs_adc1_handler", "h9_aprs_usart1_handler",
    "h9_aprs_usart2_handler",
)
THREAD_ROOTS = ("h9_aprs_boot", "h9_aprs_board_main")
IRQ_ROOTS = tuple(root for root in ROOTS if root not in THREAD_ROOTS)
# Arm Cortex-M basic exception entry stacks eight words. Allow one more
# word for alignment padding. See Arm's "How much stack memory do I need
# for my Arm Cortex-M applications?":
# https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/how-much-stack-memory-do-i-need-for-my-arm-cortex--m-applications
EXCEPTION_FRAME_BYTES = 36
KNOWN_BOARD_CALLBACKS = {
    # These bindings are explicit in aprs/h9_aprs_board_runtime.c and the
    # controller/demod source. Generic callbacks can have other targets.
    "stream_byte": ("board_uart_write",),
    "h9_wire_encode_stream": ("board_uart_write",),
    "send_frame": ("board_uart_write", "board_uart_ok"),
    "h9_aprs_controller_uart_byte": (
        "board_submit_packet", "board_set_radio_gpio", "board_read_radio_gpio",
        "board_aux_uart_write", "board_read_status",
    ),
    "h9_aprs_controller_adc_tick": (
        "board_read_adc", "board_read_wave_port", "board_write_wave_port",
        "board_tx_wave_active", "board_next_tx_wave",
    ),
    "h9_aprs_bitstream_feed": ("emit_received_byte",),
    "h9_aprs_frame_feed": ("forward_valid_frame",),
    "h9_aprs_uart_receive_idle": ("h9_aprs_uart1_dma_rearm",),
    "h9_aprs_uart_receive_process": ("h9_aprs_uart1_dma_rearm",),
    "h9_aprs_boot_banner_send": ("board_boot_write",),
}


def analyze(disassembly: str, usage_records: list[dict],
            headroom: int) -> dict:
    frames = {record["function"]: record["frame_bytes"]
              for record in usage_records}
    calls: dict[str, set[str]] = {}
    indirect: dict[str, int] = {}
    current = None
    for line in disassembly.splitlines():
        if match := FUNCTION.match(line):
            current = match.group(1)
            calls.setdefault(current, set())
            indirect.setdefault(current, 0)
            continue
        if current is None:
            continue
        if match := DIRECT_CALL.search(line):
            target = match.group(1)
            if "+0x" not in target:
                calls[current].add(target)
        elif INDIRECT_CALL.search(line):
            indirect[current] += 1

    missing_frames: set[str] = set()
    cycles: set[str] = set()

    def longest(name: str, visiting: frozenset[str],
                with_callbacks: bool) -> tuple[int, list[str]]:
        if name in visiting:
            cycles.add(name)
            return 0, []
        if name not in frames:
            missing_frames.add(name)
        own = frames.get(name, 0)
        best = (0, [])
        targets = set(calls.get(name, ()))
        if with_callbacks:
            targets.update(KNOWN_BOARD_CALLBACKS.get(name, ()))
        for target in targets:
            candidate = longest(target, visiting | {name}, with_callbacks)
            if candidate[0] > best[0]:
                best = candidate
        return own + best[0], [name, *best[1]]

    def paths_for(with_callbacks: bool) -> list[dict]:
        paths = []
        for root in ROOTS:
            total, functions = longest(root, frozenset(), with_callbacks)
            paths.append({"root": root, "frame_bytes": total,
                          "path": functions})
        paths.sort(key=lambda item: item["frame_bytes"], reverse=True)
        return paths

    direct_paths = paths_for(False)
    callback_paths = paths_for(True)
    callback_by_root = {path["root"]: path for path in callback_paths}
    thread_path = max((callback_by_root[root] for root in THREAD_ROOTS),
                      key=lambda path: path["frame_bytes"])
    irq_path = max((callback_by_root[root] for root in IRQ_ROOTS),
                   key=lambda path: path["frame_bytes"])
    aligned_thread_bytes = (thread_path["frame_bytes"] + 7) & ~7
    one_irq_bytes = (aligned_thread_bytes + EXCEPTION_FRAME_BYTES +
                     irq_path["frame_bytes"])
    return {
        "stack_headroom_bytes": headroom,
        "direct_call_paths": direct_paths,
        "largest_direct_path_bytes": direct_paths[0]["frame_bytes"],
        "known_callback_paths": callback_paths,
        "largest_known_callback_path_bytes": callback_paths[0]["frame_bytes"],
        "largest_known_path_exceeds_headroom":
            callback_paths[0]["frame_bytes"] > headroom,
        "one_irq_preemption": {
            "thread_root": thread_path["root"],
            "thread_path_bytes": thread_path["frame_bytes"],
            "thread_path_aligned_bytes": aligned_thread_bytes,
            "irq_root": irq_path["root"],
            "irq_path_bytes": irq_path["frame_bytes"],
            "exception_frame_bytes": EXCEPTION_FRAME_BYTES,
            "total_bytes": one_irq_bytes,
            "exceeds_headroom": one_irq_bytes > headroom,
        },
        "indirect_call_sites": {name: count for name, count in indirect.items()
                                if count},
        "missing_frame_records": sorted(missing_frames),
        "direct_call_cycles": sorted(cycles),
        "stack_safety_proven": False,
        "limitation": "Unknown callbacks, multiple interrupt nesting, and missing frames are excluded",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--objdump", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    disassembly = subprocess.run(
        [args.objdump, "-d", str(args.elf)], check=True, text=True,
        capture_output=True,
    ).stdout
    report = analyze(disassembly, manifest["stack_usage_records"],
                     manifest["stack_headroom_bytes"])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Longest direct-call path: {report['largest_direct_path_bytes']} bytes"
          f" / {report['stack_headroom_bytes']} available")
    print("Longest path including known board callbacks: "
          f"{report['largest_known_callback_path_bytes']} bytes")
    irq = report["one_irq_preemption"]
    print(f"Main path plus one known interrupt: {irq['total_bytes']} bytes "
          f"({irq['thread_root']} + {irq['irq_root']} + "
          f"{irq['exception_frame_bytes']} exception bytes)")
    print(f"Indirect call sites (some board targets mapped): "
          f"{sum(report['indirect_call_sites'].values())}; "
          f"missing frame records: {len(report['missing_frame_records'])}")
    print("Stack safety not proven: indirect calls and multiple interrupts remain")
    return 1 if (report["largest_known_path_exceeds_headroom"] or
                 irq["exceeds_headroom"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
