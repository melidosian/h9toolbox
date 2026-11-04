#!/usr/bin/env python3
"""Readable model of one TD-H9 v1.0.38 APRS TX-channel decision.

Reconstructed from app.bin at app+0x05C2D4 and the channel setup at
app+0x058D3E. This predicts which channel setup the code requests; it does
not measure the transmitted RF frequency or the display's red-line behavior.
It starts after the function's other eligibility checks (including APRS state)
and models only its five-case selector. Both the PTT Linkage wrapper and the
timed-beacon scheduler can call this selector; see aprs_timed_beacon_model.py.
It does not model those callers' other guards or RF hardware.
No hardware or firmware image is accessed.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
from typing import Literal


Side = Literal["A", "B"]

MODES = (
    "CHA-Lock",
    "CHB-Lock",
    "CHA-Auto",
    "CHB-Auto",
    "CHA+CHB",
)


@dataclass(frozen=True)
class Route:
    mode: str
    current_side: Side
    dual_watch: bool
    allowed: bool
    requested_side: Side | None
    channel_setup: Side | None
    temporarily_changes_current_side: bool


def route_aprs_tx(mode: int, current_side: Side, dual_watch: bool) -> Route:
    """Model the mode switch, Lock guards, and A/B channel setup selection.

    ``current_side`` is bit 7 of RAM byte 0x00011B50 before the function runs.
    The function's selected side is stored at 0x00011AEC. When Dual Watch is
    on, app+0x058D3E routes from that selected side; otherwise it routes from
    bit 7 of 0x00011B50. Auto modes temporarily change the bit and restore it.
    """
    if not 0 <= mode < len(MODES):
        raise ValueError("APRS TX CH must be 0..4")
    if current_side not in ("A", "B"):
        raise ValueError("current_side must be A or B")

    selected: Side = current_side if mode == 4 else ("B" if mode in (1, 3) else "A")
    allowed = mode not in (0, 1) or selected == current_side
    if not allowed:
        return Route(MODES[mode], current_side, dual_watch, False, None, None, False)

    temporary = mode in (2, 3) and selected != current_side
    # Lock modes already match the current side. Auto modes write the chosen
    # side to both the selected byte and current-side bit before setup.
    setup: Side = selected if dual_watch or temporary else current_side
    return Route(MODES[mode], current_side, dual_watch, True, selected, setup, temporary)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", type=int, choices=range(5), required=True,
                        help="APRS TX CH: 0=A-Lock, 1=B-Lock, 2=A-Auto, 3=B-Auto, 4=A+B")
    parser.add_argument("--current-side", choices=("A", "B"), required=True)
    parser.add_argument("--dual-watch", action="store_true")
    args = parser.parse_args()
    print(json.dumps(asdict(route_aprs_tx(args.mode, args.current_side,
                                          args.dual_watch)), indent=2))


if __name__ == "__main__":
    main()
