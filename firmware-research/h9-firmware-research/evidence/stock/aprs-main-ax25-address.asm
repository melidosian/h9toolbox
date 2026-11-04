; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 31; undecoded chunks: 0
0x01E5B34A  app+0x05B24A  7804          [--sp] = {rets, r8-r4}
0x01E5B34C  app+0x05B24C  4420          r4 = 0
0x01E5B34E  app+0x05B24E  1516          r5 = r1
0x01E5B350  app+0x05B250  048c          goto 24
0x01E5B352  app+0x05B252  d8ee2074      r7 = b[r2+r4] (u)
0x01E5B356  app+0x05B256  c0e17180      r8 = r7 << 1
0x01E5B35A  app+0x05B25A  5620          r6 = 64
0x01E5B35C  app+0x05B25C  0741          if (r7 == 0) goto 2
0x01E5B35E  app+0x05B25E  8616          r6 = r8
0x01E5B360  app+0x05B260  d717          r7 = r5.l (u)
0x01E5B362  app+0x05B262  d8ee0167      b[r0+r7] = r6
0x01E5B366  app+0x05B266  c421          r4 += 1
0x01E5B368  app+0x05B268  c521          r5 += 1
0x01E5B36A  app+0x05B26A  84f8f20d      if (r4 != 6) goto -28
0x01E5B36E  app+0x05B26E  4421          r4 = 1
0x01E5B370  app+0x05B270  8341          if (r3 != 0) goto 2
0x01E5B372  app+0x05B272  3416          r4 = r3
0x01E5B374  app+0x05B274  2a46          r2 = b[r2+6] (u)
0x01E5B376  app+0x05B276  73e17f20      r3 = r2 & 0xFFFFFF80
0x01E5B37A  app+0x05B27A  4319          r3 |= r4
0x01E5B37C  app+0x05B27C  22a1          r2 = r2 << 1
0x01E5B37E  app+0x05B27E  62e11e20      r2 = r2 & 0x1E
0x01E5B382  app+0x05B282  3219          r2 |= r3
0x01E5B384  app+0x05B284  42e16020      r2 = r2 | 0x60
0x01E5B388  app+0x05B288  1b86          r3 = r1 + 6
0x01E5B38A  app+0x05B28A  b317          r3 = r3.l (u)
0x01E5B38C  app+0x05B28C  c127          r1 += 7
0x01E5B38E  app+0x05B28E  9117          r1 = r1.l (u)
0x01E5B390  app+0x05B290  d8ee0123      b[r0+r3] = r2
0x01E5B394  app+0x05B294  1016          r0 = r1
0x01E5B396  app+0x05B296  5804          {pc, r8-r4} = [sp++]
