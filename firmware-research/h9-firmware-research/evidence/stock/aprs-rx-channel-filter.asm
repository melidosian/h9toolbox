; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 36; undecoded chunks: 0
0x01E86238  app+0x086138  7f04          [--sp] = {rets, r15-r4}
0x01E8623A  app+0x08613A  e28a          sp += -88
0x01E8623C  app+0x08613C  0c16          r12 = r0
0x01E8623E  app+0x08613E  bfeaed2a      call -109094
0x01E86242  app+0x086142  c9fff0c00000  r9 = 49392
0x01E86248  app+0x086148  c8ffac15eb01  r8 = 32183724
0x01E8624E  app+0x08614E  00ff00000501  if (r0 == 0) goto 522
0x01E86254  app+0x086154  40e08c21      r0 = 8588
0x01E86258  app+0x086158  c7ffd01a0100  r7 = 72400
0x01E8625E  app+0x08615E  d8ee7000      r0 = b[r7+r0] (u)
0x01E86262  app+0x086162  00f8fc00      if (r0 == 0) goto 504
0x01E86266  app+0x086166  d1ec7c1b      r1 = [r7+444]
0x01E8626A  app+0x08616A  d1ec702d      r2 = [r7+464]
0x01E8626E  app+0x08616E  4020          r0 = 0
0x01E86270  app+0x086170  02e80610      if (r1 == r2) goto 12
0x01E86274  app+0x086174  d1ec782d      r2 = [r7+472]
0x01E86278  app+0x086178  4021          r0 = 1
0x01E8627A  app+0x08617A  91e80002      if (r1 != r2) {
0x01E8627E  app+0x08617E  783f          r0 = 255
0x01E86280  app+0x086180  41e04204      r1 = 1090
0x01E86284  app+0x086184  d8ee9011      r1 = b[r9+r1] (u)
0x01E86288  app+0x086188  0149          if (r1 == 0) goto 18
0x01E8628A  app+0x08618A  01f80404      if (r1 == 2) goto 8
0x01E8628E  app+0x08618E  81f8e602      if (r1 != 1) goto 460
0x01E86292  app+0x086192  4120          r1 = 0
0x01E86294  app+0x086194  0481          goto 2
0x01E86296  app+0x086196  4121          r1 = 1
0x01E86298  app+0x086198  81e8e100      if (r0 != r1) goto 450
0x01E8629C  app+0x08619C  40e08423      r0 = 9092
0x01E862A0  app+0x0861A0  d8ee7000      r0 = b[r7+r0] (u)
0x01E862A4  app+0x0861A4  00f88000      if (r0 == 0) goto 256
0x01E862A8  app+0x0861A8  50ee9f00      r0 = b[r9+15] (u)
0x01E862AC  app+0x0861AC  50e86f00      if ((r0 & 1) == 0) goto 222
0x01E862B0  app+0x0861B0  50e88c10      if ((r0 & 4) == 0) goto 280
0x01E862B4  app+0x0861B4  c881          r0 = sp + 65
0x01E862B6  app+0x0861B6  4420          r4 = 0
