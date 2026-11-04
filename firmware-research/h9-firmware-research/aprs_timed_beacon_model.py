#!/usr/bin/env python3
"""Readable model of the stock TD-H9 v1.0.38 timed-beacon tick.

Reconstructed from app+0x082EB2. ``calls_tx_selector`` means control reaches
app+0x05C2D4; it does not promise a packet or RF transmission. The caller's
other receive/busy guards, scheduler period, and hardware are outside this
model. No radio or firmware image is accessed.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json


@dataclass(frozen=True)
class BeaconTick:
    next_counter: int
    calls_tx_selector: bool
    restores_receive_if_selector_succeeds: bool


def tick_timed_beacon(
    enabled: bool,
    interval: int,
    counter: int,
    state_11b1e: int,
    ptt_priority: int,
) -> BeaconTick:
    """Model RAM C513/C514-C515, 11CEC, 11B1E, and C534 reads.

    The interval is multiplied by 100 in stock code. The tick duration is not
    yet established. ``state_11b1e`` retains its raw name until its meaning is
    independently verified.
    """
    if not 0 <= interval <= 0xFFFF:
        raise ValueError("interval must be a 16-bit unsigned value")
    if not 0 <= counter <= 0xFFFFFFFF:
        raise ValueError("counter must be a 32-bit unsigned value")
    if not 0 <= state_11b1e <= 0xFF or not 0 <= ptt_priority <= 0xFF:
        raise ValueError("state and priority must be bytes")
    if not enabled:
        return BeaconTick(0, False, False)
    if interval == 0:
        return BeaconTick(counter, False, False)

    next_counter = (counter + 1) & 0xFFFFFFFF
    if next_counter < interval * 100:
        return BeaconTick(next_counter, False, False)

    selector_allowed = state_11b1e != 1 or ptt_priority != 0
    return BeaconTick(0, selector_allowed, selector_allowed and state_11b1e != 1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--enabled", action="store_true", help="Timed Beacon setting")
    parser.add_argument("--interval", type=int, required=True)
    parser.add_argument("--counter", type=int, required=True)
    parser.add_argument("--state-11b1e", type=int, required=True)
    parser.add_argument("--ptt-priority", type=int, required=True)
    args = parser.parse_args()
    decision = tick_timed_beacon(args.enabled, args.interval, args.counter,
                                 args.state_11b1e, args.ptt_priority)
    print(json.dumps(asdict(decision), indent=2))


if __name__ == "__main__":
    main()
