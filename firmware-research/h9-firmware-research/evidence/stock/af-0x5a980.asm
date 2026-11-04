; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 44; undecoded chunks: 1
0x01E5AA80  app+0x05A980  4020          r0 = 0
0x01E5AA82  app+0x05A982  81f80e02      if (r1 != 1) goto 28
0x01E5AA86  app+0x05A986  40e03203      r0 = 818
0x01E5AA8A  app+0x05A98A  d8ed5820      r2 = h[r5+r0<<1] (u)
0x01E5AA8E  app+0x05A98E  60e10f20      r0 = r2 & 0xF
0x01E5AA92  app+0x05A992  41e080b1      r1 = -20096
0x01E5AA96  app+0x05A996  0119          r1 |= r0
0x01E5AA98  app+0x05A998  40e08fb1      r0 = -20081
0x01E5AA9C  app+0x05A99C  32ea0f00      if ((r2 & 0xF) != 0) {
0x01E5AAA0  app+0x05A9A0  1016          r0 = r1
0x01E5AAA2  app+0x05A9A2  8117          r1 = r0.l (u)
0x01E5AAA4  app+0x05A9A4  5028          r0 = 72
0x01E5AAA6  app+0x05A9A6  bfea30ae      call -41888
0x01E5AAAA  app+0x05A9AA  4023          r0 = 3
0x01E5AAAC  app+0x05A9AC  bfea9af4      call -5836
0x01E5AAB0  app+0x05A9B0  4036          r0 = 22
0x01E5AAB2  app+0x05A9B2  52ee5604      b[r5+70] = r0
0x01E5AAB6  app+0x05A9B6  c1ffd8d80000  r1 = 55512
0x01E5AABC  app+0x05A9BC  5830          r0 = 112
0x01E5AABE  app+0x05A9BE  bfea24ae      call -41912
0x01E5AAC2  app+0x05A9C2  5196          call -148
0x01E5AAC4  app+0x05A9C4  519b          call -138
0x01E5AAC6  app+0x05A9C6  4016          r0 = r4
0x01E5AAC8  app+0x05A9C8  5504          {pc, r5, r4} = [sp++]
0x01E5AACA  app+0x05A9CA  7504          [--sp] = {rets, r5, r4}
0x01E5AACC  app+0x05A9CC  74e1f000      r4 = r0 & 0xFFFFFF0F
0x01E5AAD0  app+0x05A9D0  c5ff682eeb01  r5 = 32190056
0x01E5AAD6  app+0x05A9D6  d8ed5814      r1 = h[r5+r4<<1] (u)
0x01E5AADA  app+0x05A9DA  5831          r0 = 113
0x01E5AADC  app+0x05A9DC  bfea15ae      call -41942
0x01E5AAE0  app+0x05A9E0  00e12050      r0 = r5 + 32
0x01E5AAE4  app+0x05A9E4  d8ed0814      r1 = h[r0+r4<<1] (u)
0x01E5AAE8  app+0x05A9E8  5832          r0 = 114
0x01E5AAEA  app+0x05A9EA  3504          {rets, r5, r4} = [sp++]
0x01E5AAEC  app+0x05A9EC  ffea0dae      goto -41958
0x01E5AAF0  app+0x05A9F0  c1fff0c00000  r1 = 49392
0x01E5AAF6  app+0x05A9F6  50ee1f24      r2 = b[r1+79] (u)
0x01E5AAFA  app+0x05A9FA  8243          if (r2 != 0) goto 6
0x01E5AAFC  app+0x05A9FC  4221          r2 = 1
0x01E5AAFE  app+0x05A9FE  52ee1f24      b[r1+79] = r2
0x01E5AB02  app+0x05AA02  c021          r0 += 1
0x01E5AB04  app+0x05AA04  50ee1f24      r2 = b[r1+79] (u)
0x01E5AB08  app+0x05AA08  80e9fc21      if (r2 < r0) goto -8
0x01E5AB0C  app+0x05AA0C  4020          r0 = 0
0x01E5AB0E  app+0x05AA0E  52ee          .byte 0x52, 0xEE
