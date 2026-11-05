# `app_core` ADC sample capture after pin-registry reset

Source: stock TD-H9 v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The observed app+`0x060CAC..0x060D16` slice hashes to SHA-256
`f874cb5344481b88e4d410126f98c3e0744af564a499fe4bfd3769f09aae1a01`.
Addresses use the inferred `0x01E00100` application base.

Immediately after resetting the pin registry, stock writes zero twice to
MMIO `0x1E3100`, calls app+`0x04AA78`, then calls app+`0x04AAA0` with
`0x50000F`. The first helper calls ROM `0x020000E2` with `(4,4,1,0)`
and `(4,0,1,1)`; the second helper is a longer ADC mode selector that
remains unported.

Stock takes 16 samples from MMIO `0x1E3104` and stores their low halfwords
at RAM `0x12338..0x12356`. For each sample it waits for control bit 7 at
`0x1E3100` to set, stores the halfword, then sets control bit 6. It clears
the halfword at RAM `0x11B96`, selects ADC mode `0x0F`, then takes ten more
samples. Each is added to that halfword with 16-bit truncation, followed
by the bit-6 control write. Finally stock divides the accumulated halfword
by ten, stores it at `0x11B96`, and sets RAM halfword `0x11B98` to one.

The corrected Ghidra pseudocode lists an `0x1C` control mask. Its own
instruction listing shows `jmnz ... #0x1c`, but the independent Pi32v2
decoder and the vendor Pi32 `llvm-objdump` both decode the exact stock
bytes `50 E8 FD 39` as a branch while bit 7 is clear. The replacement
uses the vendor toolchain's bit-7 condition and bounds the poll. Physical
ADC behavior has not been measured. `main/h9_main_core_samples.*` ports
this bounded block through an injected mode selector; the larger
`app_core` setup call remains incomplete.
