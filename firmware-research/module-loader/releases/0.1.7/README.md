# H9 Toolbox Firmware v0.1.7

Use the Type-C TD-H9 development installer at
`/firmware-update/?release=0.1.7`. It verifies and writes the main package
and the matching far firmware image. Use the matching catalog at
`/apps/?catalog=0.1.7` for apps.

- `h9-toolbox-firmware-v0.1.7.fw` contains the main firmware.
- `h9-toolbox-v0.1.7-far.bin` contains the unified menu, resident APRS
  scheduler and part of the optional modem update.
- `h9-toolbox-firmware-v0.1.7-no-modem.fw` and
  `h9-toolbox-v0.1.7-no-modem-far.bin` are the paired build for the same
  controls with the modem updater unchecked. They were originally reproduced from
  revision `7e0f6e9` using the unified build before the modem stage is
  added. The menu, scheduler and catalog modules match the standard build.
- `apps/` contains the App Manager catalog and modules.
- Run `sha256sum -c SHA256SUMS` from this directory to verify the files.

The main and far files were rebuilt from the v0.1.7 release source
with the native display handoff and Dual Watch fixes. Both modem options
passed offline verification. These rebuilt images await radio testing.
See the [changelog](../../../../CHANGELOG.md) for controls and release notes.
