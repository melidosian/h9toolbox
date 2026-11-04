#!/usr/bin/env python3
"""Read APRS settings from saved TD-H9 clone images without changing the radio.

Offsets and enum order follow the CHIRP TD-H9 test driver. Saved images include
the eight-byte TDH9 identity prefix. A file is a historical snapshot; this
tool cannot establish the radio's current settings or RF transmit frequency.
"""

from __future__ import annotations

import argparse
from hashlib import sha256
from pathlib import Path
import sys


IMAGE_SIZE = 8 + 0x3140
RX_CHANNELS = ("Off", "CH-A", "CH-B")
TX_CHANNELS = ("CHA-Lock", "CHB-Lock", "CHA-Auto", "CHB-Auto", "CHA+CHB")
FORWARD_CHANNELS = ("CH-A", "CH-B", "CHA+CHB")
PTT_PRIORITIES = ("Call", "APRS")
MODE_BYTES = {ord("N"): "Normal", ord("H"): "Ham", ord("G"): "GMRS"}


def choice(value: int, names: tuple[str, ...]) -> str:
    return names[value] if value < len(names) else f"Unknown ({value})"


def enabled(value: int) -> str:
    return ("Off", "On")[value] if value < 2 else f"Unknown ({value})"


def frequency_mhz(encoded: bytes) -> str:
    if any((byte >> 4) > 9 or (byte & 0xF) > 9 for byte in encoded):
        return "Unknown"
    ten_hz = sum(((byte >> 4) * 10 + (byte & 0xF)) * (100 ** index)
                 for index, byte in enumerate(encoded))
    return f"{ten_hz * 10 / 1_000_000:.5f}"


def inspect(path: Path) -> dict[str, str | int]:
    image = path.read_bytes()
    if len(image) != IMAGE_SIZE:
        raise ValueError(f"Expected {IMAGE_SIZE} bytes; got {len(image)}")
    if image[:4] != b"TDH9" or image[4:7] != b"\xff\xff\xff" or image[7] not in MODE_BYTES:
        raise ValueError("Not a recognized TD-H9 clone image")
    return {
        "file": str(path),
        "sha256": sha256(image).hexdigest(),
        "mode": MODE_BYTES[image[7]],
        "saved_vfo_a_rx_mhz": frequency_mhz(image[0x1958:0x195C]),
        "saved_vfo_a_tx_mhz": frequency_mhz(image[0x195C:0x1960]),
        "saved_vfo_b_rx_mhz": frequency_mhz(image[0x1968:0x196C]),
        "saved_vfo_b_tx_mhz": frequency_mhz(image[0x196C:0x1970]),
        "aprs_switch": enabled(image[0x3084]),
        "aprs_rx_ch": choice(image[0x3116], RX_CHANNELS),
        "aprs_tx_ch": choice(image[0x3117], TX_CHANNELS),
        "aprs_tx_ch_byte": image[0x3117],
        "ptt_priority": choice(image[0x3118], PTT_PRIORITIES),
        "ptt_linkage": enabled(image[0x30F6]),
        "timed_beacon": enabled(image[0x30F7]),
        "beacon_interval_seconds": image[0x30F8] | (image[0x30F9] << 8),
        "digi_forward_ch": choice(image[0x30FA], FORWARD_CHANNELS),
        "digi1_enable": enabled(image[0x30FB]),
        "digi2_enable": enabled(image[0x3104]),
        "rx_auto_popup": enabled(image[0x311B]),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", nargs="+", type=Path)
    args = parser.parse_args()
    failed = False
    for path in args.images:
        try:
            result = inspect(path)
        except (OSError, ValueError) as error:
            print(f"{path}: {error}", file=sys.stderr)
            failed = True
            continue
        print(f"\n{result['file']} (SHA-256 {result['sha256']})")
        for key, value in result.items():
            if key not in ("file", "sha256"):
                print(f"  {key}: {value}")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
