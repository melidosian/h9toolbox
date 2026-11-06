#!/usr/bin/env python3
"""Create a source-only ZIP with the exact Brick Breaker build and recovery tools."""

from __future__ import annotations

from hashlib import sha256
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parent
OUTPUT = ROOT / "dist" / "td-h9-brick-breaker-source.zip"
FILES = (
    [ROOT / name for name in (".gitignore", "README.md", "bootstrap.py", "cli.py",
                               "hid_install.py", "build.py", "trial.ps1", "package_source.py")]
    + sorted((ROOT / "src").glob("*.s"))
    + sorted((ROOT / "src").glob("*.ld"))
    + sorted((ROOT / "tests").glob("*.py"))
    + [ROOT / "tools" / "h9-flasher" / "h9_flasher.py"]
    + [ROOT / "tools" / "h9-flasher" / "vendor" / "LICENSE.jl-uboot-tool"]
    + [ROOT / "tools" / "h9-flasher" / "vendor" / "data" / "br23loader.bin"]
    + sorted((ROOT / "tools" / "h9-flasher" / "vendor" / "jltech").glob("*.py"))
    + sorted((ROOT / "tools" / "h9-flasher" / "vendor" / "scsiio").glob("*.py"))
    + sorted((ROOT / "tools" / "h9-firmware-research").glob("*.py"))
    + [ROOT / "tools" / "pi32v2-isa" / "LICENSE"]
    + [ROOT / "tools" / "pi32v2-isa" / "isa" / "pi32v2.toml"]
    + sorted((ROOT / "tools" / "pi32v2-isa" / "pi32v2isa").glob("*.py"))
)


def main() -> None:
    if not all(path.is_file() for path in FILES):
        missing = [str(path.relative_to(ROOT)) for path in FILES if not path.is_file()]
        raise ValueError(f"Source package files are missing: {missing}")
    OUTPUT.parent.mkdir(exist_ok=True)
    lines: list[str] = []
    with ZipFile(OUTPUT, "w") as archive:
        for path in FILES:
            name = path.relative_to(ROOT).as_posix()
            content = path.read_bytes()
            lines.append(f"{sha256(content).hexdigest()}  {name}")
            info = ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            archive.writestr(info, content)
        info = ZipInfo("SHA256SUMS.txt", (1980, 1, 1, 0, 0, 0))
        info.compress_type = ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        archive.writestr(info, "\n".join(lines) + "\n")
    print(f"{OUTPUT}: {len(FILES)} source/tool files; SHA-256 {sha256(OUTPUT.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
