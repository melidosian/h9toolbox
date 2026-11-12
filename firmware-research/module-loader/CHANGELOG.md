# H9 Toolbox Firmware: versions

The firmware version lives in one file, [`VERSION`](VERSION) (`major.minor.patch`, each below 256). `build.py` writes it into the loader's API table
(`src/stub.s` `ml_fw_version`, API version 2 and later) and checks the built image; the ABOUT module shows it as `FIRMWARE V0.1.4` (`OLD` on a loader
without the field); the installer (`dist/firmware-update/firmware.mjs`) names it in `BUILD.label` / `BUILD.version`; `test/firmware-version.test.mjs` keeps
the file and the installer in step. Bump `VERSION` for every build that is released, add an entry here with the package hash, and run
`release_update.mjs` (see RELEASE.md), which keeps the previous build recognisable in the installer.

The version describes the loader firmware on top of stock v1.0.38. App versions (App Manager catalog) are separate.

## v0.1.4 (2026-11-12)

- Back in the menu returns to the radio without a restart (the loader keeps the radio's task frame); the menu has no RADIO / RESTART items (menu 1.2).
- Loader API version 2: a module that returns `0xFFFF` (`H9_RETURN_RADIO`) leaves for the radio at once without loading the menu, so SAT TRACK
  starts the resident tracker without a restart (SAT PASS 0.25). The API table carries the firmware version.
- Package `cd1bfd70d1d72c3290d554a397e0a325f6e6fd68827413a14227b389de02fb05`.
- Not yet verified on a radio: the SAT TRACK start without a restart. The menu return was verified (2026-11-12).

## Before versioning

Earlier builds keep the ids the installer already knows (image SHA-256 in `firmware.mjs` `PREVIOUS`):

| Id | What it added |
|---|---|
| preview | the app loader (hold Back for a menu of modules on the resource chip) |
| release1 | first release of the app loader |
| release2 | the background GPS recorder |
| release3 | the SAT TRACK resident VFO tracker |
| release4 | serial / USB-C clone writes at 0x2000-0x3FFF use the sector-aware writer |
| release5 | return to the radio from the menu (the loader keeps the task frame); RADIO item |
| release6 | loader API 2: a module can leave for the radio without loading the menu (SAT TRACK starts without a restart); no version number yet. v0.1.4 is this build plus the version word |
