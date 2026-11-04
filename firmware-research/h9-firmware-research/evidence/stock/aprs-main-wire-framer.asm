; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 86; undecoded chunks: 0
0x01E5B3E0  app+0x05B2E0  e1e10710      r1 = r1 * 0x7
0x01E5B3E4  app+0x05B2E4  4118          r1 += r4
0x01E5B3E6  app+0x05B2E6  1a8e          r2 = r1 + 14
0x01E5B3E8  app+0x05B2E8  8117          r1 = r0.l (u)
0x01E5B3EA  app+0x05B2EA  5016          r0 = r5
0x01E5B3EC  app+0x05B2EC  518e          call -164
0x01E5B3EE  app+0x05B2EE  c621          r6 += 1
0x01E5B3F0  app+0x05B2F0  61d7          r1 = r6.b0 (u)  #
0x01E5B3F2  app+0x05B2F2  7a40          r2 = b[r7+0] (u)
0x01E5B3F4  app+0x05B2F4  82e9ef11      if (r1 < r2) goto -34
0x01E5B3F8  app+0x05B2F8  8117          r1 = r0.l (u)
0x01E5B3FA  app+0x05B2FA  4223          r2 = 3
0x01E5B3FC  app+0x05B2FC  d8ee5121      b[r5+r1] = r2
0x01E5B400  app+0x05B300  7a42          r2 = b[r7+2] (u)
0x01E5B402  app+0x05B302  1130          r1 = -16
0x01E5B404  app+0x05B304  0241          if (r2 == 0) goto 2
0x01E5B406  app+0x05B306  2116          r1 = r2
0x01E5B408  app+0x05B308  0a81          r2 = r0 + 1
0x01E5B40A  app+0x05B30A  a217          r2 = r2.l (u)
0x01E5B40C  app+0x05B30C  d8ee5112      b[r5+r2] = r1
0x01E5B410  app+0x05B310  50ed8020      r2 = h[r8+0] (u)
0x01E5B414  app+0x05B314  0e82          r6 = r0 + 2
0x01E5B416  app+0x05B316  0249          if (r2 == 0) goto 18
0x01E5B418  app+0x05B318  e017          r0 = r6.l (u)
0x01E5B41A  app+0x05B31A  5018          r0 += r5
0x01E5B41C  app+0x05B31C  01e13b40      r1 = r4 + 59
0x01E5B420  app+0x05B320  95eac2b9      call 2847620
0x01E5B424  app+0x05B324  50ed8000      r0 = h[r8+0] (u)
0x01E5B428  app+0x05B328  0618          r6 += r0
0x01E5B42A  app+0x05B32A  e017          r0 = r6.l (u)
0x01E5B42C  app+0x05B32C  5804          {pc, r8-r4} = [sp++]
0x01E5B42E  app+0x05B32E  7804          [--sp] = {rets, r8-r4}
0x01E5B430  app+0x05B330  4420          r4 = 0
0x01E5B432  app+0x05B332  1159          if (r1 == 0) goto 114
0x01E5B434  app+0x05B334  1358          if (r3 == 0) goto 112
0x01E5B436  app+0x05B336  0526          r5 = [sp+24]
0x01E5B438  app+0x05B338  26a1          r6 = r2 << 1
0x01E5B43A  app+0x05B33A  c62a          r6 += 10
0x01E5B43C  app+0x05B33C  86e93350      if (r5 < r6) goto 102
0x01E5B440  app+0x05B340  7620          r6 = 192
0x01E5B442  app+0x05B342  0483          goto 6
0x01E5B444  app+0x05B344  d8ee3164      b[r3+r4] = r6
0x01E5B448  app+0x05B348  c421          r4 += 1
0x01E5B44A  app+0x05B34A  84f8fb0b      if (r4 != 5) goto -10
0x01E5B44E  app+0x05B34E  34f1fc5f      r4 = r5 + -4  #
0x01E5B452  app+0x05B352  b845          b[r3+5] = r0
0x01E5B454  app+0x05B354  4026          r0 = 6
0x01E5B456  app+0x05B356  4520          r5 = 0
0x01E5B458  app+0x05B358  48e0db00      r8 = 219
0x01E5B45C  app+0x05B35C  02e91750      if (r5 >= r2) goto 46
0x01E5B460  app+0x05B360  d8ee1075      r7 = b[r1+r5] (u)
0x01E5B464  app+0x05B364  17f804b6      if (r7 == 219) goto 8
0x01E5B468  app+0x05B368  97f80780      if (r7 != 192) goto 14
0x01E5B46C  app+0x05B36C  773c          r7 = 220
0x01E5B46E  app+0x05B36E  0481          goto 2
0x01E5B470  app+0x05B370  773d          r7 = 221
0x01E5B472  app+0x05B372  8617          r6 = r0.l (u)
0x01E5B474  app+0x05B374  d8ee3186      b[r3+r6] = r8
0x01E5B478  app+0x05B378  c021          r0 += 1
0x01E5B47A  app+0x05B37A  8617          r6 = r0.l (u)
0x01E5B47C  app+0x05B37C  d8ee3176      b[r3+r6] = r7
0x01E5B480  app+0x05B380  c021          r0 += 1
0x01E5B482  app+0x05B382  8617          r6 = r0.l (u)
0x01E5B484  app+0x05B384  c521          r5 += 1
0x01E5B486  app+0x05B386  84ede961      ifs (r6 < r4) goto -46
0x01E5B48A  app+0x05B38A  4420          r4 = 0
0x01E5B48C  app+0x05B38C  048c          goto 24
0x01E5B48E  app+0x05B38E  0c83          r4 = r0 + 3
0x01E5B490  app+0x05B390  4120          r1 = 0
0x01E5B492  app+0x05B392  7220          r2 = 192
0x01E5B494  app+0x05B394  0485          goto 10
0x01E5B496  app+0x05B396  0d1c          r5 = r0 + r1
0x01E5B498  app+0x05B398  d517          r5 = r5.l (u)
0x01E5B49A  app+0x05B39A  d8ee3125      b[r3+r5] = r2
0x01E5B49E  app+0x05B39E  c121          r1 += 1
0x01E5B4A0  app+0x05B3A0  9517          r5 = r1.l (u)
0x01E5B4A2  app+0x05B3A2  85f8f807      if (r5 != 3) goto -16
0x01E5B4A6  app+0x05B3A6  c017          r0 = r4.l (u)
0x01E5B4A8  app+0x05B3A8  5804          {pc, r8-r4} = [sp++]
0x01E5B4AA  app+0x05B3AA  1004          [--sp] = rets
0x01E5B4AC  app+0x05B3AC  c2ffd01a0100  r2 = 72400
0x01E5B4B2  app+0x05B3B2  d2ec2c22      r2 = [r2+556]
0x01E5B4B6  app+0x05B3B6  0242          if (r2 == 0) goto 4
0x01E5B4B8  app+0x05B3B8  2265          r2 = [r2+20]
0x01E5B4BA  app+0x05B3BA  c200          call r2
0x01E5B4BC  app+0x05B3BC  0004          pc = [sp++]
