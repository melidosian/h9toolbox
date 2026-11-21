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

sed -e 's|id="optBuild">|id="optBuild" data-release-firmware>|' \
  -e 's|H9 Toolbox Firmware v0.1.5|H9 Toolbox Firmware v0.1.6|g' \
  -e 's|Load H9 Toolbox v0.1.5|Load H9 Toolbox v0.1.6|g' \
  -e 's|hold Back from the channel screen to open the app menu|press Menu from the channel screen to open Toolbox|' \
  -e 's|hold <strong>Back</strong> to open the app menu|press <strong>Menu</strong> to open Toolbox|' \
  dist/firmware-update/index.html > dist/firmware-update/index.html.tmp
mv dist/firmware-update/index.html.tmp dist/firmware-update/index.html

# Publish the v0.1.6 firmware and its matching app catalog.
preview_release=firmware-research/module-loader/combined-preview-release
(cd "$preview_release" && sha256sum -c SHA256SUMS)
cp "$preview_release"/{h9-toolbox-firmware-v0.1.6-rc.5.fw,h9-toolbox-v0.1.6-rc.5-far.bin} dist/firmware-update/dev/
mkdir -p dist/apps/catalog/rc5
cp "$preview_release"/apps/* dist/apps/catalog/rc5/

sed -e 's|<h1 id="firmware-title">.*</h1>|<h1 id="firmware-title">H9 Toolbox Firmware v0.1.6</h1>|' \
  -e 's|opened by holding Back|opened by pressing Menu|' \
  -e 's|hold <strong>Back</strong> to open Toolbox|press <strong>Menu</strong> to open Toolbox|' \
  -e 's|Hold <strong>Back</strong> on the channel screen, then select an app with <strong>Menu</strong>.|Press <strong>Menu</strong> on the channel screen, then choose an app.|' \
  -e 's|Use the optional APRS modem update in v0.1.5|Use the included optional APRS modem stage|' \
  -e 's|H9 Toolbox Firmware v0.1.5 offers|H9 Toolbox Firmware v0.1.6 includes|' \
  -e 's|after the v0.1.5 updater confirms that the modem update was staged|after the installer verifies both firmware regions|' \
  -e 's|href="/apps/">App Manager|href="/apps/?catalog=0.1.6">App Manager|g' \
  dist/firmware/index.html > dist/firmware/index.html.tmp
mv dist/firmware/index.html.tmp dist/firmware/index.html

# Select the matching catalog at the normal App Manager URL.
test -s dist/apps/catalog/rc5/catalog.json
sed -e 's|id="catalog" class=|id="catalog" data-release-catalog class=|' \
  -e 's|hold <b>Back</b> on the radio for the menu|press <b>Menu</b> on the radio with v0.1.6|' \
  dist/apps/index.html > dist/apps/index.html.tmp
mv dist/apps/index.html.tmp dist/apps/index.html
