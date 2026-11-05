# Main ADC mode selector for the `app_core` sample path

Source: stock TD-H9 v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The selector app+`0x04AAA0..0x04AB7C` hashes to SHA-256
`c78f30b8f404ad20bf292fba170691dcdf9b641c2ec5185ae25b53dfb55a96bf`.
Its two short helper ranges app+`0x04AA78..0x04AAA0` hash to SHA-256
`7fa0f75fc51ba369789af91a33d20be4cdf03bcd03174d6976e8b95ac4a9dfb9`.
Addresses use inferred application base `0x01E00100`.

The `app_core` capture at app+`0x060CAC..0x060D16` requests modes
`0x50000F` and `0x00000F`. Both have low halfword `15` and low 20 bits
`15`, selecting case zero of the stock jump table. The selector reads the
previous halfword at RAM `0x11B98`, writes `0xFFFF` there, and later
returns the previous value. For either mode, it:

1. Calls ROM `0x020000E2(4,0,1,0)`.
2. Replaces bits 24–30 of MMIO `0x1E19EC` using
   `(old & 0x80FFFFFF) | 0x73000000`.
3. Clears bit 18 at `0x1E0044`, bit 19 at `0x1E1964`, and bit 6 at
   `0x1E1910`.
4. Takes jump-table case zero, calling app+`0x04AA78`, which calls ROM
   `0x020000E2(4,4,1,0)` and `0x020000E2(4,0,1,1)`.
5. Calls the app+`0x04AA94` wrapper, which passes
   `(4,1,3,mode >> 20)` to that ROM entry. This last value is 5 for
   `0x50000F` and 0 for `0x00000F`.
6. Writes `0xFF6E` to MMIO `0x1E3100`, then sets bits 4 and 6.

The vendor Pi32 disassembler was used to check the branch, jump-table
case, ROM targets, and register operations. The `r1 = ...` and MMIO store
at app+`0x04AAE8` are a dual issue instruction; the store uses the prior
masked `r1` while the new `r1` becomes the `0x1E0044` pointer. The
replacement writes the masked value before clearing that separate register.
`main/h9_main_adc_mode.*` ports these two modes and rejects the other
stock selector modes until their branches are reconstructed. No physical
ADC initialization has been tested on the radio.
