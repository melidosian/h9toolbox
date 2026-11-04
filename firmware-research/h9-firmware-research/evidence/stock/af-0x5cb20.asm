; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 47; undecoded chunks: 1
0x01E5CC20  app+0x05CB20  ff07          b[r7++=-1] = r7
0x01E5CC22  app+0x05CB22  8520          [sp] = r5
0x01E5CC24  app+0x05CB24  bfeafa91      call -56332
0x01E5CC28  app+0x05CB28  0288          sp += 32
0x01E5CC2A  app+0x05CB2A  5604          {pc, r6-r4} = [sp++]
0x01E5CC2C  app+0x05CB2C  7404          [--sp] = {rets, r4}
0x01E5CC2E  app+0x05CB2E  40e05802      r0 = 600
0x01E5CC32  app+0x05CB32  c4ffd01a0100  r4 = 72400
0x01E5CC38  app+0x05CB38  d8ee4000      r0 = b[r4+r0] (u)
0x01E5CC3C  app+0x05CB3C  30e80010      if (r0 == 0) {
0x01E5CC40  app+0x05CB40  c1fff5bd0000  r1 = 48629
0x01E5CC46  app+0x05CB46  c1fffec10000  r1 = 49662
0x01E5CC4C  app+0x05CB4C  4830          r0 = 48
0x01E5CC4E  app+0x05CB4E  bfea5c9d      call -50504
0x01E5CC52  app+0x05CB52  4021          r0 = 1
0x01E5CC54  app+0x05CB54  bfeac6e3      call -14452
0x01E5CC58  app+0x05CB58  5830          r0 = 112
0x01E5CC5A  app+0x05CB5A  4120          r1 = 0
0x01E5CC5C  app+0x05CB5C  bfea559d      call -50518
0x01E5CC60  app+0x05CB60  50ee4e02      r0 = b[r4+46] (u)
0x01E5CC64  app+0x05CB64  8044          if (r0 != 0) goto 8
0x01E5CC66  app+0x05CB66  4024          r0 = 4
0x01E5CC68  app+0x05CB68  4120          r1 = 0
0x01E5CC6A  app+0x05CB6A  bfea6fab      call -43298
0x01E5CC6E  app+0x05CB6E  5404          {pc, r4} = [sp++]
0x01E5CC70  app+0x05CB70  7404          [--sp] = {rets, r4}
0x01E5CC72  app+0x05CB72  e29f          sp += -4
0x01E5CC74  app+0x05CB74  0416          r4 = r0
0x01E5CC76  app+0x05CB76  4020          r0 = 0
0x01E5CC78  app+0x05CB78  d8e90300      h[sp+2] = r0
0x01E5CC7C  app+0x05CB7C  8a82          r2 = sp + 2
0x01E5CC7E  app+0x05CB7E  4024          r0 = 4
0x01E5CC80  app+0x05CB80  41e0de52      r1 = 21214
0x01E5CC84  app+0x05CB84  4322          r3 = 2
0x01E5CC86  app+0x05CB86  bfea14b0      call -40920
0x01E5CC8A  app+0x05CB8A  04fc1c08      if (r4 > 4) goto 56
0x01E5CC8E  app+0x05CB8E  dce90310      r1 = b[sp+3] (u)
0x01E5CC92  app+0x05CB92  dce90200      r0 = b[sp+2] (u)
0x01E5CC96  app+0x05CB96  a0e16014      r0 <= insert(r1, p:8, l:24)
0x01E5CC9A  app+0x05CB9A  0401          tbb [r4]
0x01E5CC9C  app+0x05CB9C  0305          r3 = [r0++=4]
0x01E5CC9E  app+0x05CB9E  080b          [r0++=r14] = r0
0x01E5CCA0  app+0x05CBA0  1000          .byte 0x10, 0x00
0x01E5CCA2  app+0x05CBA2  302f          r0 |= 32768
0x01E5CCA4  app+0x05CBA4  0488          goto 16
0x01E5CCA6  app+0x05CBA6  40e1200c      r0 = r0 | 0xA000
0x01E5CCAA  app+0x05CBAA  0485          goto 10
0x01E5CCAC  app+0x05CBAC  40e1400c      r0 = r0 | 0xC000
