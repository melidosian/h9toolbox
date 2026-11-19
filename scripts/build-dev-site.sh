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
cp "$dev_release"/{SHA256SUMS,h9-toolbox-firmware-v0.1.5.fw,h9-toolbox-earlier-dev.fw,h9-toolbox-aprs-scheduler.bin,h9-toolbox-dev-apps.bin} dist/firmware-update/dev/
rm -f dist/firmware-update/smartbeacon/index.html
sed -e 's|id="optBuild"|id="optBuild" data-dev-firmware|' \
  -e 's|Includes the app loader, Bluetooth packet support, and an optional APRS modem update.|Includes the Toolbox app loader, Bluetooth packet support, SmartBeacon, timed APRS beacons, and APRS Mute.|' \
  -e 's|<!-- dev-before-install -->|<p class="fine">If you have an armed APRS schedule, disarm it on the radio before entering update mode. An armed schedule can resume on restart.</p>|' \
  -e 's|<!-- dev-skip-backup-note -->|<p class="fine">With Skip backup selected, the updater can install H9 Toolbox on an unfamiliar build after verifying the TD-H9 model data. It writes only main firmware sectors that differ from the selected package.</p>|' \
  -e 's|H9 Toolbox Firmware also stages the optional APRS modem update if space is available.|The updater verifies the APRS scheduler before installing the main firmware.|' \
  dist/firmware-update/index.html > dist/firmware-update/index.html.tmp
mv dist/firmware-update/index.html.tmp dist/firmware-update/index.html

# Offer the catalog built for the dev firmware.
test -s dist/apps/catalog/smartbeacon/catalog.json
sed 's|<!-- dev-smartbeacon-catalog -->|<p class="note" id="smartbeaconCatalogLink"><a href="?catalog=smartbeacon">Development app catalog</a> · For the dev build of H9 Toolbox Firmware v0.1.5.</p>|' \
  dist/apps/index.html > dist/apps/index.html.tmp
mv dist/apps/index.html.tmp dist/apps/index.html

# Keep development URLs out of search results while allowing crawlers to see the header.
printf '/*\n  X-Robots-Tag: noindex\n/dev-build.txt\n  Cache-Control: no-store\n' > dist/_headers
printf 'User-agent: *\nAllow: /\n' > dist/robots.txt
rm -f dist/sitemap.xml

commit_sha=${CF_PAGES_COMMIT_SHA:-$(git rev-parse HEAD)}
printf 'dev %s\n' "${commit_sha}" > dist/dev-build.txt
