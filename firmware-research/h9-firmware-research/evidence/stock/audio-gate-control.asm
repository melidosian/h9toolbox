; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 153; undecoded chunks: 0
0x01E5234C  app+0x05224C  7504          [--sp] = {rets, r5, r4}
0x01E5234E  app+0x05224E  c5ffd01a0100  r5 = 72400
0x01E52354  app+0x052254  50ee5d27      r2 = b[r5+125] (u)
0x01E52358  app+0x052258  73e1fe20      r3 = r2 & 0xFFFFFF01
0x01E5235C  app+0x05225C  1416          r4 = r1
0x01E5235E  app+0x05225E  04e85b30      if (r3 == r4) goto 182
0x01E52362  app+0x052262  4121          r1 = 1
0x01E52364  app+0x052264  0441          if (r4 == 0) goto 2
0x01E52366  app+0x052266  4120          r1 = 0
0x01E52368  app+0x052268  00f8041a      if (r0 == 13) goto 8
0x01E5236C  app+0x05226C  b3e10421      r3 = uextra(r2, p:2, l:1)
0x01E52370  app+0x052270  b119          r1 &= r3
0x01E52372  app+0x052272  a152          if (r1 != 0) goto 164
0x01E52374  app+0x052274  31e1f50f      r1 = r0 + -11
0x01E52378  app+0x052278  81f91204      if (r1 < 2) goto 36
0x01E5237C  app+0x05227C  00f81002      if (r0 == 1) goto 32
0x01E52380  app+0x052280  00f8061a      if (r0 == 13) goto 12
0x01E52384  app+0x052284  00f80c10      if (r0 == 8) goto 24
0x01E52388  app+0x052288  4016          r0 = r4
0x01E5238A  app+0x05228A  bfea3aff      call -396
0x01E5238E  app+0x05228E  048b          goto 22
0x01E52390  app+0x052290  70e10420      r0 = r2 & 0xFFFFFFFB
0x01E52394  app+0x052294  41a2          r1 = r4 << 2
0x01E52396  app+0x052296  61e10410      r1 = r1 & 0x4
0x01E5239A  app+0x05229A  1019          r0 |= r1
0x01E5239C  app+0x05229C  52ee5d07      b[r5+125] = r0
0x01E523A0  app+0x0522A0  4021          r0 = 1
0x01E523A2  app+0x0522A2  8441          if (r4 != 0) goto 2
0x01E523A4  app+0x0522A4  4016          r0 = r4
0x01E523A6  app+0x0522A6  1454          if (r4 == 0) goto 104
0x01E523A8  app+0x0522A8  1053          if (r0 == 0) goto 102
0x01E523AA  app+0x0522AA  c0fff0c00000  r0 = 49392
0x01E523B0  app+0x0522B0  50ee020c      r0 = b[r0+194] (u)
0x01E523B4  app+0x0522B4  80f82a04      if (r0 != 2) goto 84
0x01E523B8  app+0x0522B8  40e0a40b      r0 = 2980
0x01E523BC  app+0x0522BC  d8ee5000      r0 = b[r5+r0] (u)
0x01E523C0  app+0x0522C0  70e13f00      r0 = r0 & 0xFFFFFFC0
0x01E523C4  app+0x0522C4  90f82200      if (r0 != 128) goto 68
0x01E523C8  app+0x0522C8  beeab022      call -244384
0x01E523CC  app+0x0522CC  80f81e5e      if (r0 != 47) goto 60
0x01E523D0  app+0x0522D0  40e02511      r0 = 4389
0x01E523D4  app+0x0522D4  d8ee5000      r0 = b[r5+r0] (u)
0x01E523D8  app+0x0522D8  41e02711      r1 = 4391
0x01E523DC  app+0x0522DC  d8ee5011      r1 = b[r5+r1] (u)
0x01E523E0  app+0x0522E0  1019          r0 |= r1
0x01E523E2  app+0x0522E2  61ffff001200  if ((r0 & 0xFF) != 0) goto 36
0x01E523E8  app+0x0522E8  50ee5305      r0 = b[r5+83] (u)
0x01E523EC  app+0x0522EC  804f          if (r0 != 0) goto 30
0x01E523EE  app+0x0522EE  40e02711      r0 = 4391
0x01E523F2  app+0x0522F2  4121          r1 = 1
0x01E523F4  app+0x0522F4  d8ee5110      b[r5+r0] = r1
0x01E523F8  app+0x0522F8  4021          r0 = 1
0x01E523FA  app+0x0522FA  bfea03f2      call -7162
0x01E523FE  app+0x0522FE  4020          r0 = 0
0x01E52400  app+0x052300  bfea0ef2      call -7140
0x01E52404  app+0x052304  4022          r0 = 2
0x01E52406  app+0x052306  4120          r1 = 0
0x01E52408  app+0x052308  bfea7ce4      call -14088
0x01E5240C  app+0x05230C  4021          r0 = 1
0x01E5240E  app+0x05230E  0483          goto 6
0x01E52410  app+0x052310  4019          r0 |= r4
0x01E52412  app+0x052312  8042          if (r0 != 0) goto 4
0x01E52414  app+0x052314  4020          r0 = 0
0x01E52416  app+0x052316  4188          call -240
0x01E52418  app+0x052318  5504          {pc, r5, r4} = [sp++]
0x01E5241A  app+0x05231A  7804          [--sp] = {rets, r8-r4}
0x01E5241C  app+0x05231C  0616          r6 = r0
0x01E5241E  app+0x05231E  4023          r0 = 3
0x01E52420  app+0x052320  4121          r1 = 1
0x01E52422  app+0x052322  4194          call -216
0x01E52424  app+0x052324  40e0e610      r0 = 4326
0x01E52428  app+0x052328  c7ffd01a0100  r7 = 72400
0x01E5242E  app+0x05232E  d8ee7000      r0 = b[r7+r0] (u)
0x01E52432  app+0x052332  f83f          r0 += -1
0x01E52434  app+0x052334  0117          r1 = r0.b0 (u)
0x01E52436  app+0x052336  c4ffa05ceb01  r4 = 32201888
0x01E5243C  app+0x05233C  b1ec0790      if (r1 <= 7) {
0x01E52440  app+0x052340  0817          r0 = r0.b0 (s)
0x01E52442  app+0x052342  c1ffb0f0ea01  r1 = 32174256
0x01E52448  app+0x052348  d8ec1a00      r0 = [r1+r0<<2]
0x01E5244C  app+0x05234C  4016          r0 = r4
0x01E5244E  app+0x05234E  d8ec0a56      r5 = [r0+r6<<2]
0x01E52452  app+0x052352  154f          if (r5 == 0) goto 94
0x01E52454  app+0x052354  c8ff8513eb01  r8 = 32183173
0x01E5245A  app+0x05235A  5016          r0 = r5
0x01E5245C  app+0x05235C  8116          r1 = r8
0x01E5245E  app+0x05235E  beea6694      call -186164
0x01E52462  app+0x052362  1048          if (r0 == 0) goto 80
0x01E52464  app+0x052364  beeaee93      call -186404
0x01E52468  app+0x052368  d0ec7c1e      r1 = [r7+236]
0x01E5246C  app+0x05236C  1142          if (r1 == 0) goto 68
0x01E5246E  app+0x05236E  1841          r0 = b[r1+1] (u)
0x01E52470  app+0x052370  1040          if (r0 == 0) goto 64
0x01E52472  app+0x052372  8214          r3_r2 = 0
0x01E52474  app+0x052374  4420          r4 = 0
0x01E52476  app+0x052376  23ff40540900  if (r5 < 3221225472) goto 18
0x01E5247C  app+0x05237C  4321          r3 = 1
0x01E5247E  app+0x05237E  35eaff00      if ((r5 & 0xFF) != 0) {
0x01E52482  app+0x052382  4320          r3 = 0
0x01E52484  app+0x052384  14e1d782      r4 = r8 + 4823
0x01E52488  app+0x052388  35eaff00      if ((r5 & 0xFF) != 0) {
0x01E5248C  app+0x05238C  4420          r4 = 0
0x01E5248E  app+0x05238E  8dead178      call 1765794
0x01E52492  app+0x052392  9561          [r1+4] = r5
0x01E52494  app+0x052394  9462          [r1+8] = r4
0x01E52496  app+0x052396  9b4c          b[r1+12] = r3
0x01E52498  app+0x052398  9a4d          b[r1+13] = r2
0x01E5249A  app+0x05239A  9a67          h[r1+14] = r2
0x01E5249C  app+0x05239C  9264          [r1+16] = r2
0x01E5249E  app+0x05239E  9265          [r1+20] = r2
0x01E524A0  app+0x0523A0  4021          r0 = 1
0x01E524A2  app+0x0523A2  9842          b[r1+2] = r0
0x01E524A4  app+0x0523A4  8dead478      call 1765800
0x01E524A8  app+0x0523A8  00e14987      r0 = r8 + 1865
0x01E524AC  app+0x0523AC  4121          r1 = 1
0x01E524AE  app+0x0523AE  8dea3f83      call 1771134
0x01E524B2  app+0x0523B2  5804          {pc, r8-r4} = [sp++]
0x01E524B4  app+0x0523B4  40e0e610      r0 = 4326
0x01E524B8  app+0x0523B8  d8ee7000      r0 = b[r7+r0] (u)
0x01E524BC  app+0x0523BC  6055          if (r0 == 0) goto -86
0x01E524BE  app+0x0523BE  d8ec4a56      r5 = [r4+r6<<2]
0x01E524C2  app+0x0523C2  e552          if (r5 != 0) goto -92
0x01E524C4  app+0x0523C4  5804          {pc, r8-r4} = [sp++]
0x01E524C6  app+0x0523C6  1004          [--sp] = rets
0x01E524C8  app+0x0523C8  4023          r0 = 3
0x01E524CA  app+0x0523CA  bfead3e3      call -14426
0x01E524CE  app+0x0523CE  32e13d0c      r2 = r0 + -963
0x01E524D2  app+0x0523D2  c1fff0c00000  r1 = 49392
0x01E524D8  app+0x0523D8  02fc0b78      if (r2 > 60) goto 22
0x01E524DC  app+0x0523DC  402d          r0 = 13
0x01E524DE  app+0x0523DE  01e1ac12      r1 = r1 + 684
0x01E524E2  app+0x0523E2  9841          b[r1+1] = r0
0x01E524E4  app+0x0523E4  4220          r2 = 0
0x01E524E6  app+0x0523E6  9a62          h[r1+4] = r2
0x01E524E8  app+0x0523E8  9847          b[r1+7] = r0
0x01E524EA  app+0x0523EA  9a65          h[r1+10] = r2
0x01E524EC  app+0x0523EC  984d          b[r1+13] = r0
0x01E524EE  app+0x0523EE  9a68          h[r1+16] = r2
0x01E524F0  app+0x0523F0  0004          pc = [sp++]
0x01E524F2  app+0x0523F2  32e1c10f      r2 = r0 + -63
0x01E524F6  app+0x0523F6  02fc08f0      if (r2 > 120) goto 16
0x01E524FA  app+0x0523FA  40e0ad02      r0 = 685
0x01E524FE  app+0x0523FE  422e          r2 = 14
0x01E52500  app+0x052400  d8ee1120      b[r1+r0] = r2
0x01E52504  app+0x052404  40e05801      r0 = 344
0x01E52508  app+0x052408  048b          goto 22
0x01E5250A  app+0x05240A  32e1980e      r2 = r0 + -360
0x01E5250E  app+0x05240E  02fc10f0      if (r2 > 120) goto 32
0x01E52512  app+0x052412  40e0b302      r0 = 691
0x01E52516  app+0x052416  422e          r2 = 14
0x01E52518  app+0x052418  d8ee1120      b[r1+r0] = r2
0x01E5251C  app+0x05241C  40e05b01      r0 = 347
0x01E52520  app+0x052420  d8ed1820      r2 = h[r1+r0<<1] (u)
