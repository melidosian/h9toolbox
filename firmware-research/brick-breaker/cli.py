#!/usr/bin/env python3
"""Guided, pinned TD-H9 Brick Breaker build, install and HID rollback."""

from __future__ import annotations

import argparse
import ctypes
from datetime import datetime
from hashlib import sha256
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
from urllib.request import urlopen
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parent
INPUT = ROOT / "input"
BUILD = ROOT / "build"
BACKUPS = ROOT / "backups"
STOCK_ZIP = INPUT / "TDH9-Type-C-1.0.38.zip"
STOCK_FW = INPUT / "TD-H9-V1.0.38.fw"
TARGET = BUILD / "h9_brick_breaker_hotkey_OFFLINE_EXPERIMENT.fw"
STOCK_URL = ("https://raw.githubusercontent.com/teedubyeah/TDH9/"
             "1ccb67c2684204b0097533486c545607caaf65b8/"
             "Firmware/1.0.38/H9_Update-Type-C_1.0.38.zip")
ZIP_SHA = "1d1b6f87705f026dc5c5b118786d133cbc7f0589f606c8572581df981492155a"
STOCK_SHA = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
TARGET_SHA = "208409d8d7e335904f70f9e28e27e9c413c6296a3d185a69a55707ac8ddaee68"
BACKUP_SHA = "74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648"
ZIP_MEMBER = "H9_Update-Type-C/TD-H9-V1.0.38.fw"
FLASHER = ROOT / "tools/h9-flasher/h9_flasher.py"
FIRMWARE = ROOT / "tools/h9-firmware-research/firmware.py"
RESTORE = ROOT / "tools/h9-firmware-research/restore.py"


def digest(path: Path) -> str:
    return sha256(path.read_bytes()).hexdigest()


def checked(path: Path, expected: str) -> Path:
    if not path.is_file() or digest(path) != expected:
        raise RuntimeError(f"Missing or unexpected SHA-256: {path}")
    return path


def run(*args: object) -> None:
    subprocess.run([sys.executable, *(str(value) for value in args)], check=True)


def obtain_stock(local: Path | None) -> Path:
    INPUT.mkdir(exist_ok=True)
    if local is not None:
        return checked(local.resolve(), STOCK_SHA)
    if not STOCK_FW.exists():
        if not STOCK_ZIP.exists():
            print("Downloading pinned TDH9 Type-C v1.0.38 ZIP...", flush=True)
            with urlopen(STOCK_URL, timeout=90) as response, STOCK_ZIP.open("xb") as output:
                while chunk := response.read(1024 * 1024):
                    output.write(chunk)
        checked(STOCK_ZIP, ZIP_SHA)
        with ZipFile(STOCK_ZIP) as archive:
            data = archive.read(ZIP_MEMBER)
        if sha256(data).hexdigest() != STOCK_SHA:
            raise RuntimeError("Embedded stock firmware hash differs from pinned v1.0.38")
        STOCK_FW.write_bytes(data)
    return checked(STOCK_FW, STOCK_SHA)


def prepare(local: Path | None) -> Path:
    stock = obtain_stock(local)
    app = INPUT / "stock" / "app.bin"
    if not app.exists():
        run(FIRMWARE, "extract", stock, "--output", app.parent)
    BUILD.mkdir(exist_ok=True)
    BACKUPS.mkdir(exist_ok=True)
    run(ROOT / "build.py", "--stock-image", app, "--stock-package", stock)
    checked(TARGET, TARGET_SHA)
    run(FLASHER, "inspect", TARGET, "--stock-base", stock)
    print(f"Exact hardware-tested Brick Breaker package: {TARGET}")
    return stock


def wait_for_mode(mode: str) -> None:
    if mode == "br23":
        print("\nPower off. Connect a USB data cable. Power on, then hold 8 until USB Update appears.")
    else:
        print("\nPower off. Connect a USB data cable. Power on while holding PTT+2.")
        print("The top LED should be solid green in HID recovery mode.")
    input("When the radio is in that mode, press Enter here: ")


def windows_devices() -> list[str]:
    script = ("@(Get-CimInstance Win32_DiskDrive | Where-Object { "
              "$_.Model -eq 'BR23 UBOOT1.00 USB Device' -and "
              "$_.PNPDeviceID -like 'USBSTOR\\DISK&VEN_BR23&PROD_UBOOT1.00*' } | "
              "ForEach-Object { [int]$_.Index }) | ConvertTo-Json -Compress")
    result = subprocess.run(["powershell", "-NoProfile", "-Command", script],
                            check=True, capture_output=True, text=True)
    raw = result.stdout.strip()
    if not raw:
        return []
    indices = json.loads(raw)
    return [r"\\.\PHYSICALDRIVE" + str(i) for i in
            (indices if isinstance(indices, list) else [indices])]


def linux_devices() -> list[str]:
    devices = []
    for entry in Path("/sys/class/scsi_generic").glob("sg*"):
        details = entry / "device"
        try:
            vendor = (details / "vendor").read_text().strip()
            model = (details / "model").read_text().strip()
        except OSError:
            continue
        if (vendor, model) == ("BR23", "UBOOT1.00"):
            devices.append("/dev/" + entry.name)
    return devices


