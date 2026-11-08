#!/usr/bin/env python3
"""Build the stock-derived TD-H9 resource-flash service packages offline."""

from __future__ import annotations

import argparse
from hashlib import sha256
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
TOOLS = ROOT.parent / "brick-breaker" / "tools"
RESEARCH = TOOLS / "h9-firmware-research"
FLASHER = TOOLS / "h9-flasher"
ISA = TOOLS / "pi32v2-isa"
IMAGE_BASE = 0x01E00100
STOCK_IMAGE_SIZE = 0xD3C2C
STOCK_SHA256 = "11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3"
STOCK_PACKAGE_SHA256 = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
# Erased code caves in the stock app: (ELF section, app offset, size).
CAVE_OFFSET = 0xA77F0
CAVE_SIZE = 1704
CAVE2_OFFSET = 0xA9D9E
CAVE2_SIZE = 1004
MAIN_CAVE = (".text", CAVE_OFFSET, CAVE_SIZE)
WRITE_CAVE = (".s2", CAVE2_OFFSET, CAVE2_SIZE)
# The "caves" are the white margins of the three 70 x 70 User Manual QR codes
# (app+0xA2EB2, 0xA54FA, 0xA7B42). Code there shows as noise in the margins;
# the codes still scan (checked on hardware 2026-11-08).
# Merged Brick Breaker + S2: the game keeps 0xA77F0 (1,628 of 1,704 bytes) and
# the launcher the start of 0xA9D9E (94 bytes). The service takes what is
# left: S2 the QR A margin, the compact reader the rest of 0xA9D9E, its byte
# tables the tail of 0xA77F0.
BRICK_BREAKER_APP_SHA256 = "5c4593451c4aa05f975329e687c91b9957134e7252afbfad0975ffaaeb5d8193"
BRICK_BREAKER_PACKAGE_SHA256 = "208409d8d7e335904f70f9e28e27e9c413c6296a3d185a69a55707ac8ddaee68"
MERGED_WRITE_CAVE = (".s2", 0xA51A8, 857)
MERGED_TABLES = (".tables", 0xA7E50, 0xA77F0 + CAVE_SIZE - 0xA7E50)
MERGED_READER = (".svc", 0xA9E00, CAVE2_OFFSET + CAVE2_SIZE - 0xA9E00)

# Stock BLE clone 'R' loop: r1 = r5 + r6; r2 = sp + 3; r0 = 4; r3 = 1;
# call 0x01E52CB2. Only the four call bytes at app+0x05D018 change.
READ_HOOK = {"context": (0x05D010, "d11d 8a83 4024 4321 bfeacbad"),
             "call_at": 0x05D018}
# Stock BLE clone 'W' callback: r7 = [r4+12]; r1 = r5 + 4; r2 = 32; r0 = r6;
# call r7. The last two instructions (4 bytes) become one call; the service
# entry performs r0 = r6 itself.
WRITE_HOOK = {"context": (0x05D11C, "4763 5984 4a20 6016 c700"),
              "call_at": 0x05D122}

STAGES = {
    "s1": {
        "sources": ("resource_service.s",),
        "hooks": ((READ_HOOK, "resource_service_read"),),
        "ram": (range(0x1E4D8, 0x1E4E8), range(0x1E4FC, 0x1E500)),
        "linker": "resource_service.ld",
        "caves": (MAIN_CAVE,),
        "sectors": (0x5000, 0x62000, 0xAC000, 0xD9000),
        "name": "h9_resource_service_s1",
        "kind": "stock_derived_resource_flash_read_service",
        "stage": "S1 read-only",
    },
    "s2": {
        "sources": ("resource_service.s", "resource_write.s"),
        "hooks": ((READ_HOOK, "resource_s2_read"), (WRITE_HOOK, "resource_s2_write")),
        "ram": (range(0x1E4D8, 0x1E500),),
        # The S1 code fills most of the first cave; the write module lives in
        # the second, which starts at flash 0xAEE9E and spans sectors 0xAE000
        # and 0xAF000.
        "linker": "resource_service_s2.ld",
        "caves": (MAIN_CAVE, WRITE_CAVE),
        "sectors": (0x5000, 0x62000, 0xAC000, 0xAE000, 0xAF000, 0xD9000),
        "name": "h9_resource_service_s2",
        "kind": "stock_derived_resource_flash_read_write_service",
        "stage": "S2 guarded sector erase and program",
    },
    "bb-s2": {
        "sources": ("resource_service.s", "resource_write.s"),
        "defines": ("COMPACT",),
        "base": "brick-breaker",
        "hooks": ((READ_HOOK, "resource_s2_read"), (WRITE_HOOK, "resource_s2_write")),
        "ram": (range(0x1E4D8, 0x1E500),),
        "linker": "resource_service_bb.ld",
        "caves": (MERGED_WRITE_CAVE, MERGED_TABLES, MERGED_READER),
        "sectors": (0x5000, 0x62000, 0x8D000, 0xAA000, 0xAC000, 0xAE000, 0xAF000, 0xD9000),
        "name": "h9_toolbox_firmware",
        "kind": "stock_derived_brick_breaker_with_resource_flash_service",
        "stage": "Brick Breaker (unchanged) + S2 guarded sector erase and program",
    },
}

