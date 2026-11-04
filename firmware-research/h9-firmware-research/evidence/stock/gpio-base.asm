; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 32; undecoded chunks: 1
0x01E00180  app+0x000080  002b          r0 = [sp+44]
0x01E00182  app+0x000082  72e1402f      r2 = r2 & 0xFFFFFCFF
0x01E00186  app+0x000086  64e0802b      icfg = r2
0x01E0018A  app+0x00008A  84ea1760      call 573486
0x01E0018E  app+0x00008E  82ea01f2      call 386050
0x01E00192  app+0x000092  c0ff7cfbe501  r0 = 31849340
0x01E00198  app+0x000098  d000          goto r0
0x01E0019A  app+0x00009A  0000          nop
0x01E0019C  app+0x00009C  f83b          r0 += -5
0x01E0019E  app+0x00009E  4221          r2 = 1
0x01E001A0  app+0x0000A0  021a          r2 <<= r0
0x01E001A2  app+0x0000A2  c0ff00511e00  r0 = 1986816
0x01E001A8  app+0x0000A8  31e80060      if (r1 == 0) {
0x01E001AC  app+0x0000AC  0160          r1 = [r0+0]
0x01E001AE  app+0x0000AE  90e11312      r1 = r1 & ~r2
0x01E001B2  app+0x0000B2  0160          r1 = [r0+0]
0x01E001B4  app+0x0000B4  2119          r1 |= r2
0x01E001B6  app+0x0000B6  8160          [r0+0] = r1
0x01E001B8  app+0x0000B8  8000          rts
0x01E001BA  app+0x0000BA  0116          r1 = r0
0x01E001BC  app+0x0000BC  4020          r0 = 0
0x01E001BE  app+0x0000BE  b1ec38c0      if (r1 <= 56) {
0x01E001C2  app+0x0000C2  90a4          r0 = r1 >> 4
0x01E001C4  app+0x0000C4  00a2          r0 = r0 << 2
0x01E001C6  app+0x0000C6  c1ff5406e001  r1 = 31458900
0x01E001CC  app+0x0000CC  d8ec0201      r0 = [r0+r1]
0x01E001D0  app+0x0000D0  8000          rts
0x01E001D2  app+0x0000D2  7704          [--sp] = {rets, r7-r4}
0x01E001D4  app+0x0000D4  1316          r3 = r1
0x01E001D6  app+0x0000D6  0416          r4 = r0
0x01E001D8  app+0x0000D8  84fc0182      if (r4 <= 65) goto 2
0x01E001DC  app+0x0000DC  5704          {pc, r7-r4} = [sp++]
0x01E001DE  app+0x0000DE  84f9          .byte 0x84, 0xF9
