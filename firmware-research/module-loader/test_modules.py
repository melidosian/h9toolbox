#!/usr/bin/env python3
"""Host tests for module and resident-service C code. Uses gcc in the
Docker image h9-hostcc (debian:bookworm-slim + gcc) and runs them under
UBSan (including array bounds). ASan is left out: on this Docker kernel it
loops on DEADLYSIGNAL before main, and turning ASLR off is not permitted in
the container.

    py -3 -I test_modules.py [--native]
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent
APRS = "modules/aprs"
COMMON = "modules/common"
BEACON = [f"{APRS}/beacon_core.c", f"{APRS}/third_party/smartbeacon/smartbeacon.c"]
EXTRA = {
    "test_beacon.c": [*BEACON, f"{APRS}/aprs_tx.c"],
    "test_beacon_screen.c": BEACON,
    "test_beacon_service.c": [*BEACON, "resident-service/beacon_service.c", "resident-service/dispatch.c",
                              "resident-service/modem_input.c"],
    "test_beacon_tx.c": [f"{APRS}/beacon_core.c", f"{APRS}/aprs_tx.c"],
    "test_toolbox_settings_pair.c": [f"{COMMON}/toolbox_settings_pair.c"],
    "test_sattrack.c": ["modules/satpass/orbit.c"],
}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", action="store_true", help="use the host C compiler")
    args = parser.parse_args()
    tests = sorted([*ROOT.glob("modules/*/test_*.c"), *ROOT.glob("resident-service/test_*.c")])
    failed = 0
    for test in tests:
        rel = test.relative_to(ROOT).as_posix()
        flags = ["-std=c11", "-O1", "-Wall", "-Wextra", "-Wno-int-to-pointer-cast",
                 "-fsanitize=undefined", "-fno-sanitize-recover=undefined",
                 f"-I{COMMON}", f"-I{test.parent.relative_to(ROOT).as_posix()}", rel,
                 *EXTRA.get(test.name, [])]
        if args.native:
            with tempfile.TemporaryDirectory() as folder:
                binary = str(Path(folder) / "test")
                r = subprocess.run(["cc", *flags, "-o", binary], cwd=ROOT, capture_output=True, text=True)
                if not r.returncode:
                    r = subprocess.run([binary], cwd=ROOT, timeout=60, capture_output=True, text=True)
        else:
            command = shlex.join(["gcc", *flags, "-o", "/tmp/t"]) + " && timeout 60 /tmp/t"
            r = subprocess.run(["docker", "run", "--rm", "-v", f"{ROOT}:/src", "-w", "/src",
                                "h9-hostcc", "sh", "-c", command], capture_output=True, text=True)
        print(f"{rel}: {(r.stdout + r.stderr).strip()}")
        failed += r.returncode != 0
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
