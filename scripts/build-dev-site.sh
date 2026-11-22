#!/usr/bin/env bash
set -euo pipefail

bash scripts/build-site.sh

# The experimental APRS flasher is published only by the dev build.
mkdir -p dist/aprs-update
cp prototypes/aprs-update/{index.html,aprs.css,aprs.js,protocol.mjs} dist/aprs-update/
cp firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.bin dist/aprs-update/

# Publish the next release and its matching app catalog on dev.
release=firmware-research/module-loader/releases/0.1.7
(cd "$release" && sha256sum -c SHA256SUMS)
cp "$release"/{h9-toolbox-firmware-v0.1.7.fw,h9-toolbox-v0.1.7-far.bin} dist/firmware-update/dev/
mkdir -p dist/apps/catalog/0.1.7
cp "$release"/apps/* dist/apps/catalog/0.1.7/

# Keep the explicit older dev installer and catalog available.
dev_release=firmware-research/module-loader/smartbeacon-release
cp "$dev_release"/{SHA256SUMS,h9-toolbox-aprs-scheduler.bin,h9-toolbox-aprs-stage.bin,h9-toolbox-dev-apps.bin} dist/firmware-update/dev/
rm -f dist/firmware-update/smartbeacon/index.html
sed -e 's|data-release-firmware>|data-release-firmware data-next-firmware data-legacy-dev-firmware>|' \
  -e 's|H9 Toolbox Firmware v0.1.6|H9 Toolbox Firmware v0.1.7|g' \
  -e 's|Load H9 Toolbox v0.1.6|Load H9 Toolbox v0.1.7|g' \
  -e 's|Return to the v0.1.6 installer|Return to the v0.1.7 installer|' \
  -e 's|<!-- dev-before-install -->|<p class="fine">If you have an armed APRS schedule, disarm it on the radio before entering update mode. An armed schedule can resume on restart.</p>|' \
  -e 's|H9 Toolbox Firmware also stages the optional APRS modem update if space is available.|The updater verifies the APRS scheduler and stages the optional modem update when space is available.|' \
  dist/firmware-update/index.html > dist/firmware-update/index.html.tmp
mv dist/firmware-update/index.html.tmp dist/firmware-update/index.html

test -s dist/apps/catalog/smartbeacon/catalog.json
sed -e 's|data-release-catalog class=|data-release-catalog data-next-catalog data-legacy-dev-catalog class=|' \
  -e 's|on the radio with v0.1.6|on the radio with v0.1.7|' \
  -e 's|<!-- dev-catalog-links -->|<p class="note"><a href="?catalog=smartbeacon">v0.1.5 development catalog</a> · <a href="?catalog=rc5">v0.1.6 catalog</a></p>|' \
  dist/apps/index.html > dist/apps/index.html.tmp
mv dist/apps/index.html.tmp dist/apps/index.html

# Link the release announcement directly to the staged installer.
sed -e 's|<h1 id="firmware-title">.*</h1>|<h1 id="firmware-title">H9 Toolbox Firmware v0.1.7</h1>|' \
  -e 's|href="/firmware-update/"|href="/firmware-update/?release=0.1.7"|g' \
  -e 's|href="/apps/?catalog=0.1.6"|href="/apps/"|g' \
  dist/firmware/index.html > dist/firmware/index.html.tmp
mv dist/firmware/index.html.tmp dist/firmware/index.html

# Keep development URLs out of search results while allowing crawlers to see the header.
printf '/*\n  X-Robots-Tag: noindex\n/dev-build.txt\n  Cache-Control: no-store\n' > dist/_headers
printf 'User-agent: *\nAllow: /\n' > dist/robots.txt
rm -f dist/sitemap.xml

commit_sha=${CF_PAGES_COMMIT_SHA:-$(git rev-parse HEAD)}
printf 'dev %s\n' "${commit_sha}" > dist/dev-build.txt
