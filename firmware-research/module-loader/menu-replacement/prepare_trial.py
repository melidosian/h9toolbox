"""Verify a unified build and join its two far payloads for the USB flasher."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import build


def verify(directory, stock_package):
    sys.path.insert(0, str(ROOT.parent / "h9-flasher"))
    from h9_flasher import checked_custom_firmware

    manifest = json.loads((directory / "h9_toolbox_modules-manifest.json").read_text())
    firmware = manifest["firmware"]
    package = directory / "h9_toolbox_modules_OFFLINE_EXPERIMENT.fw"
    parsed = checked_custom_firmware(package, stock_package)
    if (parsed.package_sha256, parsed.image_sha256) != (firmware["package_sha256"], firmware["flash_sha256"]):
        raise ValueError("Firmware does not match the build manifest")
    coverage = manifest["native_menu_coverage"]
    retired = coverage.get("retired_rf_sms_rows", 0)
    if coverage["active_rows"] != 110 or retired not in (0, 5) or coverage["implemented_rows"] + retired != 110:
        raise ValueError("The build does not account for all factory menu rows")
    menu = manifest["far_menu"]
    resident = firmware["resident_beacon"]
    bundle = (directory / "h9_modules.bin").read_bytes()
    modules = build.parse_bundle(bundle)
    beacon = next(module for module in modules if module["name"] == "APRS BEACON")
    if (sha256(bundle).hexdigest() != manifest["bundle"]["sha256"]
            or build.crc16(beacon["body"]) != int(resident["screen_crc16"], 16)):
        raise ValueError("Module bundle does not match the firmware")
    far = (directory / "menu_0xEA000_OFFLINE.bin").read_bytes()
    resident_bytes = (directory / "resident_0xFA000_OFFLINE.bin").read_bytes()
    if (len(far) != menu["bytes"] or sha256(far).hexdigest() != menu["sha256"]
            or build.crc16(far) != int(firmware["unified_menu"]["far_crc16"], 16)):
        raise ValueError("Far menu does not match the firmware")
    if (len(resident_bytes) != resident["body_bytes"] or sha256(resident_bytes).hexdigest() != resident["sha256"]
            or build.crc16(resident_bytes) != int(resident["crc16"], 16)):
        raise ValueError("Resident APRS payload does not match the firmware")
    gap = build.RESIDENT_TX_FLASH - build.FAR_MENU_FLASH
    if (int(menu["flash_offset"], 16) != build.FAR_MENU_FLASH
            or int(resident["flash_offset"], 16) != build.RESIDENT_TX_FLASH
            or len(far) > gap or len(resident_bytes) > 0x2000):
        raise ValueError("Far payload layout exceeds its reserved window")
    return far.ljust(gap, b"\xff") + resident_bytes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--stock-base", required=True, type=Path)
    args = parser.parse_args()
    data = verify(args.directory, args.stock_base)
    path = args.directory / "unified-far-0xEA000.bin"
    path.write_bytes(data)
    print(f"Verified package, module bundle and both far payloads. Combined far SHA-256 {sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
