# APRS controller GPIOB bit 5 trace

Source: captured APRS v1.0.15 image, SHA-256 `1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`, imported into the read-only `H9Aprs` Ghidra project. The private image and generated decompiler reports remain outside Git.

- `0x08005C10` and `0x08005C16` dispatch serial command `0x11` subcommands 1 and 2 to `FUN_080029E4` and `FUN_080029D0`.
- `FUN_080029E4` passes GPIOB base `0x50000400`, mask `0x20`, and set mode to `FUN_08003882`. That helper writes the mask at base+`0x18`.
- `FUN_080029D0` passes the same base and mask in clear mode; the helper writes at base+`0x28`.
- `FUN_08002964` passes the same base and mask to `FUN_08003870`, which tests base+`0x10` (input data). The serial `0x11` subcommand 3 uses that result.
- The three base-address literals occur at image addresses `0x08002970`, `0x080029E0`, and `0x080029F4`; `verify_source_inputs.py` checks them in the pinned image.

`replacement-source/aprs/h9_aprs_gpio_board.*` ports these register operations and the later-traced stock initializer at `0x080029F8`: enable the GPIOB clock, configure bit 5 as an output, and initially clear it. No evidence identifies the external circuit connected to bit 5, and this source cannot be treated as a validated PTT driver.
