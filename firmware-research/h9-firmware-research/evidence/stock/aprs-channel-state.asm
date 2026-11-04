; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 123; undecoded chunks: 0
0x01E5A616  app+0x05A516  7804          [--sp] = {rets, r8-r4}
0x01E5A618  app+0x05A518  e29c          sp += -16
0x01E5A61A  app+0x05A51A  c4ff671beb01  r4 = 32185191
0x01E5A620  app+0x05A520  01e1e541      r1 = r4 + 485
0x01E5A624  app+0x05A524  8888          r0 = sp + 8
0x01E5A626  app+0x05A526  bfeae8b2      call -39472
0x01E5A62A  app+0x05A52A  d8e90a70      r7 = h[sp+10] (u)
0x01E5A62E  app+0x05A52E  d8e90860      r6 = h[sp+8] (u)
0x01E5A632  app+0x05A532  c5ffd01a0100  r5 = 72400
0x01E5A638  app+0x05A538  50ee5015      r1 = b[r5+80] (u)
0x01E5A63C  app+0x05A53C  c0fff0c00000  r0 = 49392
0x01E5A642  app+0x05A542  015a          if (r1 == 0) goto 52
0x01E5A644  app+0x05A544  d1ec5c1b      r1 = [r5+444]
0x01E5A648  app+0x05A548  d1ec502d      r2 = [r5+464]
0x01E5A64C  app+0x05A54C  82e80810      if (r1 != r2) goto 16
0x01E5A650  app+0x05A550  50ee5018      r1 = b[r5+128] (u)
0x01E5A654  app+0x05A554  b927          r1 &= 4294967167
0x01E5A656  app+0x05A556  52ee5018      b[r5+128] = r1
0x01E5A65A  app+0x05A55A  50ee0205      r0 = b[r0+82] (u)
0x01E5A65E  app+0x05A55E  0492          goto 36
0x01E5A660  app+0x05A560  d1ec582d      r2 = [r5+472]
0x01E5A664  app+0x05A564  82e80810      if (r1 != r2) goto 16
0x01E5A668  app+0x05A568  50ee5018      r1 = b[r5+128] (u)
0x01E5A66C  app+0x05A56C  3127          r1 |= 128
0x01E5A66E  app+0x05A56E  52ee5018      b[r5+128] = r1
0x01E5A672  app+0x05A572  50ee0205      r0 = b[r0+82] (u)
0x01E5A676  app+0x05A576  0496          goto 44
0x01E5A678  app+0x05A578  54ee5018      r1 = b[r5+128] (s)
0x01E5A67C  app+0x05A57C  50ee0205      r0 = b[r0+82] (u)
0x01E5A680  app+0x05A580  81fd1000      ifs (r1 < 0) goto 32
0x01E5A684  app+0x05A584  0051          if (r0 == 0) goto 34
0x01E5A686  app+0x05A586  50ee5901      r0 = b[r5+25] (u)
0x01E5A68A  app+0x05A58A  1048          if (r0 == 0) goto 80
0x01E5A68C  app+0x05A58C  40e0e210      r0 = 4322
0x01E5A690  app+0x05A590  d8ee5000      r0 = b[r5+r0] (u)
0x01E5A694  app+0x05A594  1043          if (r0 == 0) goto 70
0x01E5A696  app+0x05A596  e017          r0 = r6.l (u)
0x01E5A698  app+0x05A598  f417          r4 = r7.l (u)
0x01E5A69A  app+0x05A59A  4230          r2 = 16
0x01E5A69C  app+0x05A59C  4116          r1 = r4
0x01E5A69E  app+0x05A59E  bfeaf2fe      call -540
0x01E5A6A2  app+0x05A5A2  149a          goto 116
0x01E5A6A4  app+0x05A5A4  80f82a02      if (r0 != 1) goto 84
0x01E5A6A8  app+0x05A5A8  c014          r8 = 0
0x01E5A6AA  app+0x05A5AA  d4e90580      [sp+4] = r8
0x01E5A6AE  app+0x05A5AE  4130          r1 = 16
0x01E5A6B0  app+0x05A5B0  e0d7          r0 = r6.l (u)  #
0x01E5A6B2  app+0x05A5B2  8120          [sp] = r1
0x01E5A6B4  app+0x05A5B4  f517          r5 = r7.l (u)
0x01E5A6B6  app+0x05A5B6  c3ffffff0000  r3 = 65535
0x01E5A6BC  app+0x05A5BC  5116          r1 = r5
0x01E5A6BE  app+0x05A5BE  4216          r2 = r4
0x01E5A6C0  app+0x05A5C0  bfeaaca4      call -46760
0x01E5A6C4  app+0x05A5C4  00e14060      r0 = r6 + 64
0x01E5A6C8  app+0x05A5C8  8017          r0 = r0.l (u)
0x01E5A6CA  app+0x05A5CA  7990          r1 = r7 + 16
0x01E5A6CC  app+0x05A5CC  9317          r3 = r1.l (u)
0x01E5A6CE  app+0x05A5CE  6a20          r2 = 160
0x01E5A6D0  app+0x05A5D0  d4e90180      [sp] = r8
0x01E5A6D4  app+0x05A5D4  5116          r1 = r5
0x01E5A6D6  app+0x05A5D6  bfea72b2      call -39708
0x01E5A6DA  app+0x05A5DA  2482          goto 132
0x01E5A6DC  app+0x05A5DC  4120          r1 = 0
0x01E5A6DE  app+0x05A5DE  f4d7          r4 = r7.l (u)  #
0x01E5A6E0  app+0x05A5E0  8120          [sp] = r1
0x01E5A6E2  app+0x05A5E2  6898          r0 = r6 + 24
0x01E5A6E4  app+0x05A5E4  8017          r0 = r0.l (u)
0x01E5A6E6  app+0x05A5E6  02e12060      r2 = r6 + 32
0x01E5A6EA  app+0x05A5EA  a217          r2 = r2.l (u)
0x01E5A6EC  app+0x05A5EC  7b90          r3 = r7 + 16
0x01E5A6EE  app+0x05A5EE  b317          r3 = r3.l (u)
0x01E5A6F0  app+0x05A5F0  4116          r1 = r4
0x01E5A6F2  app+0x05A5F2  bfea64b2      call -39736
0x01E5A6F6  app+0x05A5F6  d1ec502d      r2 = [r5+464]
0x01E5A6FA  app+0x05A5FA  1489          goto 82
0x01E5A6FC  app+0x05A5FC  50ee5a01      r0 = b[r5+26] (u)
0x01E5A700  app+0x05A600  0057          if (r0 == 0) goto 46
0x01E5A702  app+0x05A602  40e0e310      r0 = 4323
0x01E5A706  app+0x05A606  d8ee5000      r0 = b[r5+r0] (u)
0x01E5A70A  app+0x05A60A  0052          if (r0 == 0) goto 36
0x01E5A70C  app+0x05A60C  e017          r0 = r6.l (u)
0x01E5A70E  app+0x05A60E  f417          r4 = r7.l (u)
0x01E5A710  app+0x05A610  4230          r2 = 16
0x01E5A712  app+0x05A612  4116          r1 = r4
0x01E5A714  app+0x05A614  bfea1bff      call -458
0x01E5A718  app+0x05A618  4120          r1 = 0
0x01E5A71A  app+0x05A61A  00f14060      r0 = r6 + 64  #
0x01E5A71E  app+0x05A61E  8120          [sp] = r1
0x01E5A720  app+0x05A620  8017          r0 = r0.l (u)
0x01E5A722  app+0x05A622  7a90          r2 = r7 + 16
0x01E5A724  app+0x05A624  a317          r3 = r2.l (u)
0x01E5A726  app+0x05A626  6a20          r2 = 160
0x01E5A728  app+0x05A628  4116          r1 = r4
0x01E5A72A  app+0x05A62A  bfea48b2      call -39792
0x01E5A72E  app+0x05A62E  0498          goto 48
0x01E5A730  app+0x05A630  4120          r1 = 0
0x01E5A732  app+0x05A632  f4d7          r4 = r7.l (u)  #
0x01E5A734  app+0x05A634  8120          [sp] = r1
0x01E5A736  app+0x05A636  6898          r0 = r6 + 24
0x01E5A738  app+0x05A638  8017          r0 = r0.l (u)
0x01E5A73A  app+0x05A63A  02e12060      r2 = r6 + 32
0x01E5A73E  app+0x05A63E  a217          r2 = r2.l (u)
0x01E5A740  app+0x05A640  7b90          r3 = r7 + 16
0x01E5A742  app+0x05A642  b317          r3 = r3.l (u)
0x01E5A744  app+0x05A644  4116          r1 = r4
0x01E5A746  app+0x05A646  bfea3ab2      call -39820
0x01E5A74A  app+0x05A64A  d1ec582d      r2 = [r5+472]
0x01E5A74E  app+0x05A64E  4130          r1 = 16
0x01E5A750  app+0x05A650  e0d7          r0 = r6.l (u)  #
0x01E5A752  app+0x05A652  8120          [sp] = r1
0x01E5A754  app+0x05A654  c3ffffff0000  r3 = 65535
0x01E5A75A  app+0x05A65A  4116          r1 = r4
0x01E5A75C  app+0x05A65C  bfea0ef8      call -4068
0x01E5A760  app+0x05A660  0284          sp += 16
0x01E5A762  app+0x05A662  5804          {pc, r8-r4} = [sp++]
0x01E5A764  app+0x05A664  7404          [--sp] = {rets, r4}
0x01E5A766  app+0x05A666  e29d          sp += -12
0x01E5A768  app+0x05A668  41e0dd10      r1 = 4317
0x01E5A76C  app+0x05A66C  c2ffd01a0100  r2 = 72400
0x01E5A772  app+0x05A672  d8ee2011      r1 = b[r2+r1] (u)
0x01E5A776  app+0x05A676  0416          r4 = r0
0x01E5A778  app+0x05A678  0155          if (r1 == 0) goto 42
0x01E5A77A  app+0x05A67A  81f81502      if (r1 != 1) goto 42
