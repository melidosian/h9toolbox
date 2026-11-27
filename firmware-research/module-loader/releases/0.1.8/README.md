# H9 Toolbox Firmware v0.1.8

Select v0.1.8 in the H9 Toolbox installer for the Type-C TD-H9 at
`/firmware-update/?release=0.1.8`. The installer selects the main firmware
and its paired far image. The matching App Manager catalog is at
`/apps/?catalog=0.1.8`.

- `h9-toolbox-firmware-v0.1.8.fw` contains the main firmware.
- `h9-toolbox-v0.1.8-far.bin` contains the unified menu, resident APRS
  scheduler and part of the optional modem update.
- `h9-toolbox-firmware-v0.1.8-no-modem.fw` and
  `h9-toolbox-v0.1.8-no-modem-far.bin` are the paired build for the same
  controls with the modem updater unchecked. They were originally reproduced from
  revision `204af53` using the unified build before the modem stage is
  added. The menu, scheduler and catalog modules match the standard build.
- `apps/` contains the catalog and modules from the matching build.
- Run `sha256sum -c SHA256SUMS` here to verify the release files.

The main and far files were rebuilt from the v0.1.8 release source
with the native display handoff, Dual Watch fix, and support for opening
Toolbox before installing any optional apps. Both modem options passed
offline verification. These rebuilt images await radio testing.
See the [changelog](../../../../CHANGELOG.md) for controls and release notes.
