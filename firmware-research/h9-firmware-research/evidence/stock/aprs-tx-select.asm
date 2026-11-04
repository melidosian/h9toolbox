; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 161; undecoded chunks: 1
0x01E5C3D4  app+0x05C2D4  7904          [--sp] = {rets, r9-r4}
0x01E5C3D6  app+0x05C2D6  e29c          sp += -16
0x01E5C3D8  app+0x05C2D8  c0fff0c00000  r0 = 49392
0x01E5C3DE  app+0x05C2DE  50ee0a1d      r1 = b[r0+218] (u)
0x01E5C3E2  app+0x05C2E2  c5ffd01a0100  r5 = 72400
0x01E5C3E8  app+0x05C2E8  81f81002      if (r1 != 1) goto 32
0x01E5C3EC  app+0x05C2EC  41e0800a      r1 = 2688
0x01E5C3F0  app+0x05C2F0  d8ee5011      r1 = b[r5+r1] (u)
0x01E5C3F4  app+0x05C2F4  4420          r4 = 0
0x01E5C3F6  app+0x05C2F6  3153          if (r1 == 0) goto 230
0x01E5C3F8  app+0x05C2F8  41e0a202      r1 = 674
0x01E5C3FC  app+0x05C2FC  d8ec5a11      r1 = [r5+r1<<2]
0x01E5C400  app+0x05C300  42e0a402      r2 = 676
0x01E5C404  app+0x05C304  d8ec5a22      r2 = [r5+r2<<2]
0x01E5C408  app+0x05C308  2119          r1 |= r2
0x01E5C40A  app+0x05C30A  3149          if (r1 == 0) goto 210
0x01E5C40C  app+0x05C30C  41e04304      r1 = 1091
0x01E5C410  app+0x05C310  d8ee0021      r2 = b[r0+r1] (u)
0x01E5C414  app+0x05C314  4420          r4 = 0
0x01E5C416  app+0x05C316  02fc6208      if (r2 > 4) goto 196
0x01E5C41A  app+0x05C31A  50ee5018      r1 = b[r5+128] (u)
0x01E5C41E  app+0x05C31E  96a7          r6 = r1 >> 7
0x01E5C420  app+0x05C320  4020          r0 = 0
0x01E5C422  app+0x05C322  0201          tbb [r2]
0x01E5C424  app+0x05C324  0305          r3 = [r0++=4]
0x01E5C426  app+0x05C326  0908          [r0++=r8] = r1
0x01E5C428  app+0x05C328  1800          .byte 0x18, 0x00
0x01E5C42A  app+0x05C32A  0645          if (r6 == 0) goto 10
0x01E5C42C  app+0x05C32C  2498          goto 176
0x01E5C42E  app+0x05C32E  4021          r0 = 1
0x01E5C430  app+0x05C330  8642          if (r6 != 0) goto 4
0x01E5C432  app+0x05C332  2495          goto 170
0x01E5C434  app+0x05C334  4021          r0 = 1
0x01E5C436  app+0x05C336  49e00100      r9 = 1
0x01E5C43A  app+0x05C33A  10e80006      if (r0 == r6) {
0x01E5C43E  app+0x05C33E  c114          r9 = 0
0x01E5C440  app+0x05C340  50ee5c81      r8 = b[r5+28] (u)
0x01E5C444  app+0x05C344  90e80086      if (r0 != r6) {
0x01E5C448  app+0x05C348  b927          r1 &= 4294967167
0x01E5C44A  app+0x05C34A  a1e18403      r1 <= insert(r0, p:7, l:1)
0x01E5C44E  app+0x05C34E  52ee5018      b[r5+128] = r1
0x01E5C452  app+0x05C352  0484          goto 8
0x01E5C454  app+0x05C354  50ee5c81      r8 = b[r5+28] (u)
0x01E5C458  app+0x05C358  c114          r9 = 0
0x01E5C45A  app+0x05C35A  6016          r0 = r6
0x01E5C45C  app+0x05C35C  52ee5c01      b[r5+28] = r0
0x01E5C460  app+0x05C360  52ee5d04      b[r5+77] = r0
0x01E5C464  app+0x05C364  4020          r0 = 0
0x01E5C466  app+0x05C366  4720          r7 = 0
0x01E5C468  app+0x05C368  bfeae9e4      call -13870
0x01E5C46C  app+0x05C36C  d1ec5c0b      r0 = [r5+444]
0x01E5C470  app+0x05C370  d1ec5c1c      r1 = [r5+460]
0x01E5C474  app+0x05C374  42e05802      r2 = 600
0x01E5C478  app+0x05C378  4421          r4 = 1
0x01E5C47A  app+0x05C37A  d8ee5142      b[r5+r2] = r4
0x01E5C47E  app+0x05C37E  42e08907      r2 = 1929
0x01E5C482  app+0x05C382  d8ee5022      r2 = b[r5+r2] (u)
0x01E5C486  app+0x05C386  8120          [sp] = r1
0x01E5C488  app+0x05C388  8021          [sp+4] = r0
0x01E5C48A  app+0x05C38A  dee90820      b[sp+8] = r2
0x01E5C48E  app+0x05C38E  dee90970      b[sp+9] = r7
0x01E5C492  app+0x05C392  c0ff2ea8e501  r0 = 31828014
0x01E5C498  app+0x05C398  8023          [sp+12] = r0
0x01E5C49A  app+0x05C39A  8880          r0 = sp + 0
0x01E5C49C  app+0x05C39C  bfea64f4      call -5944
0x01E5C4A0  app+0x05C3A0  4832          r0 = 50
0x01E5C4A2  app+0x05C3A2  8deaad22      call 1721690
0x01E5C4A6  app+0x05C3A6  bfea41fd      call -1406
0x01E5C4AA  app+0x05C3AA  0046          if (r0 == 0) goto 12
0x01E5C4AC  app+0x05C3AC  bfea5eff      call -324
0x01E5C4B0  app+0x05C3B0  8017          r0 = r0.l (u)
0x01E5C4B2  app+0x05C3B2  8deaa522      call 1721674
0x01E5C4B6  app+0x05C3B6  0485          goto 10
0x01E5C4B8  app+0x05C3B8  40e0f401      r0 = 500
0x01E5C4BC  app+0x05C3BC  8deaa022      call 1721664
0x01E5C4C0  app+0x05C3C0  4420          r4 = 0
0x01E5C4C2  app+0x05C3C2  09f80c00      if (r9 == 0) goto 24
0x01E5C4C6  app+0x05C3C6  50ee5008      r0 = b[r5+128] (u)
0x01E5C4CA  app+0x05C3CA  b827          r0 &= 4294967167
0x01E5C4CC  app+0x05C3CC  a0e18463      r0 <= insert(r6, p:7, l:1)
0x01E5C4D0  app+0x05C3D0  52ee5008      b[r5+128] = r0
0x01E5C4D4  app+0x05C3D4  52ee5c81      b[r5+28] = r8
0x01E5C4D8  app+0x05C3D8  4020          r0 = 0
0x01E5C4DA  app+0x05C3DA  bfeab0e4      call -13984
0x01E5C4DE  app+0x05C3DE  4016          r0 = r4
0x01E5C4E0  app+0x05C3E0  0284          sp += 16
0x01E5C4E2  app+0x05C3E2  5904          {pc, r9-r4} = [sp++]
0x01E5C4E4  app+0x05C3E4  1004          [--sp] = rets
0x01E5C4E6  app+0x05C3E6  40e02204      r0 = 1058
0x01E5C4EA  app+0x05C3EA  c1fff0c00000  r1 = 49392
0x01E5C4F0  app+0x05C3F0  d8ee1000      r0 = b[r1+r0] (u)
0x01E5C4F4  app+0x05C3F4  b0e80140      if (r0 != 1) {
0x01E5C4F8  app+0x05C3F8  4020          r0 = 0
0x01E5C4FA  app+0x05C3FA  0004          pc = [sp++]
0x01E5C4FC  app+0x05C3FC  5824          r0 = 100
0x01E5C4FE  app+0x05C3FE  8dea7f22      call 1721598
0x01E5C502  app+0x05C402  8804          {rets} = [sp++]
0x01E5C504  app+0x05C404  b787          goto -306
0x01E5C506  app+0x05C406  1004          [--sp] = rets
0x01E5C508  app+0x05C408  bfea1cf6      call -5064
0x01E5C50C  app+0x05C40C  c0fff0c00000  r0 = 49392
0x01E5C512  app+0x05C412  41e0b003      r1 = 944
0x01E5C516  app+0x05C416  d8ee0011      r1 = b[r0+r1] (u)
0x01E5C51A  app+0x05C41A  81f81602      if (r1 != 1) goto 44
0x01E5C51E  app+0x05C41E  41e04404      r1 = 1092
0x01E5C522  app+0x05C422  d8ee0011      r1 = b[r0+r1] (u)
0x01E5C526  app+0x05C426  81f81002      if (r1 != 1) goto 32
0x01E5C52A  app+0x05C42A  41e02204      r1 = 1058
0x01E5C52E  app+0x05C42E  d8ee0001      r0 = b[r0+r1] (u)
0x01E5C532  app+0x05C432  80f80a02      if (r0 != 1) goto 20
0x01E5C536  app+0x05C436  40e05802      r0 = 600
0x01E5C53A  app+0x05C43A  c1ffd01a0100  r1 = 72400
0x01E5C540  app+0x05C440  d8ee1000      r0 = b[r1+r0] (u)
0x01E5C544  app+0x05C444  30ea0100      if ((r0 & 0x1) != 0) {
0x01E5C548  app+0x05C448  618d          call -102
0x01E5C54A  app+0x05C44A  0004          pc = [sp++]
0x01E5C54C  app+0x05C44C  7504          [--sp] = {rets, r5, r4}
0x01E5C54E  app+0x05C44E  4020          r0 = 0
0x01E5C550  app+0x05C450  bfeadb9f      call -49226
0x01E5C554  app+0x05C454  40e02611      r0 = 4390
0x01E5C558  app+0x05C458  c4ffd01a0100  r4 = 72400
0x01E5C55E  app+0x05C45E  4121          r1 = 1
0x01E5C560  app+0x05C460  d8ee4110      b[r4+r0] = r1
0x01E5C564  app+0x05C464  4021          r0 = 1
0x01E5C566  app+0x05C466  bfeaf69f      call -49172
0x01E5C56A  app+0x05C46A  bfea72a1      call -48412
0x01E5C56E  app+0x05C46E  c5fff0c00000  r5 = 49392
0x01E5C574  app+0x05C474  50ee520d      r0 = b[r5+210] (u)
0x01E5C578  app+0x05C478  0055          if (r0 == 0) goto 42
0x01E5C57A  app+0x05C47A  50ee4e04      r0 = b[r4+78] (u)
0x01E5C57E  app+0x05C47E  8052          if (r0 != 0) goto 36
0x01E5C580  app+0x05C480  c0ffc41a0100  r0 = 72388
0x01E5C586  app+0x05C486  0960          r1 = h[r0+0] (u)
0x01E5C588  app+0x05C488  0144          if (r1 == 0) goto 8
0x01E5C58A  app+0x05C48A  4120          r1 = 0
0x01E5C58C  app+0x05C48C  8960          h[r0+0] = r1
0x01E5C58E  app+0x05C48E  bfea2acd      call -26028
0x01E5C592  app+0x05C492  50ee4008      r0 = b[r4+128] (u)
0x01E5C596  app+0x05C496  80a7          r0 = r0 >> 7
0x01E5C598  app+0x05C498  52ee4d04      b[r4+77] = r0
0x01E5C59C  app+0x05C49C  5194          call -152
0x01E5C59E  app+0x05C49E  4021          r0 = 1
0x01E5C5A0  app+0x05C4A0  52ee530d      b[r5+211] = r0
0x01E5C5A4  app+0x05C4A4  5504          {pc, r5, r4} = [sp++]
0x01E5C5A6  app+0x05C4A6  7404          [--sp] = {rets, r4}
0x01E5C5A8  app+0x05C4A8  e29b          sp += -20
0x01E5C5AA  app+0x05C4AA  40e0dd10      r0 = 4317
0x01E5C5AE  app+0x05C4AE  c1ffd01a0100  r1 = 72400
0x01E5C5B4  app+0x05C4B4  d8ee1000      r0 = b[r1+r0] (u)
0x01E5C5B8  app+0x05C4B8  00fc2d02      if (r0 > 1) goto 90
0x01E5C5BC  app+0x05C4BC  c4ffc829eb01  r4 = 32188872
0x01E5C5C2  app+0x05C4C2  888c          r0 = sp + 12
0x01E5C5C4  app+0x05C4C4  4116          r1 = r4
0x01E5C5C6  app+0x05C4C6  bfea18a3      call -47568
0x01E5C5CA  app+0x05C4CA  4993          r1 = r4 + 19
0x01E5C5CC  app+0x05C4CC  8884          r0 = sp + 4
0x01E5C5CE  app+0x05C4CE  bfea14a3      call -47576
0x01E5C5D2  app+0x05C4D2  d8e91230      r3 = h[sp+18] (u)
0x01E5C5D6  app+0x05C4D6  d8e90e10      r1 = h[sp+14] (u)
0x01E5C5DA  app+0x05C4DA  d8e91020      r2 = h[sp+16] (u)
0x01E5C5DE  app+0x05C4DE  d8e90c00      r0 = h[sp+12] (u)
0x01E5C5E2  app+0x05C4E2  4420          r4 = 0
