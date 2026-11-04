# Main firmware RAM byte-transfer routine

The pinned v1.0.38 main `app.bin` (SHA-256 `11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`) copies a 55,928-byte code region from app offset `0x0C618C` to RAM address zero during startup. Its first 64 bytes are zero. The routine at RAM `0x40` begins at app offset `0x0C61CC`:

```text
RAM 0x40  c1 ff 08 3e 1e 00  r1 = 0x001E3E08
RAM 0x46  90 61              [r1+4] = r0
RAM 0x48  00 ef 10 10        [r1+0] |= 0x10
RAM 0x4c  10 60              r0 = [r1+0]
RAM 0x4e  50 e8 fd 0b        loop while (r0 & 2) != 0
RAM 0x52  10 61              r0 = [r1+4]
RAM 0x54  00 17              r0 = low unsigned byte
RAM 0x56  80 00              return
```

`main/h9_main_serial_bus.c` ports those register operations with a bounded poll. The target implementation is linked at RAM `0x40` and the diagnostic link check verifies the 64-byte zero prefix. The C function's arguments and return value differ from the stock routine's ABI, so it is **not** a drop-in target for unported callers. The connected peripheral and its role in the radio are unproven. No board code calls this routine yet, and the remaining RAM code, interrupt setup, and full boot path are missing. The diagnostic ELF is not a flashable firmware image.
