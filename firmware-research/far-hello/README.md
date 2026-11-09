# Far hello: code running from main flash 0xEA000

The first test of the free main-flash space found on 2026-11-09:
`0xEA000–0xFBFFF` (72 KiB) is after the settings VM (two 32 KiB banks at
`0xDA000`/`0xE2000`) and is written by no stock code
([../resource-flash-log.md](../resource-flash-log.md)). The chip maps all of
flash into the CPU, so flash `0xEA000` runs at `0x01EE5000`.

## What it is

- **Far program** (`h9_far_hello_0xEA000.bin`, 1,696 bytes): a 16-byte header
  (`H9X1` magic, then a jump) and the unchanged [C-hello](../c-hello) startup
  from the point Brick Breaker enters, plus C-hello's `main.c`.
- **H9 Toolbox Firmware with a far launcher** (`h9_far_hello_OFFLINE_EXPERIMENT.fw`):
  - a 24-byte stub in the spare tail of the `.tables` cave (app+`0xA7E80`)
  - the Back-hold launcher's jump (app+`0xA9DD2`) pointed at the stub
    instead of Brick Breaker

  Everything else is H9 Toolbox Firmware: the radio boots and works normally,
  hold-8 update mode still works, and the Bluetooth resource service is
  unchanged.

Holding **Back** runs the stub. It reads the first word of the far region
through the CPU mapping:

- **`H9X1`:** jump into the far program (C-hello's readout screen).
- **Anything else:** start Brick Breaker as before. A wrong guess about how
  the mapping decrypts that area therefore cannot crash the radio.

## The hypothesis it tests

The app area is stored encrypted: 32-byte blocks XORed with JieLi's CRC
stream, keyed `0xF181 ^ ((flash - 0x5000) >> 2)`, and decrypted by the
flash mapping. The far blob is encrypted the same way, continued to
`0xEA000`. The build checks that this cipher reproduces the repacker's
bytes for the stub.

| On hardware | Meaning |
|---|---|
| C-hello appears | The mapping decrypts the far region like the app area; code runs from 0xEA000 |
| Brick Breaker appears | The magic did not read back; next try writes the blob unencrypted |

While C-hello runs, a Bluetooth read (programmer fast read or boot-logo
read) tests whether the resource service still answers. Power-cycle to
leave.

## Build

```
export JIELI_TOOLCHAIN=~/path/to/jieli-linux-toolchains-20260730.1
python3 -I build.py --stock-package <TD-H9-V1.0.38.fw>
```

It needs the pinned H9 Toolbox build in `../resource-service/build`
(`build.py --stage bb-s2`). It checks:

- the stub decode
- sectors changed: vs stock, the 8 H9 Toolbox sectors; vs H9 Toolbox, only
  `0x5000`, `0xAC000`, `0xAE000`, `0xD9000`
- the cipher
- that the package rebuilds byte for byte

## Flash (hold-8 mode, twice)

```
h9_flasher.py write-far build/h9_far_hello_0xEA000.bin --device 4C4A:2342 --backup <far-backup.bin> --yes
h9_flasher.py flash build/h9_far_hello_OFFLINE_EXPERIMENT.fw --stock-base <TD-H9-V1.0.38.fw> --device 4C4A:2342 --backup <backup.bin> --yes
```

`write-far` only accepts sector-aligned writes inside `0xEA000–0xFBFFF`,
saves the old bytes first and verifies every sector. To undo, reflash H9
Toolbox Firmware; the far bytes are then unused.
