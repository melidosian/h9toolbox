# JieLi toolchain (bundled)

`jieli-linux-toolchains-20260730.1.tar.xz` is JieLi's Linux toolchain for
their Pi32/Pi32v2/Q32S chips (clang 4.0.1 for `-target pi32v2 -mcpu=r3`,
GNU ld 2.26, binutils), unchanged from JieLi's own download server:

- Source: `https://jl-update.oss-cn-shenzhen.aliyuncs.com/jieli-linux-toolchains-20260730.1.tar.xz`
- SHA-256: `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd`
- 26,027,508 bytes; extracts to about 135 MB (`common/`, `pi32/`, `pi32v2/`, `q32s/`)

It is included so builds do not depend on finding it. It is JieLi's
software, not H9 Toolbox's; it will be removed if JieLi asks.

## Use

```sh
sh firmware-research/toolchain/jieli/setup.sh   # checks the hash, extracts here
```

`toolchain/h9cc` and `doom-firmware/build.py` then find it without
`JIELI_TOOLCHAIN`; set that variable to use a different copy. The tools are
Linux x86-64 binaries: on a Mac they run in the `h9-jieli` Docker image
(`toolchain/Dockerfile`; Colima with Rosetta works).
