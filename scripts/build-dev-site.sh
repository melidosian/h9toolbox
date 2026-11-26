#!/usr/bin/env bash
set -euo pipefail

bash scripts/build-site.sh

# Offer the experimental 33 cm build only on Dev.
band_release=firmware-research/module-loader/releases/0.1.8
(cd "$band_release" && sha256sum -c SHA256SUMS)
cp "$band_release"/h9-toolbox-{firmware-v0.1.8*.fw,v0.1.8*-far.bin} dist/firmware-update/dev/
mkdir -p dist/apps/catalog/0.1.8
cp "$band_release"/apps/* dist/apps/catalog/0.1.8/

# The experimental APRS flasher is published only by the dev build.
mkdir -p dist/aprs-update
cp prototypes/aprs-update/{index.html,aprs.css,aprs.js,protocol.mjs} dist/aprs-update/
cp firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.bin dist/aprs-update/

# Keep the explicit older dev installer and catalog available.
dev_release=firmware-research/module-loader/smartbeacon-release
cp "$dev_release"/{SHA256SUMS,h9-toolbox-aprs-scheduler.bin,h9-toolbox-aprs-stage.bin,h9-toolbox-dev-apps.bin} dist/firmware-update/dev/
rm -f dist/firmware-update/smartbeacon/index.html
sed -e 's|data-next-firmware>|data-next-firmware data-legacy-dev-firmware data-33cm-firmware>|' \
  -e 's|v0\.1\.7|v0.1.8|g' \
  -e 's|<!-- dev-before-install -->|<p class="fine">If you have an armed APRS schedule, disarm it on the radio before entering update mode. An armed schedule can resume on restart.</p>|' \
  -e 's|H9 Toolbox Firmware also stages the optional APRS modem update if space is available.|The updater verifies the APRS scheduler and stages the optional modem update when space is available.|' \
  dist/firmware-update/index.html > dist/firmware-update/index.html.tmp
mv dist/firmware-update/index.html.tmp dist/firmware-update/index.html

test -s dist/apps/catalog/smartbeacon/catalog.json
sed 's|data-next-catalog class=|data-next-catalog data-legacy-dev-catalog data-33cm-catalog class=|' \
  dist/apps/index.html > dist/apps/index.html.tmp
mv dist/apps/index.html.tmp dist/apps/index.html

sed -e 's|v0\.1\.7|v0.1.8|g' -e 's|V0\.1\.7|V0.1.8|g' \
  -e 's|catalog=0\.1\.7|catalog=0.1.8|g' \
  dist/firmware/index.html > dist/firmware/index.html.tmp
mv dist/firmware/index.html.tmp dist/firmware/index.html

# Keep development URLs out of search results while allowing crawlers to see the header.
printf '/*\n  X-Robots-Tag: noindex\n/dev-build.txt\n  Cache-Control: no-store\n' > dist/_headers
printf 'User-agent: *\nAllow: /\n' > dist/robots.txt
rm -f dist/sitemap.xml

commit_sha=${CF_PAGES_COMMIT_SHA:-$(git rev-parse HEAD)}
printf 'dev %s\n' "${commit_sha}" > dist/dev-build.txt