# Stock routines the services call, pinned by their first instruction bytes.
STOCK_ENTRIES = {
    0x01E52CB2: "7a04 44e0b203",        # partition reader (selector, off, buf, len)
    0x01E001BA: "0116 4020 b1ec38c0",   # GPIO pin-bank lookup
    0x01E4FF06: "7404 c4ff081e1e00",    # SPI 2 byte exchange
    0x01E4FF22: "7804 2416 0615",       # NOR read (buf, chip address, len)
    0x01E50000: "7604 0416 8dea168b",   # NOR 4 KiB sector erase (sector index)
    0x01E5006E: "7c04 2b16 1c16 0816",  # NOR page program (src, chip address, len)
    0x01E50286: "1004 e29f 4020",       # BLE TX capacity (ble_user_cmd_prepare class 9)
    0x01E5D2E0: "7604 e29e c3ffd01a0100",  # BLE notify submit (handle, buf, len)
    0x01E16652: "6204 7804 e298",          # ble_user_cmd_prepare (class 11 = conn param update)
    0x01E50316: "1004 c0ffd01a0100 0944",  # stock connection-parameter request (table 0x01EA2A38)
}
# The stock clone reply is sent on handle 12: r0 = 12 at 0x01E5DD44, then
# 0x01E5D2E0(r0, r5 = 0x127B0, r4 = length) at 0x01E5DDD0..0x01E5DDD7.
REPLY_HANDLE = ((0x01E5DD44, "402c 2484"), (0x01E5DDD0, "5116 4216 bfea84fa"))
# Seven 20-byte partition records: name, origin, length, flags, selector.
PARTITION_TABLE = 0x01EB53F0
PARTITIONS = ((0x000000, 0x100000), (0x100000, 0x300000), (0x400000, 0x400000),
              (0x800000, 0x100000), (0x900000, 0x100000), (0xA00000, 0x300000),
              (0xD00000, 0x300000))
# Stock write-protect mask in 0x01E4FEF4: selectors 1, 2, and 5 (38 = 0b100110).
WRITE_PROTECT = (0x01E4FEF4, bytes.fromhex("4926 c8e11200"))
FLASH_READY = (0x01E885C8, bytes.fromhex("40e0b203 d8ed6800 41e0a55a"))


