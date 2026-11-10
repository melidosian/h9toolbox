#!/bin/sh
# Extract the bundled JieLi toolchain next to this script and print the
# JIELI_TOOLCHAIN path. The build scripts (toolchain/h9cc, doom-firmware/
# build.py) use that folder by default, so after this nothing else is needed.
#   sh firmware-research/toolchain/jieli/setup.sh
set -eu
here=$(cd "$(dirname "$0")" && pwd)
archive="$here/jieli-linux-toolchains-20260730.1.tar.xz"
want=a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd
got=$(shasum -a 256 "$archive" 2>/dev/null || sha256sum "$archive")
[ "${got%% *}" = "$want" ] || { echo "toolchain archive SHA-256 mismatch: ${got%% *}" >&2; exit 1; }
if [ ! -x "$here/jieli-linux-toolchains-20260730.1/pi32v2/bin/clang" ]; then
  tar -xJf "$archive" -C "$here"
fi
echo "$here/jieli-linux-toolchains-20260730.1"
