#!/usr/bin/env python3
"""Host tests for module C code: compiles modules/*/test_*.c with gcc in the
Docker image h9-hostcc (debian:bookworm-slim + gcc) and runs them under
UBSan (including array bounds). ASan is left out: on this Docker kernel it
loops on DEADLYSIGNAL before main, and turning ASLR off is not permitted in
the container.

    py -3 -I test_modules.py
"""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


def main() -> int:
    tests = sorted((ROOT / "modules").glob("*/test_*.c"))
    failed = 0
    for test in tests:
        rel = test.relative_to(ROOT).as_posix()
        cmd = (f"gcc -std=c11 -O1 -Wall -Wextra -Wno-int-to-pointer-cast -fsanitize=undefined "
               f"-fno-sanitize-recover=undefined -Imodules/common -I{test.parent.relative_to(ROOT).as_posix()} "
               f"-o /tmp/t {rel} && timeout 60 /tmp/t")
        r = subprocess.run(["docker", "run", "--rm", "-v", f"{ROOT}:/src", "-w", "/src", "h9-hostcc", "sh", "-c", cmd],
                           capture_output=True, text=True)
        print(f"{rel}: {(r.stdout + r.stderr).strip()}")
        failed += r.returncode != 0
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
