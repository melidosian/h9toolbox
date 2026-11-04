; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 33; undecoded chunks: 0
0x01E82FB2  app+0x082EB2  7404          [--sp] = {rets, r4}
0x01E82FB4  app+0x082EB4  40e02304      r0 = 1059
0x01E82FB8  app+0x082EB8  c1fff0c00000  r1 = 49392
0x01E82FBE  app+0x082EBE  d8ee1020      r2 = b[r1+r0] (u)
0x01E82FC2  app+0x082EC2  c0ffd01a0100  r0 = 72400
0x01E82FC8  app+0x082EC8  b2e80180      if (r2 != 1) {
0x01E82FCC  app+0x082ECC  4120          r1 = 0
0x01E82FCE  app+0x082ECE  d2ec0d11      [r0+540] = r1
0x01E82FD2  app+0x082ED2  5404          {pc, r4} = [sp++]
0x01E82FD4  app+0x082ED4  42e01202      r2 = 530
0x01E82FD8  app+0x082ED8  d8ed1822      r2 = h[r1+r2<<1] (u)
0x01E82FDC  app+0x082EDC  1240          if (r2 == 0) goto 64
0x01E82FDE  app+0x082EDE  d2ec0c31      r3 = [r0+540]
0x01E82FE2  app+0x082EE2  c321          r3 += 1
0x01E82FE4  app+0x082EE4  e2e16420      r2 = r2 * 0x64
0x01E82FE8  app+0x082EE8  d2ec0d31      [r0+540] = r3
0x01E82FEC  app+0x082EEC  82e91730      if (r3 < r2) goto 46
0x01E82FF0  app+0x082EF0  50ee0e44      r4 = b[r0+78] (u)
0x01E82FF4  app+0x082EF4  42e04404      r2 = 1092
0x01E82FF8  app+0x082EF8  d8ee1012      r1 = b[r1+r2] (u)
0x01E82FFC  app+0x082EFC  4220          r2 = 0
0x01E82FFE  app+0x082EFE  d2ec0d21      [r0+540] = r2
0x01E83002  app+0x082F02  84f80102      if (r4 != 1) goto 2
0x01E83006  app+0x082F06  014b          if (r1 == 0) goto 22
0x01E83008  app+0x082F08  4832          r0 = 50
0x01E8300A  app+0x082F0A  8beaf9ec      call 1563122
0x01E8300E  app+0x082F0E  beeae1c9      call -158782
0x01E83012  app+0x082F12  04f80402      if (r4 == 1) goto 8
0x01E83016  app+0x082F16  0043          if (r0 == 0) goto 6
0x01E83018  app+0x082F18  4020          r0 = 0
0x01E8301A  app+0x082F1A  beeae7ce      call -156210
0x01E8301E  app+0x082F1E  5404          {pc, r4} = [sp++]
0x01E83020  app+0x082F20  4830          r0 = 48
