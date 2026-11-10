# H9 Toolbox module loader: release checklist

Local prep only (2026-11-10). Nothing here is committed or pushed. Branch:
`charlie/ram-loader`. The public site deploys `dist/` as is
(`.github/workflows/pages.yml`), so anything under `dist/` that gets
committed and merged goes live.

## Done locally

- Firmware: `src/stub.s` `ml_reset` fixed (stock cpu_reset sequence). Build
  package `c1a6f78b…`, flash `08d2409a…`.
- Installer (`dist/firmware-update/`): ships that build as
  `h9-toolbox-firmware-v1.0.38.fw`; seeds for stock, the Brick Breaker build
  of H9 Toolbox, the original Brick Breaker and the module-loader preview
  (made by `release_installer.mjs`). Step 04 and the result screen point to
  the App Manager.
- App Manager (`dist/apps/`): site header, nav and footer; boot logo shares
  the Bluetooth connection; APRS 0.3 (LIST, INFO, MAP, MSG, SEND).
- Site: App Manager in every nav and the sitemap; `/firmware/`, home page,
  `llms.txt` and the root README now lead with APRS and the App Manager.
- Tests: `node --test test/*.test.mjs` 113/113; `py -3 -I test_modules.py` OK.

## Hardware checks (2026-11-10)

Passed: install through the local `/firmware-update/` page; Back hold →
menu; RADIO; USB UPDATE; boot logo write from the App Manager; APRS opens on
the list with stored stations, squelch open (battery-save wake + RF reg
0x30); APRS SEND transmits on the APRS TX channel (CH-B).

Still open:
1. APRS live receive: RX count rising over time.
2. A sent message shows in MSG.
3. Optional: `ml_reset` via `build/test-ml-reset-bundle.bin` (only reached
   when the menu fails to load).

## Still to decide

- `/boot-logo/`: keep as is, or point it to the App Manager.
- Dev site: `/dev/` path or Cloudflare Pages (`pages-dev.yml`, untracked).

## Commit plan (when approved)

Commit `firmware-research/module-loader/` (its `.gitignore` keeps `build/`
out), `dist/` changes, tests, `resource-flash-log.md`, README, AGENTS.md.
Review first: `archive/`, `prototypes/bluetooth-rx-audio/`, the stage 24–29
scripts. Never commit RAM dumps, flash backups, `build/` outputs, stock images.
