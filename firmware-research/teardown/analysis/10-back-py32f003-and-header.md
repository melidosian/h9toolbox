# 10: back face, Puya PY32F003 microcontroller and the 5-hole header (macro)

**Area:** back face, bottom corner of the board. Received 2026-11-13. Visual only.

| Item | Reading | Confidence |
|------|---------|------------|
| 20-lead chip, logo `PUYA`, text `PY32F003` / `F18P6` / `5L1TA1B` | **Puya PY32F003F18P6**: ARM Cortex-M0+ microcontroller, 20-lead TSSOP. By the part-number scheme this is the 64 KB flash / 8 KB RAM version. `5L1TA1B` is the lot code | marking read; memory size from the part-number scheme, not checked against a datasheet here |
| Row of 5 plated holes, gold rings, a few millimetres from the chip | **5-pin header for this microcontroller**. Five pins fits its debug port (power, ground, two debug lines, reset) | holes read; pin functions unverified |
| Exposed plated board edge with a row of small vias | ground stitching along the edge | read |
| Threaded metal boss in the corner cut-out | chassis screw post | read |
| Blue silk ring with a plain hole | tooling hole | read |
| Groups of four small resistors/capacitors below the chip | support parts | read |

## Why this matters

- The handbook lists the APRS processor as "a separate small MCU (probably ARM
  Cortex-M) ... Not identified; research parked". This is a separate small ARM
  Cortex-M MCU, and it is the only second microcontroller seen so far. **It is the
  leading candidate for the APRS MCU.** Supporting fact: the APRS image is 17,304
  bytes, which fits a 64 KB part.
- Not yet confirmed. To confirm: check that the APRS image's vector table (initial
  stack pointer and reset address) fits 8 KB RAM at `0x20000000` and flash at
  `0x08000000`; trace the header pins with a continuity meter to the chip's debug
  pins; compare the firmware's USART2 / GPIOB5 use with the PY32F003 pin list.
- If the header is the debug port, the APRS firmware could be read or replaced
  through it with a standard debug probe, unless read protection is set.
