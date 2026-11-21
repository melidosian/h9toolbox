# H9 Toolbox Firmware v0.1.7

Use the Type-C TD-H9 development installer at
`/firmware-update/?release=0.1.7`. It verifies and writes the main package
and the matching far firmware image. Use the matching catalog at
`/apps/?catalog=0.1.7` for apps.

- `h9-toolbox-firmware-v0.1.7.fw` contains the main firmware.
- `h9-toolbox-v0.1.7-far.bin` contains the unified menu, resident APRS
  scheduler and part of the optional modem update.
- `apps/` contains the App Manager catalog and modules.
- Run `sha256sum -c SHA256SUMS` from this directory to verify the files.

The decoded main firmware matches the radio-tested RC12 except for the
version word. The far image and the source module bundle match RC12
byte for byte. See the [changelog](../../../../CHANGELOG.md) for controls
and release notes.
