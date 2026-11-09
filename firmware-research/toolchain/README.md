# JieLi Pi32v2 C toolchain on macOS (Docker)

Compile C for the TD-H9's main CPU (JieLi BR23, Pi32v2 core revision r3)
with JieLi's own Linux toolchain, running in Docker. On Apple Silicon the
x86-64 binaries run under Rosetta through Colima.

The toolchain is JieLi's proprietary software: it is not in this repository
or in the Docker image. `h9cc` mounts your extracted copy read-only.

## Setup (once)

1. Download JieLi's Linux toolchain from their tools page
   (<https://doc.zh-jieli.com/Tools/zh-cn/other_info/index.html>, "Linux
   版", currently 2.5.3; <https://pkgman.jieliapp.com/s/linux-toolchain>).
   Tested: `jieli-linux-toolchains-20260730.1.tar.xz`, 26,027,508 bytes,
   SHA-256 `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`
   (its MD5 matches the server ETag `EB6B1A89…`).
2. Extract it anywhere outside the repo and point `JIELI_TOOLCHAIN` at the
   extracted `jieli-linux-toolchains-*` folder.
3. Start a Docker engine. On Apple Silicon:
   `brew install colima && colima start --vm-type vz --vz-rosetta`.
4. Build the runtime image: `docker build -t h9-jieli firmware-research/toolchain`.

## Use

```
export JIELI_TOOLCHAIN=~/path/to/jieli-linux-toolchains-20260730.1
firmware-research/toolchain/h9cc clang -target pi32v2 -mcpu=r3 -Oz -integrated-as -c file.c -o file.o
firmware-research/toolchain/h9cc objdump -d file.o
```

`h9cc <tool>` runs `/opt/jieli/pi32v2/bin/<tool>` in the container with the
current directory mounted at `/work` (so paths must be inside it). The
toolchain reports `clang version 4.0.1`, the same compiler generation used
for the earlier Windows builds in `replacement-source/`.

## Compatibility (checked 2026-11-08)

- [`examples/gradient.c`](examples/gradient.c) compiles to 66 bytes of
  Pi32v2 code (ELF machine `0xF1`).
- The repository's own Pi32v2 tools (`brick-breaker/tools/pi32v2-isa`) read
  the object identically and link it at a fixed RAM address (`0x29F00`, the
  proposed loader slot), so C modules and hand-written assembly can be built
  together. Nothing compiled this way has run on a radio yet.