def check_stock(stock: bytes, ram: tuple[range, ...], caves=(MAIN_CAVE,)) -> None:
    if len(stock) != STOCK_IMAGE_SIZE or sha256(stock).hexdigest() != STOCK_SHA256:
        raise ValueError("Expected the exact official v1.0.38 main application")
    for hook in (READ_HOOK, WRITE_HOOK):
        start, context = hook["context"]
        if stock[start:start + len(bytes.fromhex(context))] != bytes.fromhex(context):
            raise ValueError(f"Stock clone call site at app+0x{start:X} differs")
    for _, offset, size in caves:
        if stock[offset:offset + size] != b"\xff" * size:
            raise ValueError(f"Code cave at app+0x{offset:X} is not erased in the stock app")
    for address, prefix in STOCK_ENTRIES.items():
        if not stock[address - IMAGE_BASE:].startswith(bytes.fromhex(prefix)):
            raise ValueError(f"Stock routine at 0x{address:08X} differs")
    for address, text in REPLY_HANDLE:
        if not stock[address - IMAGE_BASE:].startswith(bytes.fromhex(text)):
            raise ValueError(f"Stock clone reply path at 0x{address:08X} differs")
    for address, expected in (FLASH_READY, WRITE_PROTECT):
        if stock[address - IMAGE_BASE:address - IMAGE_BASE + len(expected)] != expected:
            raise ValueError(f"Stock check at 0x{address:08X} differs")
    for index, (origin, length) in enumerate(PARTITIONS):
        record = stock[PARTITION_TABLE - IMAGE_BASE + 20 * index:][:20]
        _, got_origin, got_length, _, selector = struct.unpack("<5I", record)
        if (got_origin, got_length, selector) != (origin, length, index):
            raise ValueError(f"Stock partition {index} differs")
    # Service state sits in linker padding between .bss and the heap; no
    # aligned literal in the stock image may point at it.
    for word in range(0, len(stock) - 3, 4):
        if any(struct.unpack_from("<I", stock, word)[0] in span for span in ram):
            raise ValueError(f"Stock literal at app+0x{word:X} references service RAM")


def assemble(stage: dict, output: Path) -> tuple[list[tuple[int, bytes]], list[tuple[dict, bytes]], dict]:
    sys.path.insert(0, str(ISA))
    from pi32v2isa import asm, elf, ld  # noqa: E402

    objects = []
    for name in stage["sources"]:
        source = ROOT / "src" / name
        obj = output / f"{Path(name).stem}.o"
        defines = "".join(f".set {name}, 1\n" for name in stage.get("defines", ()))
        obj.write_bytes(asm.assemble_source(defines + source.read_text(encoding="utf-8"),
                                            filename=str(source), include=[str(output)]))
        objects.append(str(obj))
    linked = output / f"{stage['name']}.elf"
    options = ld.parse_args(["-T", str(ROOT / "src" / stage["linker"]),
                             "-o", str(linked), *objects])
    state, elf_bytes = ld.link(options)
    if elf_bytes is None or state.errors:
        raise RuntimeError(f"Pi32 linker failed: {state.errors}")
    linked.write_bytes(elf_bytes)
    program = elf.read(elf_bytes)
    regions = []
    for section, offset, size in stage["caves"]:
        found = next((s for s in program.sections if s.name == section), None)
        if found is None or found.addr != IMAGE_BASE + offset:
            raise ValueError(f"Section {section} is not at the pinned cave app+0x{offset:X}")
        data = bytes(found.data)
        if not data or len(data) > size:
            raise ValueError(f"Section {section} ({len(data)} bytes) does not fit the {size}-byte cave")
        regions.append((offset, data))
    if program.entry != IMAGE_BASE + stage["caves"][0][1]:
        raise ValueError("Linked service entry is not at the first cave")
    symbols = {s.name: s.value for s in program.symbols if s.name}

    patches = []
    for hook, entry in stage["hooks"]:
        target = symbols[entry] - IMAGE_BASE
        call = bytes(next(s for s in elf.read(asm.assemble_source(
            f"call {target - hook['call_at'] - 4}\n")).sections if s.name == ".text").data)
        if len(call) != 4:
            raise ValueError(f"Hook for {entry} did not fit four bytes")
        patches.append((hook, call))
    entries = {entry: hex(symbols[entry]) for _, entry in stage["hooks"]}
    return regions, patches, entries


