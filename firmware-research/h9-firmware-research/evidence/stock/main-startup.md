# Main Pi32v2 startup trace

Source: captured main v1.0.38 `app.bin`, SHA-256 `11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`. The virtual base `0x01E00100` is inferred from JLFS metadata. Instruction decoding used Pi32v2 r3; this is static evidence, not a boot test.

At app+`0x000000`, the stock entry first calls app+`0x02D82E` before setting a stack. That leaf routine reads the launch structure through `r0` and copies fields into RAM beginning at `0xE2A0`; its 42 instruction bytes through the final `rts` are matched byte for byte in `main/h9_main_startup.S` by `link_main_check.py`. At app+`0x000004` and `0x00000A`, stock sets both `sp` and `ssp` to decimal `58000` (`0xE290`). The replacement assembler emits the same 12 stack-setup bytes.

Stock then clears RAM from `0xE2E0` for `66036` bytes, ending at `0x1E4D4`. Its preceding `.data` copy has a zero length in this image. A later loop copies `55928` bytes from mapped source `0x01EC628C` to RAM zero, changes `icfg`, calls two startup routines, and jumps to `0x01E5FB7C`. The purpose and relocation contract of that copied code still need reconstruction.

The replacement diagnostic linker confines `.data`/`.bss` to the observed `0xE2E0..0x1E4D4` interval. Its reset path copies linker-defined `.data`, zeros `.bss`, and reaches an inert board entry. The stock RAM-zero code copy, interrupt configuration, remaining startup calls, device initialization, and application loop are not ported. The linked ELF must not be treated as a bootable update image.
