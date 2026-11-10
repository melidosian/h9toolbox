#!/usr/bin/env python3
"""Retired remote bootstrap. Use the reviewed local install instructions."""

from __future__ import annotations

import sys


def main() -> int:
    print(
        "The remote Brick Breaker bootstrap has been retired. "
        "Review firmware-research/brick-breaker/README.md, check out the "
        "pinned source revision, and run cli.py as a local file.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
