"""Read-only map of the separate TD-H9 APRS controller firmware image.

The image is a headerless Cortex-M flash region, not the JieLi main-radio app.
This script identifies structural evidence only; it does not infer function names
or modify the image. Optionally install Capstone 5 to print selected Thumb code.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


FLASH_BASE = 0x08002000
VECTOR_BYTES = 0xC0
KNOWN_SHA256 = "1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33"
PERIPHERALS = {
    0x40004400: "USART2-like register base",
    0x40010800: "GPIOA-like register base",
    0x40012400: "ADC1-like register base",
    0x40012C00: "TIM1-like register base",
    0x40013800: "USART1-like register base",
    0x40020000: "DMA1-like register base",
    0x40021000: "RCC-like register base",
}


def map_image(data: bytes) -> dict:
    digest = hashlib.sha256(data).hexdigest()
    if digest != KNOWN_SHA256:
        raise ValueError(f"Unexpected APRS image SHA-256: {digest}")
    if len(data) < VECTOR_BYTES:
        raise ValueError("Image is too small for the observed vector table")
    vectors = []
    for offset in range(0, VECTOR_BYTES, 4):
        value = struct.unpack_from("<I", data, offset)[0]
        if value and offset != 0 and not (FLASH_BASE <= value - 1 < FLASH_BASE + len(data)):
            raise ValueError(f"Vector at {offset:#x} points outside the image: {value:#x}")
        vectors.append({"offset": f"0x{offset:02x}", "value": f"0x{value:08x}"})
    if not 0x20000000 <= int(vectors[0]["value"], 16) < 0x20020000:
        raise ValueError("Initial stack pointer is outside the expected SRAM range")
    if int(vectors[1]["value"], 16) & 1 == 0:
        raise ValueError("Reset vector does not select Thumb state")

    peripheral_literals = []
    for offset in range(VECTOR_BYTES, len(data) - 3, 4):
        value = struct.unpack_from("<I", data, offset)[0]
        if value in PERIPHERALS:
            peripheral_literals.append({
                "offset": f"0x{offset:04x}",
                "address": f"0x{FLASH_BASE + offset:08x}",
                "value": f"0x{value:08x}",
                "inferred_register_family": PERIPHERALS[value],
            })

    strings = [
        {"offset": f"0x{match.start():04x}", "text": match.group().decode("ascii")}
        for match in re.finditer(rb"[ -~]{7,}", data)
        if b"APRS" in match.group() or b"Version" in match.group() or
        re.fullmatch(rb"V\d+(?:\.\d+)+", match.group())
    ]
    return {
        "image_bytes": len(data),
        "sha256": digest,
        "inferred_flash_base": f"0x{FLASH_BASE:08x}",
        "initial_stack_pointer": vectors[0]["value"],
        "reset_handler": vectors[1]["value"],
        "vectors": vectors,
        "peripheral_literals": peripheral_literals,
        "selected_strings": strings,
        "limitations": [
            "The base address is inferred from vector targets; no device or linker map was supplied.",
            "Peripheral names are inferences from STM32-style address layout, not confirmed device identification.",
            "Literal occurrence is not proof that a peripheral is active or that a specific function uses it.",
            "Neither decoder behavior nor speaker-audio routing is established by this structural map.",
        ],
    }


def disassemble(data: bytes, address: int, length: int) -> list[str]:
    try:
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB
    except ImportError as exc:
        raise SystemExit("Capstone 5 is needed for --disasm; install it separately") from exc
    offset = address - FLASH_BASE
    if offset < 0 or offset + length > len(data):
        raise ValueError("Requested disassembly range is outside the image")
    decoder = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
    return [f"{ins.address:08x}: {ins.mnemonic:8} {ins.op_str}" for ins in
            decoder.disasm(data[offset:offset + length], address)]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--output", type=Path, help="New JSON report path")
    parser.add_argument("--disasm", type=lambda s: int(s, 0), help="Thumb address to disassemble")
    parser.add_argument("--length", type=lambda s: int(s, 0), default=0x80)
    args = parser.parse_args()
    data = args.image.read_bytes()
    result = map_image(data)
    if args.output:
        with args.output.open("x", encoding="utf-8") as stream:
            json.dump(result, stream, indent=2)
            stream.write("\n")
    else:
        print(json.dumps(result, indent=2))
    if args.disasm is not None:
        print("\n".join(disassemble(data, args.disasm, args.length)))


if __name__ == "__main__":
    main()
