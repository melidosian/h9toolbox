#!/usr/bin/env bash
set -euo pipefail

# Match the production site's KISS TNC route.
mkdir -p dist/kiss-tnc
cp prototypes/kiss-tnc/{index.html,style.css,app.mjs,kiss.mjs,tnc.mjs} dist/kiss-tnc/

# The experimental APRS flasher is published only by the dev build.
mkdir -p dist/aprs-update
cp prototypes/aprs-update/{index.html,aprs.css,aprs.js,protocol.mjs} dist/aprs-update/
cp firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.bin dist/aprs-update/

# Publish the dev build of H9 Toolbox Firmware and its matching files.
dev_release=firmware-research/module-loader/smartbeacon-release
(cd "$dev_release" && sha256sum -c SHA256SUMS)
mkdir -p dist/firmware-update/dev
cp "$dev_release"/{SHA256SUMS,h9-toolbox-firmware-v0.1.5.fw,h9-toolbox-earlier-dev.fw,h9-toolbox-dev-before-aprs.fw,h9-toolbox-aprs-scheduler.bin,h9-toolbox-aprs-stage.bin,h9-toolbox-dev-apps.bin} dist/firmware-update/dev/
rm -f dist/firmware-update/smartbeacon/index.html
sed -e 's|id="optBuild"|id="optBuild" data-dev-firmware|' \
  -e 's|Includes the app loader, Bluetooth packet support, and an optional APRS modem update.|Includes the Toolbox app loader, Bluetooth packet support, SmartBeacon, timed APRS beacons, and APRS Mute.|' \
  -e 's|<!-- dev-before-install -->|<p class="fine">If you have an armed APRS schedule, disarm it on the radio before entering update mode. An armed schedule can resume on restart.</p>|' \
  -e 's|H9 Toolbox Firmware also stages the optional APRS modem update if space is available.|The updater verifies the APRS scheduler and stages the optional modem update when space is available.|' \
  dist/firmware-update/index.html > dist/firmware-update/index.html.tmp
mv dist/firmware-update/index.html.tmp dist/firmware-update/index.html

# Publish the combined preview and its matching app catalog on dev only.
preview_release=firmware-research/module-loader/combined-preview-release
(cd "$preview_release" && sha256sum -c SHA256SUMS)
cp "$preview_release"/{h9-toolbox-firmware-v0.1.6-rc.5.fw,h9-toolbox-v0.1.6-rc.5-far.bin} dist/firmware-update/dev/
mkdir -p dist/apps/catalog/rc5
cp "$preview_release"/apps/* dist/apps/catalog/rc5/

# Stage the next release and its catalog on the development site.
release=firmware-research/module-loader/releases/0.1.7
(cd "$release" && sha256sum -c SHA256SUMS)
cp "$release"/{h9-toolbox-firmware-v0.1.7.fw,h9-toolbox-v0.1.7-far.bin} dist/firmware-update/dev/
mkdir -p dist/apps/catalog/0.1.7
cp "$release"/apps/* dist/apps/catalog/0.1.7/

sed -e 's|<h1 id="firmware-title">.*</h1>|<h1 id="firmware-title">H9 Toolbox Firmware v0.1.7</h1>|' \
  -e 's|hold <strong>Back</strong> to open Toolbox|press <strong>Menu</strong> to open Toolbox|' \
  -e 's|Hold <strong>Back</strong> on the channel screen, then select an app with <strong>Menu</strong>.|Press <strong>Menu</strong> on the channel screen, then choose an app. Hold <strong>Back</strong> for Quick Change, or hold <strong>#</strong> to start scanning.|' \
  -e 's|Use the optional APRS modem update in v0.1.5|Use the included optional APRS modem stage|' \
  -e 's|H9 Toolbox Firmware v0.1.5 offers|H9 Toolbox Firmware v0.1.7 includes|' \
  -e 's|after the v0.1.5 updater confirms that the modem update was staged|after the installer verifies both firmware regions|' \
  -e 's|href="/apps/">App Manager|href="/apps/?catalog=0.1.7">App Manager|g' \
  dist/firmware/index.html > dist/firmware/index.html.tmp
mv dist/firmware/index.html.tmp dist/firmware/index.html

# Offer the catalog built for the dev firmware.
test -s dist/apps/catalog/smartbeacon/catalog.json
test -s dist/apps/catalog/rc5/catalog.json
test -s dist/apps/catalog/0.1.7/catalog.json
sed 's|<!-- dev-smartbeacon-catalog -->|<p class="note" id="smartbeaconCatalogLink"><a href="?catalog=smartbeacon">v0.1.5 development catalog</a> · <a href="?catalog=rc5">v0.1.6 preview catalog</a> · <a href="?catalog=0.1.7">v0.1.7 catalog</a>.</p>|' \
  dist/apps/index.html > dist/apps/index.html.tmp
mv dist/apps/index.html.tmp dist/apps/index.html

# Keep development URLs out of search results while allowing crawlers to see the header.
printf '/*\n  X-Robots-Tag: noindex\n/dev-build.txt\n  Cache-Control: no-store\n' > dist/_headers
printf 'User-agent: *\nAllow: /\n' > dist/robots.txt
rm -f dist/sitemap.xml

commit_sha=${CF_PAGES_COMMIT_SHA:-$(git rev-parse HEAD)}
printf 'dev %s\n' "${commit_sha}" > dist/dev-build.txt
