# Main clock ROM mode helper

Source: pinned main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The independent Pi32v2 r3 decoder lists app+`0x0C5288..0x0C52D4`
in ignored `output/main-clock-helpers-independent.asm`.

After the first clock-apply transition, stock calls app+`0x0C5288`.
It reads the calibration-enable byte at RAM `0xD9D0`. If nonzero, it reads
the selected band byte at `0xD9D4` and indexes four literal bytes at
app+`0x0C564E`: `{7, 8, 10, 14}`. It calls ROM `0x020000E2` twice:

- Enabled: `(9, 0, 4, table[band])`, then `(6, 0, 3, 7)`.
- Disabled: `(9, 0, 4, 0)`, then `(6, 0, 3, 0)`.

The stock code does not check the band index. The replacement
`h9_main_clock_rom_mode_apply` rejects an enabled out-of-range band before
either ROM call; the disabled path does not need to read the band. Host
tests cover all four table entries, disabled mode, and the invalid-band
guard. The target binding points at the observed RAM bytes and ROM entry.

Stock next re-runs app+`0x0C5492`, the already ported clock selector.
The diagnostic board entry follows that order and then runs the MMIO,
source/latch, delay, auxiliary, and final ROM sequence through the helper's
return, described in [the apply trace](main-clock-apply.md). No physical
clock behavior has been measured with replacement code.