def locate_device(override: str | None) -> str:
    devices = windows_devices() if os.name == "nt" else linux_devices()
    if len(devices) > 1:
        raise RuntimeError(f"Expected one BR23 USB updater, found {devices!r}")
    if override and devices and override != devices[0]:
        raise RuntimeError(f"Specified device {override!r} differs from discovered {devices[0]!r}")
    if not devices and not override:
        hint = r"\\.\PHYSICALDRIVE#" if os.name == "nt" else "/dev/sg#"
        override = input(f"Updater not auto-detected. Enter its {hint} path (or blank to stop): ").strip()
    if not devices and not override:
        raise RuntimeError("No BR23 updater selected")
    device = devices[0] if devices else override
    run(FLASHER, "probe", "--device", device)
    print(f"Verified updater: {device}")
    return device


def admin_windows() -> bool:
    return bool(ctypes.windll.shell32.IsUserAnAdmin())


def elevate_once() -> bool:
    if os.name == "nt":
        if admin_windows():
            return False
        import subprocess as sp
        args = sp.list2cmdline([str(Path(__file__).resolve()), *sys.argv[1:]])
        print("Windows needs one Administrator prompt for the backup and flash.")
        code = ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable,
                                                    args, str(ROOT), 1)
        if code <= 32:
            raise RuntimeError(f"Administrator launch failed (Windows code {code})")
        return True
    if platform.system() == "Linux" and os.geteuid() != 0:
        if not shutil.which("sudo"):
            raise RuntimeError("Linux raw SCSI access needs root; sudo is unavailable")
        print("Linux needs one sudo prompt for the backup and flash.", flush=True)
        subprocess.run(["sudo", sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]],
                       check=True)
        return True
    return False


def default_backup() -> Path:
    BACKUPS.mkdir(exist_ok=True)
    return BACKUPS / ("h9-before-brick-breaker-" + datetime.now().strftime("%Y%m%d-%H%M%S") + ".bin")


def backup_br23(device: str, stock: Path, backup: Path) -> None:
    if backup.exists() or Path(str(backup) + ".json").exists():
        raise RuntimeError(f"Backup path already exists: {backup}")
    backup.parent.mkdir(parents=True, exist_ok=True)
    run(FLASHER, "backup", TARGET, "--stock-base", stock,
        "--device", device, "--backup", backup)
    checked(backup, BACKUP_SHA)
    run(RESTORE, "plan", "--backup", backup, "--package", TARGET)
    print(f"Verified fresh radio backup: {backup}")


def install_br23(stock: Path, device_arg: str | None) -> None:
    wait_for_mode("br23")
    device = locate_device(device_arg)
    backup = default_backup()
    backup_br23(device, stock, backup)
    print("\nThe next step changes five 4 KiB sectors. Keep the backup and manifest.")
    if input("Type FLASH to write Brick Breaker: ").strip() != "FLASH":
        print("No write sent; backup retained.")
        return
    run(FLASHER, "flash", TARGET, "--stock-base", stock,
        "--device", device, "--backup", backup, "--yes")
    print("Flash and full readback verified. Disconnect USB and power on normally.")


def install_hid(stock: Path) -> None:
    wait_for_mode("hid")
    from hid_install import install
    install(stock, TARGET, default_backup())


def restore_hid(backup: Path) -> None:
    checked(TARGET, TARGET_SHA)
    checked(backup, BACKUP_SHA)
    run(RESTORE, "plan", "--backup", backup, "--package", TARGET)
    wait_for_mode("hid")
    run(RESTORE, "inspect", "--backup", backup, "--package", TARGET)
    if input("Type RESTORE to return to stock v1.0.38: ").strip() != "RESTORE":
        print("No write sent.")
        return
    run(RESTORE, "restore", "--backup", backup, "--package", TARGET, "--yes")
    print("Stock main firmware CRCs verified. Disconnect USB and power on normally.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", nargs="?", choices=("install", "build", "restore"),
                        default="install")
    parser.add_argument("--stock", type=Path, help="Existing exact v1.0.38 Type-C .fw")
    parser.add_argument("--backup", type=Path, help="Exact backup from this radio, for restore")
    parser.add_argument("--device", help="BR23 SCSI device; must match auto-detection")
    args = parser.parse_args()
    if sys.version_info < (3, 11):
        parser.error("Python 3.11 or newer is required")
    print(f"TD-H9 Brick Breaker | {platform.system()} | {ROOT}")
    if args.command == "restore":
        backups = sorted(BACKUPS.glob("h9-before-brick-breaker-*.bin"))
        backup = args.backup or (backups[-1] if backups else None)
        if backup is None:
            parser.error("No backup found; pass --backup PATH")
        print(f"Selected backup: {backup}")
        restore_hid(backup.resolve())
        return 0
    stock = prepare(args.stock)
    if args.command == "build":
        return 0
    if platform.system() == "Darwin":
        print("macOS HID install is experimental and has not been hardware-tested on Mac.")
        install_hid(stock)
    else:
        if elevate_once():
            return 0
        install_br23(stock, args.device)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError, ImportError) as exc:
        print(f"Stopped: {exc}", file=sys.stderr)
        raise SystemExit(1)
