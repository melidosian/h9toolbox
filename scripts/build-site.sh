#!/usr/bin/env bash
set -euo pipefail

# Publish the KISS TNC test page.
mkdir -p dist/kiss-tnc
cp prototypes/kiss-tnc/{index.html,style.css,app.mjs,kiss.mjs,tnc.mjs} dist/kiss-tnc/

# The updater recognizes earlier dev images as backup and upgrade sources.
older_release=firmware-research/module-loader/smartbeacon-release
(cd "$older_release" && sha256sum -c SHA256SUMS)
mkdir -p dist/firmware-update/dev
cp "$older_release"/{h9-toolbox-firmware-v0.1.5.fw,h9-toolbox-earlier-dev.fw,h9-toolbox-dev-before-aprs.fw} dist/firmware-update/dev/

python3 - <<'PY'
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

source = Path("firmware-research/h9-flasher")
files = ["README.md", "h9_flasher.py", "vendor/LICENSE.jl-uboot-tool", "vendor/data/br23loader.bin"]
paths = [source / name for name in files] + sorted((source / "vendor").rglob("*.py"))
with ZipFile("dist/firmware-update/h9-desktop-backup.zip", "w", compression=ZIP_DEFLATED) as archive:
    for path in paths:
        archive.write(path, Path("h9-desktop-backup") / path.relative_to(source))
PY

sed -e 's|id="optBuild">|id="optBuild" data-release-firmware data-next-firmware>|' \
  -e 's|H9 Toolbox Firmware v0.1.5|H9 Toolbox Firmware v0.1.7|g' \
  -e 's|Load H9 Toolbox v0.1.5|Load H9 Toolbox v0.1.7|g' \
  -e 's|hold Back from the channel screen to open the app menu|press Menu from the channel screen to open Toolbox|' \
  -e 's|hold <strong>Back</strong> to open the app menu|press <strong>Menu</strong> to open Toolbox|' \
  dist/firmware-update/index.html > dist/firmware-update/index.html.tmp
mv dist/firmware-update/index.html.tmp dist/firmware-update/index.html

# Publish the v0.1.6 firmware and its matching app catalog.
preview_release=firmware-research/module-loader/combined-preview-release
(cd "$preview_release" && sha256sum -c SHA256SUMS)
cp "$preview_release"/{h9-toolbox-firmware-v0.1.6-rc.{4,5}.fw,h9-toolbox-v0.1.6-rc.{4,5}-far.bin} dist/firmware-update/dev/
mkdir -p dist/apps/catalog/rc5
cp "$preview_release"/apps/* dist/apps/catalog/rc5/

# Offer the Quick Change release and its matched app catalog.
quick_release=firmware-research/module-loader/releases/0.1.7
(cd "$quick_release" && sha256sum -c SHA256SUMS)
cp "$quick_release"/h9-toolbox-{firmware-v0.1.7*.fw,v0.1.7*-far.bin} dist/firmware-update/dev/
mkdir -p dist/apps/catalog/0.1.7
cp "$quick_release"/apps/* dist/apps/catalog/0.1.7/

# Select the matching catalog at the normal App Manager URL.
test -s dist/apps/catalog/0.1.7/catalog.json
sed -e 's|id="catalog" class=|id="catalog" data-release-catalog data-next-catalog class=|' \
  -e 's|hold <b>Back</b> on the radio for the menu|press <b>Menu</b> on the radio with v0.1.7|' \
  dist/apps/index.html > dist/apps/index.html.tmp
mv dist/apps/index.html.tmp dist/apps/index.html