def brick_breaker_app(stock_image: Path, stock_package: Path, output: Path) -> bytes:
    """The exact hardware-tested Brick Breaker app, built by its own builder."""
    spec = importlib.util.spec_from_file_location(
        "brick_breaker_build", ROOT.parent / "brick-breaker" / "build.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    manifest = module.build(stock_image, stock_package, output)
    if (manifest["app_sha256"] != BRICK_BREAKER_APP_SHA256 or
            manifest["package_sha256"] != BRICK_BREAKER_PACKAGE_SHA256):
        raise ValueError("Brick Breaker base differs from the hardware-tested build")
    return (output / "h9_brick_breaker_hotkey_NOT_FOR_FLASH.raw").read_bytes()


def build(stage_name: str, stock_image: Path, stock_package: Path, output: Path) -> dict:
    stage = STAGES[stage_name]
    stock = stock_image.read_bytes()
    check_stock(stock, stage["ram"], stage["caves"])
    if sha256(stock_package.read_bytes()).hexdigest() != STOCK_PACKAGE_SHA256:
        raise ValueError("Expected the exact official v1.0.38 .fw package")
    output.mkdir(parents=True, exist_ok=True)
    regions, patches, entries = assemble(stage, output)

    base = stock
    if stage.get("base") == "brick-breaker":
        base = brick_breaker_app(stock_image, stock_package, output / "brick-breaker")
        for _, offset, size in stage["caves"]:
            if base[offset:offset + size] != b"\xff" * size:
                raise ValueError(f"Brick Breaker already uses app+0x{offset:X}")
    patched = bytearray(base)
    for hook, call in patches:
        patched[hook["call_at"]:hook["call_at"] + 4] = call
    for offset, data in regions:
        patched[offset:offset + len(data)] = data
    # Only the hook calls and the service bytes may differ from the base.
    allowed = [(hook["call_at"], 4) for hook, _ in patches] + [(o, len(d)) for o, d in regions]
    for index in range(len(base)):
        if patched[index] != base[index] and not any(o <= index < o + n for o, n in allowed):
            raise ValueError(f"Unexpected change at app+0x{index:X}")
    raw = output / f"{stage['name']}_NOT_FOR_FLASH.raw"
    raw.write_bytes(patched)

    package = output / f"{stage['name']}_OFFLINE_EXPERIMENT.fw"
    rebuilt = output / f"{stage['name']}_REBUILD_CHECK.fw"
    destination = rebuilt if package.exists() else package
    if destination.exists():
        raise ValueError(f"Refusing to overwrite package check: {destination}")
    result = subprocess.run(
        [sys.executable, str(RESEARCH / "firmware.py"), "repack", str(stock_package),
         "--app", str(raw), "--output", str(destination)],
        capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"Stock-derived package rebuild failed:\n{result.stdout}{result.stderr}")
    if destination == rebuilt:
        same = rebuilt.read_bytes() == package.read_bytes()
        rebuilt.unlink()
        if not same:
            raise ValueError("Rebuilt package differs from the existing package")

    sys.path.insert(0, str(FLASHER))
    from h9_flasher import parse_firmware  # noqa: E402

    original_flash = parse_firmware(stock_package).image
    target_flash = parse_firmware(package).image
    changed = tuple(offset for offset in range(0, len(original_flash), 4096)
                    if original_flash[offset:offset + 4096] != target_flash[offset:offset + 4096])
    if changed != stage["sectors"]:
        raise ValueError(f"Unexpected changed flash sectors: {[hex(x) for x in changed]}")
    manifest = {
        "kind": stage["kind"],
        "stage": stage["stage"],
        "stock_app_sha256": STOCK_SHA256,
        "base_app_sha256": sha256(base).hexdigest(),
        "hooks": [{"offset": hex(hook["call_at"]),
                   "original_bytes": stock[hook["call_at"]:hook["call_at"] + 4].hex(),
                   "new_bytes": call.hex()} for hook, call in patches],
        "entries": entries,
        "caves": [{"offset": hex(offset), "bytes": len(data), "sha256": sha256(data).hexdigest()}
                  for offset, data in regions],
        "service_ram": [[hex(span.start), hex(span.stop - 1)] for span in stage["ram"]],
        "app_sha256": sha256(patched).hexdigest(),
        "package_sha256": sha256(package.read_bytes()).hexdigest(),
        "flash_sha256": sha256(target_flash).hexdigest(),
        "changed_flash_sectors": [hex(offset) for offset in changed],
        "hardware_observation": "Not yet flashed",
    }
    (output / f"{stage['name']}-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", choices=sorted(STAGES), default="s1")
    parser.add_argument("--stock-image", type=Path, required=True)
    parser.add_argument("--stock-package", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build")
    args = parser.parse_args()
    manifest = build(args.stage, args.stock_image.resolve(), args.stock_package.resolve(),
                     args.output.resolve())
    print(json.dumps(manifest, indent=2))
    print("Offline build only; not hardware tested")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
