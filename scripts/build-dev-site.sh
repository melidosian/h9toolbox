#!/usr/bin/env bash
set -euo pipefail

# Match the production site's KISS TNC route.
mkdir -p dist/kiss-tnc
cp prototypes/kiss-tnc/{index.html,style.css,app.mjs,kiss.mjs,tnc.mjs} dist/kiss-tnc/

# The experimental APRS flasher is published only by the dev build.
mkdir -p dist/aprs-update
cp prototypes/aprs-update/{index.html,aprs.css,aprs.js,protocol.mjs} dist/aprs-update/
cp firmware-research/kiss-tnc/APRS-1.0.15-rx15-rxff.bin dist/aprs-update/
sed 's|<!-- dev-aprs-link -->|<p><a href="/aprs-update/">Open the experimental APRS modem flasher</a></p>|' \
  dist/firmware-update/index.html > dist/firmware-update/index.html.tmp
mv dist/firmware-update/index.html.tmp dist/firmware-update/index.html

# Offer the catalog built for the H3-style SmartBeacon bench firmware.
test -s dist/apps/catalog/smartbeacon/catalog.json
sed 's|<!-- dev-smartbeacon-catalog -->|<p class="note" id="smartbeaconCatalogLink"><a href="?catalog=smartbeacon">SmartBeacon bench apps</a> · For the matching dev firmware.</p>|' \
  dist/apps/index.html > dist/apps/index.html.tmp
mv dist/apps/index.html.tmp dist/apps/index.html

# Keep development URLs out of search results while allowing crawlers to see the header.
printf '/*\n  X-Robots-Tag: noindex\n/dev-build.txt\n  Cache-Control: no-store\n' > dist/_headers
printf 'User-agent: *\nAllow: /\n' > dist/robots.txt
rm -f dist/sitemap.xml

commit_sha=${CF_PAGES_COMMIT_SHA:-$(git rev-parse HEAD)}
printf 'dev %s\n' "${commit_sha}" > dist/dev-build.txt
