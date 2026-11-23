# H9 Toolbox Firmware v0.1.8

Use the Type-C TD-H9 Dev installer at `/firmware-update/` or
`/firmware-update/?release=0.1.8`. Both select the main firmware and
its paired far image. The matching App Manager catalog is available at
`/apps/` or `/apps/?catalog=0.1.8` on Dev.

- `h9-toolbox-firmware-v0.1.8.fw` contains the main firmware.
- `h9-toolbox-v0.1.8-far.bin` contains the unified menu, resident APRS
  scheduler and part of the optional modem update.
- `apps/` contains the catalog and modules from the matching build.
- Run `sha256sum -c SHA256SUMS` here to verify the release files.

The main application matches the RC6 bench image apart from its version
marker. The far image is byte-identical to RC6. See the
[changelog](../../../../CHANGELOG.md) for the 900TX controls.
