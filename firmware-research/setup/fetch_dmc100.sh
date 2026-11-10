#!/bin/sh
# Fetch DMC100-Doom (Artur Bieniek, GPL-2.0-or-later) at the commit the TD-H9
# port is built against, into firmware-research/setup/dmc100, and apply the
# TD-H9 pack patch (adds the E1M1H / E1M1S pack settings to tools/conv).
#   sh firmware-research/setup/fetch_dmc100.sh
set -eu
here=$(cd "$(dirname "$0")" && pwd)
dest="$here/dmc100"
commit=0cdc78eb66cdddacc43112d5f79c258c4a5440b7
patch="$here/../doom-firmware/patches/dmc100-e1m1h-pack.patch"
if [ ! -d "$dest/.git" ]; then
  git clone --quiet https://github.com/ArturBieniek4/DMC100-Doom.git "$dest"
fi
git -C "$dest" fetch --quiet origin "$commit" 2>/dev/null || true
git -C "$dest" checkout --quiet "$commit"
[ "$(git -C "$dest" rev-parse HEAD)" = "$commit" ] || { echo "DMC100 is not at $commit" >&2; exit 1; }
if git -C "$dest" apply --check "$patch" 2>/dev/null; then
  git -C "$dest" apply "$patch"
fi
echo "$dest"
