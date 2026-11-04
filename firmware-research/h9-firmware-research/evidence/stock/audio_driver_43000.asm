; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 856; undecoded chunks: 3
0x01E43100  app+0x043000  c1ffa63ae401  r1 = 31734438
0x01E43106  app+0x043006  0062          r0 = [r0+8]
0x01E43108  app+0x043008  b0f10c02      r0 = uextra(r0, p:4, l:3)  #
0x01E4310C  app+0x04300C  5260          r2 = [r5+0]
0x01E4310E  app+0x04300E  d8ee1000      r0 = b[r1+r0] (u)
0x01E43112  app+0x043012  70e1f800      r0 = r0 & 0xFFFFFF07
0x01E43116  app+0x043016  71e1e026      r1 = r2 & 0xF8FFFFFF
0x01E4311A  app+0x04301A  a1e10c0c      r1 <= insert(r0, p:24, l:3)
0x01E4311E  app+0x04301E  d160          [r5+0] = r1
0x01E43120  app+0x043020  00ef0059      [r5+0] |= 0x200000
0x01E43124  app+0x043024  40e0e803      r0 = 1000
0x01E43128  app+0x043028  80ea163d      call 31276
0x01E4312C  app+0x04302C  c0ff01048000  r0 = 8389633
0x01E43132  app+0x043032  64e80050      [r5+0] |= r0
0x01E43136  app+0x043036  4025          r0 = 5
0x01E43138  app+0x043038  8dea23f7      call 1830470
0x01E4313C  app+0x04303C  4061          r0 = [r4+4]
0x01E4313E  app+0x04303E  084a          r0 = b[r0+10] (u)
0x01E43140  app+0x043040  30ea0400      if ((r0 & 0x4) != 0) {
0x01E43144  app+0x043044  c3ef005f      [r5+12] &= 0xFFFFFDFF
0x01E43148  app+0x043048  c0ff02002000  r0 = 2097154
0x01E4314E  app+0x04304E  64e80050      [r5+0] |= r0
0x01E43152  app+0x043052  c3ef8059      [r5+12] &= 0xFFEFFFFF
0x01E43156  app+0x043056  40e0e803      r0 = 1000
0x01E4315A  app+0x04305A  80eafd3c      call 31226
0x01E4315E  app+0x04305E  c3ef805e      [r5+12] &= 0xFFFFFBFF
0x01E43162  app+0x043062  4021          r0 = 1
0x01E43164  app+0x043064  c840          b[r4+0] = r0
0x01E43166  app+0x043066  5504          {pc, r5, r4} = [sp++]
0x01E43168  app+0x043068  01fc1310      if (r1 > 8) goto 38
0x01E4316C  app+0x04306C  c2ff282f1e00  r2 = 1978152
0x01E43172  app+0x043072  0101          tbb [r1]
0x01E43174  app+0x043074  0508          r5 = [r0++=r8]
0x01E43176  app+0x043076  0b0d          h[r0++=r10] = r3
0x01E43178  app+0x043078  0f0f          h[r0++=r14] = r7
0x01E4317A  app+0x04307A  0f0f          h[r0++=r14] = r7
0x01E4317C  app+0x04307C  1000          .byte 0x10, 0x00
0x01E4317E  app+0x04307E  8017          r0 = r0.l (u)
0x01E43180  app+0x043080  a060          [r2+0] = r0
0x01E43182  app+0x043082  8000          rts
0x01E43184  app+0x043084  00b0          r0 = r0 << 16
0x01E43186  app+0x043086  a060          [r2+0] = r0
0x01E43188  app+0x043088  8000          rts
0x01E4318A  app+0x04308A  8017          r0 = r0.l (u)
0x01E4318C  app+0x04308C  0481          goto 2
0x01E4318E  app+0x04308E  00b0          r0 = r0 << 16
0x01E43190  app+0x043090  a061          [r2+4] = r0
0x01E43192  app+0x043092  8000          rts
0x01E43194  app+0x043094  a864          h[r2+8] = r0
0x01E43196  app+0x043096  8000          rts
0x01E43198  app+0x043098  7404          [--sp] = {rets, r4}
0x01E4319A  app+0x04309A  4120          r1 = 0
0x01E4319C  app+0x04309C  422a          r2 = 10
0x01E4319E  app+0x04309E  c3ffdc191e00  r3 = 1972700
0x01E431A4  app+0x0430A4  4420          r4 = 0
0x01E431A6  app+0x0430A6  0489          goto 18
0x01E431A8  app+0x0430A8  3060          r0 = [r3+0]
0x01E431AA  app+0x0430AA  20ea801f      if ((r0 & 0x100) == 0) {
0x01E431AE  app+0x0430AE  c121          r1 += 1
0x01E431B0  app+0x0430B0  c421          r4 += 1
0x01E431B2  app+0x0430B2  402a          r0 = 10
0x01E431B4  app+0x0430B4  80ead03c      call 31136
0x01E431B8  app+0x0430B8  fa3f          r2 += -1
0x01E431BA  app+0x0430BA  61ffff20f4ff  if ((r2 & 0xFF) != 0) goto -24
0x01E431C0  app+0x0430C0  1117          r1 = r1.b0 (u)
0x01E431C2  app+0x0430C2  4217          r2 = r4.b0 (u)
0x01E431C4  app+0x0430C4  4021          r0 = 1
0x01E431C6  app+0x0430C6  92e90001      if (r2 < r1) {
0x01E431CA  app+0x0430CA  4020          r0 = 0
0x01E431CC  app+0x0430CC  5404          {pc, r4} = [sp++]
0x01E431CE  app+0x0430CE  7704          [--sp] = {rets, r7-r4}
0x01E431D0  app+0x0430D0  1416          r4 = r1
0x01E431D2  app+0x0430D2  c1ffcc191e00  r1 = 1972684
0x01E431D8  app+0x0430D8  84f80510      if (r4 != 8) goto 10
0x01E431DC  app+0x0430DC  1260          r2 = [r1+0]
0x01E431DE  app+0x0430DE  72e1e02c      r2 = r2 & 0xFFFF8FFF
0x01E431E2  app+0x0430E2  322d          r2 |= 8192
0x01E431E4  app+0x0430E4  048d          goto 26
0x01E431E6  app+0x0430E6  22e13418      r2 = r1 + -6092
0x01E431EA  app+0x0430EA  2360          r3 = [r2+0]
0x01E431EC  app+0x0430EC  1260          r2 = [r1+0]
0x01E431EE  app+0x0430EE  8446          if (r4 != 0) goto 12
0x01E431F0  app+0x0430F0  3317          r3 = r3.b0 (u)
0x01E431F2  app+0x0430F2  83f80302      if (r3 != 1) goto 6
0x01E431F6  app+0x0430F6  42e1e02c      r2 = r2 | 0x7000
0x01E431FA  app+0x0430FA  0482          goto 4
0x01E431FC  app+0x0430FC  a2e10c46      r2 <= insert(r4, p:12, l:3)
0x01E43200  app+0x043100  9260          [r1+0] = r2
0x01E43202  app+0x043102  70f1fe00      r0 = r0 & 0xFFFFFF01  #
0x01E43206  app+0x043106  1260          r2 = [r1+0]
0x01E43208  app+0x043108  ba2f          r2 &= 4294934527
0x01E4320A  app+0x04310A  a2e18407      r2 <= insert(r0, p:15, l:1)
0x01E4320E  app+0x04310E  40f00008      r0 = 2048  #
0x01E43212  app+0x043112  9260          [r1+0] = r2
0x01E43214  app+0x043114  0716          r7 = r0
0x01E43216  app+0x043116  4116          r1 = r4
0x01E43218  app+0x043118  5187          call -178
0x01E4321A  app+0x04311A  40e0e803      r0 = 1000
0x01E4321E  app+0x04311E  80ea9b3c      call 31030
0x01E43222  app+0x043122  519a          call -140
0x01E43224  app+0x043124  0516          r5 = r0
0x01E43226  app+0x043126  46e0fe07      r6 = 2046
0x01E4322A  app+0x04312A  f017          r0 = r7.l (u)
0x01E4322C  app+0x04312C  20ff780c0b00  if (r0 == 63488) goto 22
0x01E43232  app+0x043132  6016          r0 = r6
0x01E43234  app+0x043134  4116          r1 = r4
0x01E43236  app+0x043136  4198          call -208
0x01E43238  app+0x043138  402a          r0 = 10
0x01E4323A  app+0x04313A  80ea8d3c      call 31002
0x01E4323E  app+0x04313E  ff3e          r7 += -2
0x01E43240  app+0x043140  fe3e          r6 += -2
0x01E43242  app+0x043142  518a          call -172
0x01E43244  app+0x043144  00e8f151      if (r5 == r0) goto -30
0x01E43248  app+0x043148  f817          r0 = r7.l (s)
0x01E4324A  app+0x04314A  5704          {pc, r7-r4} = [sp++]
0x01E4324C  app+0x04314C  7d04          [--sp] = {rets, r13-r4}
0x01E4324E  app+0x04314E  c1ff00021e00  r1 = 1966592
0x01E43254  app+0x043154  0dd6          r13 = r0  #
0x01E43256  app+0x043156  1460          r4 = [r1+0]
0x01E43258  app+0x043158  ccffd01a0100  r12 = 72400
0x01E4325E  app+0x04315E  15e1ecc9      r5 = r12 + 6636
0x01E43262  app+0x043162  5016          r0 = r5
0x01E43264  app+0x043164  bfea5efe      call -836
0x01E43268  app+0x043168  4021          r0 = 1
0x01E4326A  app+0x04316A  c6ff002f1e00  r6 = 1978112
0x01E43270  app+0x043170  40f09d06      r0 = 1693  #
0x01E43274  app+0x043174  e062          [r6+8] = r0
0x01E43276  app+0x043176  d8ecca00      r0 = [r12+r0<<2]
0x01E4327A  app+0x04317A  e061          [r6+4] = r0
0x01E4327C  app+0x04317C  4720          r7 = 0
0x01E4327E  app+0x04317E  e765          [r6+20] = r7
0x01E43280  app+0x043180  e764          [r6+16] = r7
0x01E43282  app+0x043182  00ef886f      [r6+0] |= 0x110
0x01E43286  app+0x043186  5016          r0 = r5
0x01E43288  app+0x043188  bfeab7fe      call -658
0x01E4328C  app+0x04318C  4832          r0 = 50
0x01E4328E  app+0x04318E  8dea78f6      call 1830128
0x01E43292  app+0x043192  2be1c46a      r11 = r6 + -5436
0x01E43296  app+0x043196  02ef80be      [r11+8] |= 0x400
0x01E4329A  app+0x04319A  4017          r0 = r4.b0 (u)
0x01E4329C  app+0x04319C  80f80d02      if (r0 != 1) goto 26
0x01E432A0  app+0x0431A0  d0ecb090      r9 = [r11+0]
0x01E432A4  app+0x0431A4  d0ecb8a0      r10 = [r11+8]
0x01E432A8  app+0x0431A8  d0ecbc82      r8 = [r11+44]
0x01E432AC  app+0x0431AC  c0ef00b8      [r11+0] &= 0xFF7FFFFF
0x01E432B0  app+0x0431B0  02ef80bf      [r11+8] |= 0x100
0x01E432B4  app+0x0431B4  0bef00bb      [r11+44] |= 0x20000
0x01E432B8  app+0x0431B8  4721          r7 = 1
0x01E432BA  app+0x0431BA  8014          r1_r0 = 0
0x01E432BC  app+0x0431BC  4188          call -240
0x01E432BE  app+0x0431BE  0516          r5 = r0
0x01E432C0  app+0x0431C0  4021          r0 = 1
0x01E432C2  app+0x0431C2  4120          r1 = 0
0x01E432C4  app+0x0431C4  4184          call -248
0x01E432C6  app+0x0431C6  5018          r0 += r5
0x01E432C8  app+0x0431C8  8817          r0 = r0.l (s)
0x01E432CA  app+0x0431CA  4422          r4 = 2
0x01E432CC  app+0x0431CC  f4e10104      r0 = r0 / r4 (s)
0x01E432D0  app+0x0431D0  50edd100      h[r13+0] = r0
0x01E432D4  app+0x0431D4  4020          r0 = 0
0x01E432D6  app+0x0431D6  4121          r1 = 1
0x01E432D8  app+0x0431D8  bfea79ff      call -270
0x01E432DC  app+0x0431DC  0516          r5 = r0
0x01E432DE  app+0x0431DE  4021          r0 = 1
0x01E432E0  app+0x0431E0  4121          r1 = 1
0x01E432E2  app+0x0431E2  bfea74ff      call -280
0x01E432E6  app+0x0431E6  5018          r0 += r5
0x01E432E8  app+0x0431E8  8817          r0 = r0.l (s)
0x01E432EA  app+0x0431EA  f4e10104      r0 = r0 / r4 (s)
0x01E432EE  app+0x0431EE  50edd300      h[r13+2] = r0
0x01E432F2  app+0x0431F2  40e07c06      r0 = 1660
0x01E432F6  app+0x0431F6  d8ecca00      r0 = [r12+r0<<2]
0x01E432FA  app+0x0431FA  0062          r0 = [r0+8]
0x01E432FC  app+0x0431FC  50e82592      if ((r0 & 262144) != 0) goto 74
0x01E43300  app+0x043200  60e10f00      r0 = r0 & 0xF
0x01E43304  app+0x043204  f83a          r0 += -6
0x01E43306  app+0x043206  00fc5206      if (r0 > 3) goto 164
0x01E4330A  app+0x04320A  0001          tbb [r0]
0x01E4330C  app+0x04320C  0231          r2 = [sp+68]
0x01E4330E  app+0x04320E  4102          .byte 0x41, 0x02
0x01E43310  app+0x043210  4020          r0 = 0
0x01E43312  app+0x043212  4122          r1 = 2
0x01E43314  app+0x043214  bfea5bff      call -330
0x01E43318  app+0x043218  0516          r5 = r0
0x01E4331A  app+0x04321A  4021          r0 = 1
0x01E4331C  app+0x04321C  4122          r1 = 2
0x01E4331E  app+0x04321E  bfea56ff      call -340
0x01E43322  app+0x043222  5018          r0 += r5
0x01E43324  app+0x043224  8817          r0 = r0.l (s)
0x01E43326  app+0x043226  f4e10104      r0 = r0 / r4 (s)
0x01E4332A  app+0x04322A  50edd500      h[r13+4] = r0
0x01E4332E  app+0x04322E  4020          r0 = 0
0x01E43330  app+0x043230  4123          r1 = 3
0x01E43332  app+0x043232  bfea4cff      call -360
0x01E43336  app+0x043236  0516          r5 = r0
0x01E43338  app+0x043238  4021          r0 = 1
0x01E4333A  app+0x04323A  4123          r1 = 3
0x01E4333C  app+0x04323C  bfea47ff      call -370
0x01E43340  app+0x043240  5018          r0 += r5
0x01E43342  app+0x043242  8817          r0 = r0.l (s)
0x01E43344  app+0x043244  f4e10104      r0 = r0 / r4 (s)
0x01E43348  app+0x043248  1490          goto 96
0x01E4334A  app+0x04324A  4020          r0 = 0
0x01E4334C  app+0x04324C  4128          r1 = 8
0x01E4334E  app+0x04324E  bfea3eff      call -388
0x01E43352  app+0x043252  0516          r5 = r0
0x01E43354  app+0x043254  4021          r0 = 1
0x01E43356  app+0x043256  4128          r1 = 8
0x01E43358  app+0x043258  bfea39ff      call -398
0x01E4335C  app+0x04325C  5018          r0 += r5
0x01E4335E  app+0x04325E  8817          r0 = r0.l (s)
0x01E43360  app+0x043260  f4e10104      r0 = r0 / r4 (s)
0x01E43364  app+0x043264  50edd500      h[r13+4] = r0
0x01E43368  app+0x043268  50ed6103      h[r6+48] = r0
0x01E4336C  app+0x04326C  1480          goto 64
0x01E4336E  app+0x04326E  4020          r0 = 0
0x01E43370  app+0x043270  4122          r1 = 2
0x01E43372  app+0x043272  bfea2cff      call -424
0x01E43376  app+0x043276  0516          r5 = r0
0x01E43378  app+0x043278  4021          r0 = 1
0x01E4337A  app+0x04327A  4122          r1 = 2
0x01E4337C  app+0x04327C  bfea27ff      call -434
0x01E43380  app+0x043280  5018          r0 += r5
0x01E43382  app+0x043282  8817          r0 = r0.l (s)
0x01E43384  app+0x043284  f4e10104      r0 = r0 / r4 (s)
0x01E43388  app+0x043288  50edd500      h[r13+4] = r0
0x01E4338C  app+0x04328C  0490          goto 32
0x01E4338E  app+0x04328E  4020          r0 = 0
0x01E43390  app+0x043290  4123          r1 = 3
0x01E43392  app+0x043292  bfea1cff      call -456
0x01E43396  app+0x043296  0516          r5 = r0
0x01E43398  app+0x043298  4021          r0 = 1
0x01E4339A  app+0x04329A  4123          r1 = 3
0x01E4339C  app+0x04329C  bfea17ff      call -466
0x01E433A0  app+0x0432A0  5018          r0 += r5
0x01E433A2  app+0x0432A2  8817          r0 = r0.l (s)
0x01E433A4  app+0x0432A4  4122          r1 = 2
0x01E433A6  app+0x0432A6  f4e10101      r0 = r0 / r1 (s)
0x01E433AA  app+0x0432AA  50edd700      h[r13+6] = r0
0x01E433AE  app+0x0432AE  7028          r0 = 200
0x01E433B0  app+0x0432B0  80ead23b      call 30628
0x01E433B4  app+0x0432B4  0746          if (r7 == 0) goto 12
0x01E433B6  app+0x0432B6  d0ecb190      [r11+0] = r9
0x01E433BA  app+0x0432BA  d0ecb9a0      [r11+8] = r10
0x01E433BE  app+0x0432BE  d0ecbd82      [r11+44] = r8
0x01E433C2  app+0x0432C2  c2ef80be      [r11+8] &= 0xFFFFFBFF
0x01E433C6  app+0x0432C6  7028          r0 = 200
0x01E433C8  app+0x0432C8  3d04          {rets, r13-r4} = [sp++]
0x01E433CA  app+0x0432CA  c0eac53b      goto 30602
0x01E433CE  app+0x0432CE  c1ff14350100  r1 = 79124
0x01E433D4  app+0x0432D4  1083          rep 4 4 {
0x01E433D6  app+0x0432D6  0206          r2 = h[r0++=2] (u)
0x01E433D8  app+0x0432D8  9206          h[r1++=2] = r2
0x01E433DA  app+0x0432DA  8000          rts
0x01E433DC  app+0x0432DC  40e0561a      r0 = 6742
0x01E433E0  app+0x0432E0  c1ffd01a0100  r1 = 72400
0x01E433E6  app+0x0432E6  d8ee1000      r0 = b[r1+r0] (u)
0x01E433EA  app+0x0432EA  8000          rts
0x01E433EC  app+0x0432EC  7404          [--sp] = {rets, r4}
0x01E433EE  app+0x0432EE  8dea21f1      call 1827394
0x01E433F2  app+0x0432F2  c3ff382b0100  r3 = 76600
0x01E433F8  app+0x0432F8  4420          r4 = 0
0x01E433FA  app+0x0432FA  3016          r0 = r3
0x01E433FC  app+0x0432FC  0098          rep 2 25 {
0x01E433FE  app+0x0432FE  8405          [r0++=4] = r4
0x01E43400  app+0x043300  80ea7e17      call 12028
0x01E43404  app+0x043304  00e18439      r0 = r3 + 2436
0x01E43408  app+0x043308  01f15430      r1 = r3 + 84  #
0x01E4340C  app+0x04330C  b064          [r3+16] = r0
0x01E4340E  app+0x04330E  bc40          b[r3+0] = r4
0x01E43410  app+0x043310  c0ffee600000  r0 = 24814
0x01E43416  app+0x043316  9060          [r1+0] = r0
0x01E43418  app+0x043318  c0ffb239e401  r0 = 31734194
0x01E4341E  app+0x04331E  9061          [r1+4] = r0
0x01E43420  app+0x043320  c0ffb439e401  r0 = 31734196
0x01E43426  app+0x043326  9062          [r1+8] = r0
0x01E43428  app+0x043328  3404          {rets, r4} = [sp++]
0x01E4342A  app+0x04332A  cdea11f1      goto 1827362
0x01E4342E  app+0x04332E  0046          if (r0 == 0) goto 12
0x01E43430  app+0x043330  c1ff3c2b0100  r1 = 76604
0x01E43436  app+0x043336  1082          rep 4 3 {
0x01E43438  app+0x043338  1206          r2 = h[r1++=2] (u)
0x01E4343A  app+0x04333A  8206          h[r0++=2] = r2
0x01E4343C  app+0x04333C  8000          rts
0x01E4343E  app+0x04333E  0046          if (r0 == 0) goto 12
0x01E43440  app+0x043340  c1ff3c2b0100  r1 = 76604
0x01E43446  app+0x043346  1082          rep 4 3 {
0x01E43448  app+0x043348  0206          r2 = h[r0++=2] (u)
0x01E4344A  app+0x04334A  9206          h[r1++=2] = r2
0x01E4344C  app+0x04334C  8000          rts
0x01E4344E  app+0x04334E  7a04          [--sp] = {rets, r10-r4}
0x01E43450  app+0x043350  c8ff042f1e00  r8 = 1978116
0x01E43456  app+0x043356  d0ec8410      r1 = [r8+4]
0x01E4345A  app+0x04335A  d0ec8431      r3 = [r8+20]
0x01E4345E  app+0x04335E  d0ec8c20      r2 = [r8+12]
0x01E43462  app+0x043362  d0ec8040      r4 = [r8+0]
0x01E43466  app+0x043366  991e          r1 = r1 - r3
0x01E43468  app+0x043368  b9e14010      r9 = uextra(r1, p:0, l:16)
0x01E4346C  app+0x04336C  41e0fe7f      r1 = 32766
0x01E43470  app+0x043370  9118          r1 += r9
0x01E43472  app+0x043372  f4e11019      r1 = r1 / r9 (u)
0x01E43476  app+0x043376  0ae10110      r10 = r1 + 1
0x01E4347A  app+0x04337A  4520          r5 = 0
0x01E4347C  app+0x04337C  46e0ff7f      r6 = 32767
0x01E43480  app+0x043380  6116          r1 = r6
0x01E43482  app+0x043382  049f          goto 62
0x01E43484  app+0x043384  b4e0127a      r7 = r1 - r10
0x01E43488  app+0x043388  91ec000a      if (r1 <= r10) {
0x01E4348C  app+0x04338C  4720          r7 = 0
0x01E4348E  app+0x04338E  d0ec8030      r3 = [r8+0]
0x01E43492  app+0x043392  50ee0a16      r1 = b[r0+106] (u)
0x01E43496  app+0x043396  a417          r4 = r2.l (u)
0x01E43498  app+0x043398  141b          r4 *= r1
0x01E4349A  app+0x04339A  44a1          r4 = r4 << 1
0x01E4349C  app+0x04339C  4318          r3 += r4
0x01E4349E  app+0x04339E  0487          goto 14
0x01E434A0  app+0x0433A0  54ed3040      r4 = h[r3+0] (s)
0x01E434A4  app+0x0433A4  741b          r4 *= r7
0x01E434A6  app+0x0433A6  f4e14146      r4 = r4 / r6 (s)
0x01E434AA  app+0x0433AA  b406          h[r3++=2] = r4
0x01E434AC  app+0x0433AC  f93f          r1 += -1
0x01E434AE  app+0x0433AE  f158          if (r1 != 0) goto -16
0x01E434B0  app+0x0433B0  d0ec8410      r1 = [r8+4]
0x01E434B4  app+0x0433B4  c221          r2 += 1
0x01E434B6  app+0x0433B6  a317          r3 = r2.l (u)
0x01E434B8  app+0x0433B8  13e90001      if (r3 >= r1) {
0x01E434BC  app+0x0433BC  4220          r2 = 0
0x01E434BE  app+0x0433BE  c521          r5 += 1
0x01E434C0  app+0x0433C0  7116          r1 = r7
0x01E434C2  app+0x0433C2  89e8df51      if (r5 != r9) goto -66
0x01E434C6  app+0x0433C6  5a04          {pc, r10-r4} = [sp++]
0x01E434C8  app+0x0433C8  7704          [--sp] = {rets, r7-r4}
0x01E434CA  app+0x0433CA  1416          r4 = r1
0x01E434CC  app+0x0433CC  0516          r5 = r0
0x01E434CE  app+0x0433CE  4620          r6 = 0
0x01E434D0  app+0x0433D0  c0ffb012eb01  r0 = 32182960
0x01E434D6  app+0x0433D6  0485          goto 10
0x01E434D8  app+0x0433D8  d8ec0a16      r1 = [r0+r6<<2]
0x01E434DC  app+0x0433DC  04e80410      if (r1 == r4) goto 8
0x01E434E0  app+0x0433E0  c621          r6 += 1
0x01E434E2  app+0x0433E2  86fcf91d      if (r6 <= 14) goto -14
0x01E434E6  app+0x0433E6  5704          {pc, r7-r4} = [sp++]
0x01E434E8  app+0x0433E8  c7ff002f1e00  r7 = 1978112
0x01E434EE  app+0x0433EE  7064          r0 = [r7+16]
0x01E434F0  app+0x0433F0  7165          r1 = [r7+20]
0x01E434F2  app+0x0433F2  01e81800      if (r0 == r1) goto 48
0x01E434F6  app+0x0433F6  7064          r0 = [r7+16]
0x01E434F8  app+0x0433F8  7165          r1 = [r7+20]
0x01E434FA  app+0x0433FA  01e81400      if (r0 == r1) goto 40
0x01E434FE  app+0x0433FE  5016          r0 = r5
0x01E43500  app+0x043400  5186          call -180
0x01E43502  app+0x043402  8dea97f0      call 1827118
0x01E43506  app+0x043406  5a88          r2 = r5 + 8
0x01E43508  app+0x043408  2016          r0 = r2
0x01E4350A  app+0x04340A  8deab9fb      call 1832818
0x01E4350E  app+0x04340E  43ea0170      [r7+12] = 0x1
0x01E43512  app+0x043412  00ef2070      [r7+0] |= 0x20
0x01E43516  app+0x043416  00ef4070      [r7+0] |= 0x40
0x01E4351A  app+0x04341A  8dea99f0      call 1827122
0x01E4351E  app+0x04341E  4134          r1 = 20
0x01E43520  app+0x043420  2016          r0 = r2
0x01E43522  app+0x043422  8dea20fb      call 1832512
0x01E43526  app+0x043426  d0ec5947      [r5+120] = r4
0x01E4352A  app+0x04342A  c0ef0f70      [r7+0] &= 0xFFFFFFF0
0x01E4352E  app+0x04342E  64e80076      [r7+0] |= r6
0x01E43532  app+0x043432  5704          {pc, r7-r4} = [sp++]
0x01E43534  app+0x043434  7904          [--sp] = {rets, r9-r4}
0x01E43536  app+0x043436  0416          r4 = r0
0x01E43538  app+0x043438  4888          r0 = r4 + 8
0x01E4353A  app+0x04343A  8dea05fa      call 1831946
0x01E4353E  app+0x04343E  07e18c40      r7 = r4 + 140
0x01E43542  app+0x043442  06f15840      r6 = r4 + 88  #
0x01E43546  app+0x043446  7a60          r2 = h[r7+0] (u)
0x01E43548  app+0x043448  c1d4          r9 = 0  #
0x01E4354A  app+0x04344A  606c          r0 = [r6+48]
0x01E4354C  app+0x04344C  4120          r1 = 0
0x01E4354E  app+0x04344E  96ea3179      call 2945634
0x01E43552  app+0x043452  c5ff002f1e00  r5 = 1978112
0x01E43558  app+0x043458  d0ec5591      [r5+20] = r9
0x01E4355C  app+0x04345C  d0ec5191      [r5+16] = r9
0x01E43560  app+0x043460  606c          r0 = [r6+48]
0x01E43562  app+0x043462  08f16840      r8 = r4 + 104  #
0x01E43566  app+0x043466  d061          [r5+4] = r0
0x01E43568  app+0x043468  7860          r0 = h[r7+0] (u)
0x01E4356A  app+0x04346A  50ee8210      r1 = b[r8+2] (u)
0x01E4356E  app+0x04346E  80a1          r0 = r0 >> 1
0x01E43570  app+0x043470  f4e10001      r0 = r0 / r1 (u)
0x01E43574  app+0x043474  d062          [r5+8] = r0
0x01E43576  app+0x043476  00ef805f      [r5+0] |= 0x100
0x01E4357A  app+0x04347A  43ea3c50      [r5+12] = 0x3C
0x01E4357E  app+0x04347E  5061          r0 = [r5+4]
0x01E43580  app+0x043480  5062          r0 = [r5+8]
0x01E43582  app+0x043482  5064          r0 = [r5+16]
0x01E43584  app+0x043484  5065          r0 = [r5+20]
0x01E43586  app+0x043486  5066          r0 = [r5+24]
0x01E43588  app+0x043488  4023          r0 = 3
0x01E4358A  app+0x04348A  40d6          r0 = r4  #
0x01E4358C  app+0x04348C  e849          b[r6+9] = r0
0x01E4358E  app+0x04348E  bfeac9fc      call -1646
0x01E43592  app+0x043492  50ed8d91      h[r8+28] = r9
0x01E43596  app+0x043496  50ed7990      h[r7+8] = r9
0x01E4359A  app+0x04349A  52ee7390      b[r7+3] = r9
0x01E4359E  app+0x04349E  6068          r0 = [r6+32]
0x01E435A0  app+0x0434A0  004a          if (r0 == 0) goto 20
0x01E435A2  app+0x0434A2  41e09001      r1 = 400
0x01E435A6  app+0x0434A6  f4e10001      r0 = r0 / r1 (u)
0x01E435AA  app+0x0434AA  c1fffcff0000  r1 = 65532
0x01E435B0  app+0x0434B0  9019          r0 &= r1
0x01E435B2  app+0x0434B2  50ed8f01      h[r8+30] = r0
0x01E435B6  app+0x0434B6  00ef885f      [r5+0] |= 0x110
0x01E435BA  app+0x0434BA  6861          r0 = h[r6+2] (u)
0x01E435BC  app+0x0434BC  6960          r1 = h[r6+0] (u)
0x01E435BE  app+0x0434BE  a1e14008      r1 <= insert(r0, p:16, l:16)
0x01E435C2  app+0x0434C2  d16a          [r5+40] = r1
0x01E435C4  app+0x0434C4  4061          r0 = [r4+4]
0x01E435C6  app+0x0434C6  6963          r1 = h[r6+6] (u)
0x01E435C8  app+0x0434C8  0a4a          r2 = b[r0+10] (u)
0x01E435CA  app+0x0434CA  10b0          r0 = r1 << 16
0x01E435CC  app+0x0434CC  22ea04e0      if ((r2 & 0x4) == 0) {
0x01E435D0  app+0x0434D0  6962          r1 = h[r6+4] (u)
0x01E435D2  app+0x0434D2  1019          r0 |= r1
0x01E435D4  app+0x0434D4  d06b          [r5+44] = r0
0x01E435D6  app+0x0434D6  4020          r0 = 0
0x01E435D8  app+0x0434D8  d06b          [r5+44] = r0
0x01E435DA  app+0x0434DA  6862          r0 = h[r6+4] (u)
0x01E435DC  app+0x0434DC  50ed5103      h[r5+48] = r0
0x01E435E0  app+0x0434E0  4016          r0 = r4
0x01E435E2  app+0x0434E2  3904          {rets, r9-r4} = [sp++]
0x01E435E4  app+0x0434E4  868a          goto -1516
0x01E435E6  app+0x0434E6  7a04          [--sp] = {rets, r10-r4}
0x01E435E8  app+0x0434E8  0416          r4 = r0
0x01E435EA  app+0x0434EA  d0ec4081      r8 = [r4+16]
0x01E435EE  app+0x0434EE  07f16080      r7 = r8 + 96  #
0x01E435F2  app+0x0434F2  4b44          r3 = b[r4+4] (u)
0x01E435F4  app+0x0434F4  1ad6          r10 = r1  #
0x01E435F6  app+0x0434F6  7566          r5 = [r7+24]
0x01E435F8  app+0x0434F8  4994          r1 = r4 + 20
0x01E435FA  app+0x0434FA  00f13870      r0 = r7 + 56  #
0x01E435FE  app+0x0434FE  4e63          r6 = h[r4+6] (u)
0x01E43600  app+0x043500  6216          r2 = r6
0x01E43602  app+0x043502  bfeaacf9      call -3240
0x01E43606  app+0x043506  f0e16005      r0 = r6 * r5
0x01E4360A  app+0x04350A  41e0e803      r1 = 1000
0x01E4360E  app+0x04350E  c6ff002f1e00  r6 = 1978112
0x01E43614  app+0x043514  f4f10001      r0 = r0 / r1 (u)  #
0x01E43618  app+0x043518  6262          r2 = [r6+8]
0x01E4361A  app+0x04351A  c9fffeff7f00  r9 = 8388606
0x01E43620  app+0x043520  90e10209      r0 = r0 & r9
0x01E43624  app+0x043524  10e90042      if (r0 >= r2) {
0x01E43628  app+0x043528  6062          r0 = [r6+8]
0x01E4362A  app+0x04352A  f83f          r0 += -1
0x01E4362C  app+0x04352C  794a          r1 = b[r7+10] (u)
0x01E4362E  app+0x04352E  10db          r0 *= r1  #
0x01E43630  app+0x043530  4b44          r3 = b[r4+4] (u)
0x01E43632  app+0x043532  00a1          r0 = r0 << 1
0x01E43634  app+0x043534  b4e0020a      r0 = r0 - r10
0x01E43638  app+0x043538  4220          r2 = 0
0x01E4363A  app+0x04353A  b3e80100      if (r3 != 1) {
0x01E4363E  app+0x04353E  0216          r2 = r0
0x01E43640  app+0x043540  4120          r1 = 0
0x01E43642  app+0x043542  4321          r3 = 1
0x01E43644  app+0x043544  4521          r5 = 1
0x01E43646  app+0x043546  4016          r0 = r4
0x01E43648  app+0x043548  8eeaf413      call 1845224
0x01E4364C  app+0x04354C  cd40          b[r4+0] = r5
0x01E4364E  app+0x04354E  4864          r0 = h[r4+8] (u)
0x01E43650  app+0x043550  0050          if (r0 == 0) goto 32
0x01E43652  app+0x043552  7266          r2 = [r7+24]
0x01E43654  app+0x043554  50ed8419      r1 = h[r8+148] (u)
0x01E43658  app+0x043558  201b          r0 *= r2
0x01E4365A  app+0x04355A  42e0e803      r2 = 1000
0x01E4365E  app+0x04355E  f4e10002      r0 = r0 / r2 (u)
0x01E43662  app+0x043562  90e10209      r0 = r0 & r9
0x01E43666  app+0x043566  0142          if (r1 == 0) goto 4
0x01E43668  app+0x043568  80ec0310      if (r1 <= r0) goto 6
0x01E4366C  app+0x04356C  e063          [r6+12] = r0
0x01E4366E  app+0x04356E  50ed7503      h[r7+52] = r0
0x01E43672  app+0x043572  7841          r0 = b[r7+1] (u)
0x01E43674  app+0x043574  80f80f06      if (r0 != 3) goto 30
0x01E43678  app+0x043578  4164          r1 = [r4+16]
0x01E4367A  app+0x04357A  8deadbef      call 1826742
0x01E4367E  app+0x04357E  00ef4060      [r6+0] |= 0x40
0x01E43682  app+0x043582  00ef2060      [r6+0] |= 0x20
0x01E43686  app+0x043586  4022          r0 = 2
0x01E43688  app+0x043588  52ee1106      b[r1+97] = r0
0x01E4368C  app+0x04358C  8deae0ef      call 1826752
0x01E43690  app+0x043590  4022          r0 = 2
0x01E43692  app+0x043592  bfea5fed      call -9538
0x01E43696  app+0x043596  5a04          {pc, r10-r4} = [sp++]
0x01E43698  app+0x043598  7804          [--sp] = {rets, r8-r4}
0x01E4369A  app+0x04359A  e29f          sp += -4
0x01E4369C  app+0x04359C  41e01e04      r1 = 1054
0x01E436A0  app+0x0435A0  c6ffd01a0100  r6 = 72400
0x01E436A6  app+0x0435A6  d8ec6a41      r4 = [r6+r1<<2]
0x01E436AA  app+0x0435AA  07e16040      r7 = r4 + 96
0x01E436AE  app+0x0435AE  08d6          r8 = r0  #
0x01E436B0  app+0x0435B0  7941          r1 = b[r7+1] (u)
0x01E436B2  app+0x0435B2  01f80306      if (r1 == 3) goto 6
0x01E436B6  app+0x0435B6  7841          r0 = b[r7+1] (u)
0x01E436B8  app+0x0435B8  80f82604      if (r0 != 2) goto 76
0x01E436BC  app+0x0435BC  7841          r0 = b[r7+1] (u)
0x01E436BE  app+0x0435BE  00f80a04      if (r0 == 2) goto 20
0x01E436C2  app+0x0435C2  00f13870      r0 = r7 + 56  #
0x01E436C6  app+0x0435C6  7366          r3 = [r7+24]
0x01E436C8  app+0x0435C8  50ed4c28      r2 = h[r4+140] (u)
0x01E436CC  app+0x0435CC  716a          r1 = [r7+40]
0x01E436CE  app+0x0435CE  7d4a          r5 = b[r7+10] (u)
0x01E436D0  app+0x0435D0  8520          [sp] = r5
0x01E436D2  app+0x0435D2  bfea38f9      call -3472
0x01E436D6  app+0x0435D6  40e06810      r0 = 4200
0x01E436DA  app+0x0435DA  d8ee6000      r0 = b[r6+r0] (u)
0x01E436DE  app+0x0435DE  b0e80180      if (r0 != 1) {
0x01E436E2  app+0x0435E2  10e16860      r0 = r6 + 4200
0x01E436E6  app+0x0435E6  6120          r1 = 128
0x01E436E8  app+0x0435E8  bfea7dff      call -262
0x01E436EC  app+0x0435EC  10e16860      r0 = r6 + 4200
0x01E436F0  app+0x0435F0  6220          r2 = 128
0x01E436F2  app+0x0435F2  4320          r3 = 0
0x01E436F4  app+0x0435F4  8116          r1 = r8
0x01E436F6  app+0x0435F6  8eea2314      call 1845318
0x01E436FA  app+0x0435FA  b0ee7f80      ifs (r0 <= 127) {
0x01E436FE  app+0x0435FE  50ed4618      r1 = h[r4+134] (u)
0x01E43702  app+0x043602  4016          r0 = r4
0x01E43704  app+0x043604  8eea8814      call 1845520
0x01E43708  app+0x043608  0281          sp += 4
0x01E4370A  app+0x04360A  5804          {pc, r8-r4} = [sp++]
0x01E4370C  app+0x04360C  7704          [--sp] = {rets, r7-r4}
0x01E4370E  app+0x04360E  0416          r4 = r0
0x01E43710  app+0x043610  05e16040      r5 = r4 + 96
0x01E43714  app+0x043614  5841          r0 = b[r5+1] (u)
0x01E43716  app+0x043616  00f80306      if (r0 == 3) goto 6
0x01E4371A  app+0x04361A  5841          r0 = b[r5+1] (u)
0x01E4371C  app+0x04361C  80f81904      if (r0 != 2) goto 50
0x01E43720  app+0x043620  c6ff002f1e00  r6 = 1978112
0x01E43726  app+0x043626  6068          r0 = [r6+32]
0x01E43728  app+0x043628  4020          r0 = 0
0x01E4372A  app+0x04362A  50ed4509      h[r4+148] = r0
0x01E4372E  app+0x04362E  52ee4f08      b[r4+143] = r0
0x01E43732  app+0x043632  4016          r0 = r4
0x01E43734  app+0x043634  bfea8bfe      call -746
0x01E43738  app+0x043638  4024          r0 = 4
0x01E4373A  app+0x04363A  4724          r7 = 4
0x01E4373C  app+0x04363C  bfea0aed      call -9708
0x01E43740  app+0x043640  c0ef2060      [r6+0] &= 0xFFFFFFDF
0x01E43744  app+0x043644  00ef4060      [r6+0] |= 0x40
0x01E43748  app+0x043648  4016          r0 = r4
0x01E4374A  app+0x04364A  8eea5714      call 1845422
0x01E4374E  app+0x04364E  df41          b[r5+1] = r7
0x01E43750  app+0x043650  5704          {pc, r7-r4} = [sp++]
0x01E43752  app+0x043652  5841          r0 = b[r5+1] (u)
0x01E43754  app+0x043654  5704          {pc, r7-r4} = [sp++]
0x01E43756  app+0x043656  7504          [--sp] = {rets, r5, r4}
0x01E43758  app+0x043658  40e06810      r0 = 4200
0x01E4375C  app+0x04365C  c4ffd01a0100  r4 = 72400
0x01E43762  app+0x043662  d8ee4000      r0 = b[r4+r0] (u)
0x01E43766  app+0x043666  41e01e04      r1 = 1054
0x01E4376A  app+0x04366A  d8ec4a31      r3 = [r4+r1<<2]
0x01E4376E  app+0x04366E  0053          if (r0 == 0) goto 38
0x01E43770  app+0x043670  00f81106      if (r0 == 3) goto 34
0x01E43774  app+0x043674  8dea5eef      call 1826492
0x01E43778  app+0x043678  45e06810      r5 = 4200
0x01E4377C  app+0x04367C  d8ee4005      r0 = b[r4+r5] (u)
0x01E43780  app+0x043680  30e80140      if (r0 == 1) {
0x01E43784  app+0x043684  10e17c40      r0 = r4 + 4220
0x01E43788  app+0x043688  bfeabaf9      call -3212
0x01E4378C  app+0x04368C  4023          r0 = 3
0x01E4378E  app+0x04368E  d8ee4105      b[r4+r5] = r0
0x01E43792  app+0x043692  8dea5def      call 1826490
0x01E43796  app+0x043696  00e19830      r0 = r3 + 152
0x01E4379A  app+0x04369A  bfeab6f9      call -3220
0x01E4379E  app+0x04369E  8042          if (r0 != 0) goto 4
0x01E437A0  app+0x0436A0  3016          r0 = r3
0x01E437A2  app+0x0436A2  5194          call -152
0x01E437A4  app+0x0436A4  5504          {pc, r5, r4} = [sp++]
0x01E437A6  app+0x0436A6  7704          [--sp] = {rets, r7-r4}
0x01E437A8  app+0x0436A8  41e01e04      r1 = 1054
0x01E437AC  app+0x0436AC  c6ffd01a0100  r6 = 72400
0x01E437B2  app+0x0436B2  d8ec6a11      r1 = [r6+r1<<2]
0x01E437B6  app+0x0436B6  0516          r5 = r0
0x01E437B8  app+0x0436B8  04e1c010      r4 = r1 + 192
0x01E437BC  app+0x0436BC  4120          r1 = 0
0x01E437BE  app+0x0436BE  4720          r7 = 0
0x01E437C0  app+0x0436C0  4016          r0 = r4
0x01E437C2  app+0x0436C2  8deafff9      call 1831934
0x01E437C6  app+0x0436C6  40e01d04      r0 = 1053
0x01E437CA  app+0x0436CA  d8ec6a10      r1 = [r6+r0<<2]
0x01E437CE  app+0x0436CE  11e80005      if (r1 == r5) {
0x01E437D2  app+0x0436D2  d8ec6b70      [r6+r0<<2] = r7
0x01E437D6  app+0x0436D6  4016          r0 = r4
0x01E437D8  app+0x0436D8  3704          {rets, r7-r4} = [sp++]
0x01E437DA  app+0x0436DA  cdea2ffa      goto 1832030
0x01E437DE  app+0x0436DE  7404          [--sp] = {rets, r4}
0x01E437E0  app+0x0436E0  4320          r3 = 0
0x01E437E2  app+0x0436E2  c2ff00770100  r2 = 96000
0x01E437E8  app+0x0436E8  c4ff8012eb01  r4 = 32182912
0x01E437EE  app+0x0436EE  0487          goto 14
0x01E437F0  app+0x0436F0  d8ec4a13      r1 = [r4+r3<<2]
0x01E437F4  app+0x0436F4  00e80710      if (r1 == r0) goto 14
0x01E437F8  app+0x0436F8  00ec0610      if (r1 > r0) goto 12
0x01E437FC  app+0x0436FC  c321          r3 += 1
0x01E437FE  app+0x0436FE  83fef717      ifs (r3 <= 11) goto -18
0x01E43802  app+0x043702  2116          r1 = r2
0x01E43804  app+0x043704  0481          goto 2
0x01E43806  app+0x043706  0116          r1 = r0
0x01E43808  app+0x043708  1016          r0 = r1
0x01E4380A  app+0x04370A  5404          {pc, r4} = [sp++]
0x01E4380C  app+0x04370C  7604          [--sp] = {rets, r6-r4}
0x01E4380E  app+0x04370E  e29e          sp += -8
0x01E43810  app+0x043710  0416          r4 = r0
0x01E43812  app+0x043712  045c          if (r4 == 0) goto 56
0x01E43814  app+0x043714  c6ff382b0100  r6 = 76600
0x01E4381A  app+0x04371A  6064          r0 = [r6+16]
0x01E4381C  app+0x04371C  05e1c000      r5 = r0 + 192
0x01E43820  app+0x043720  4120          r1 = 0
0x01E43822  app+0x043722  5016          r0 = r5
0x01E43824  app+0x043724  8deacef9      call 1831836
0x01E43828  app+0x043728  e463          [r6+12] = r4
0x01E4382A  app+0x04372A  4021          r0 = 1
0x01E4382C  app+0x04372C  e842          b[r6+2] = r0
0x01E4382E  app+0x04372E  c0ffe239e401  r0 = 31734242
0x01E43834  app+0x043734  8020          [sp] = r0
0x01E43836  app+0x043736  c0ff563ae401  r0 = 31734358
0x01E4383C  app+0x04373C  40d6          r0 = r4  #
0x01E4383E  app+0x04373E  8021          [sp+4] = r0
0x01E43840  app+0x043740  8980          r1 = sp + 0
0x01E43842  app+0x043742  80ea0d02      call 1050
0x01E43846  app+0x043746  5016          r0 = r5
0x01E43848  app+0x043748  8deaf8f9      call 1831920
0x01E4384C  app+0x04374C  0282          sp += 8
0x01E4384E  app+0x04374E  5604          {pc, r6-r4} = [sp++]
0x01E43850  app+0x043750  1004          [--sp] = rets
0x01E43852  app+0x043752  bfeafcfd      call -1032
0x01E43856  app+0x043756  4121          r1 = 1
0x01E43858  app+0x043758  52ee0f18      b[r0+143] = r1
0x01E4385C  app+0x04375C  4120          r1 = 0
0x01E4385E  app+0x04375E  50ed0719      h[r0+150] = r1
0x01E43862  app+0x043762  0004          pc = [sp++]
0x01E43864  app+0x043764  e904          [--sp] = {psr, rets, reti}
0x01E43866  app+0x043766  d8e81f00      [--sp] = {r4-r0}
0x01E4386A  app+0x04376A  c4ff002f1e00  r4 = 1978112
0x01E43870  app+0x043770  4060          r0 = [r4+0]
0x01E43872  app+0x043772  50e80c38      if ((r0 & 128) == 0) goto 24
0x01E43876  app+0x043776  00ef4040      [r4+0] |= 0x40
0x01E4387A  app+0x04377A  4060          r0 = [r4+0]
0x01E4387C  app+0x04377C  50e80728      if ((r0 & 32) == 0) goto 14
0x01E43880  app+0x043780  c0ff0cd90000  r0 = 55564
0x01E43886  app+0x043786  0060          r0 = [r0+0]
0x01E43888  app+0x043788  0042          if (r0 == 0) goto 4
0x01E4388A  app+0x04378A  8eea0f14      call 1845278
0x01E4388E  app+0x04378E  00e14040      r0 = r4 + 64
0x01E43892  app+0x043792  0160          r1 = [r0+0]
0x01E43894  app+0x043794  51e80a38      if ((r1 & 128) == 0) goto 20
0x01E43898  app+0x043798  10ef4040      [r4+64] |= 0x40
0x01E4389C  app+0x04379C  0160          r1 = [r0+0]
0x01E4389E  app+0x04379E  51e80528      if ((r1 & 32) == 0) goto 10
0x01E438A2  app+0x0437A2  0060          r0 = [r0+0]
0x01E438A4  app+0x0437A4  30ea1000      if ((r0 & 0x10) != 0) {
0x01E438A8  app+0x0437A8  80ea1a0e      call 7220
0x01E438AC  app+0x0437AC  d4e81f00      {r4-r0} = [sp++]
0x01E438B0  app+0x0437B0  a904          {psr, rets, reti} = [sp++]
0x01E438B2  app+0x0437B2  8100          rti
0x01E438B4  app+0x0437B4  7504          [--sp] = {rets, r5, r4}
0x01E438B6  app+0x0437B6  e29c          sp += -16
0x01E438B8  app+0x0437B8  0364          r3 = [r0+16]
0x01E438BA  app+0x0437BA  4420          r4 = 0
0x01E438BC  app+0x0437BC  4520          r5 = 0
0x01E438BE  app+0x0437BE  d0e90940      d[sp+8] = r5_r4
0x01E438C2  app+0x0437C2  d0e90140      d[sp] = r5_r4
0x01E438C6  app+0x0437C6  d0ec3857      r5 = [r3+120]
0x01E438CA  app+0x0437CA  24d6          r4 = r2  #
0x01E438CC  app+0x0437CC  8521          [sp+4] = r5
0x01E438CE  app+0x0437CE  8123          [sp+12] = r1
0x01E438D0  app+0x0437D0  d8e90b40      h[sp+10] = r4
0x01E438D4  app+0x0437D4  50ee3a16      r1 = b[r3+106] (u)
0x01E438D8  app+0x0437D8  dee90010      b[sp] = r1
0x01E438DC  app+0x0437DC  c834          r0 += 52
0x01E438DE  app+0x0437DE  8980          r1 = sp + 0
0x01E438E0  app+0x0437E0  8eea160b      call 1840684
0x01E438E4  app+0x0437E4  4016          r0 = r4
0x01E438E6  app+0x0437E6  0284          sp += 16
0x01E438E8  app+0x0437E8  5504          {pc, r5, r4} = [sp++]
0x01E438EA  app+0x0437EA  7d04          [--sp] = {rets, r13-r4}
0x01E438EC  app+0x0437EC  d0ec0091      r9 = [r0+16]
0x01E438F0  app+0x0437F0  03e16890      r3 = r9 + 104
0x01E438F4  app+0x0437F4  50ee32b0      r11 = b[r3+2] (u)
0x01E438F8  app+0x0437F8  a8a1          r0 = r2 >>> 1
0x01E438FA  app+0x0437FA  f4f1010b      r0 = r0 / r11 (s)  #
0x01E438FE  app+0x0437FE  3464          r4 = [r3+16]
0x01E43900  app+0x043800  bce14000      r12 = uextra(r0, p:0, l:16)
0x01E43904  app+0x043804  5824          r0 = 100
0x01E43906  app+0x043806  f4e14000      r0 = r4 / r0 (u)
0x01E4390A  app+0x04380A  46e00040      r6 = 16384
0x01E4390E  app+0x04380E  f4e16000      r0 = r6 / r0 (u)
0x01E43912  app+0x043812  0ae10100      r10 = r0 + 1
0x01E43916  app+0x043816  c014          r8 = 0
0x01E43918  app+0x043818  c514          r13 = 0
0x01E4391A  app+0x04381A  049f          goto 62
0x01E4391C  app+0x04381C  b416          r4 = r11
0x01E4391E  app+0x04381E  1716          r7 = r1
0x01E43920  app+0x043820  0489          goto 18
0x01E43922  app+0x043822  54ed7050      r5 = h[r7+0] (s)
0x01E43926  app+0x043826  54ed3e02      r0 = h[r3+46] (s)
0x01E4392A  app+0x04382A  501b          r0 *= r5
0x01E4392C  app+0x04382C  f4e10106      r0 = r0 / r6 (s)
0x01E43930  app+0x043830  f006          h[r7++=2] = r0
0x01E43932  app+0x043832  fc3f          r4 += -1
0x01E43934  app+0x043834  f456          if (r4 != 0) goto -20
0x01E43936  app+0x043836  54ed3e42      r4 = h[r3+46] (s)
0x01E4393A  app+0x04383A  2aff804c0b00  ifs (r4 >= 16384) goto 22
0x01E43940  app+0x043840  c017          r0 = r4.l (u)
0x01E43942  app+0x043842  a018          r0 += r10
0x01E43944  app+0x043844  04e18c90      r4 = r9 + 140
0x01E43948  app+0x043848  8dd7          r5 = r0.l (s)  #
0x01E4394A  app+0x04384A  c865          h[r4+10] = r0
0x01E4394C  app+0x04384C  25ed804c      ifs (r5 >= 16384) {
0x01E43950  app+0x043850  ce65          h[r4+10] = r6
0x01E43952  app+0x043852  52ee4380      b[r4+3] = r8
0x01E43956  app+0x043856  0de101d0      r13 = r13 + 1
0x01E4395A  app+0x04385A  8ce8dfd1      if (r13 != r12) goto -66
0x01E4395E  app+0x04385E  2016          r0 = r2
0x01E43960  app+0x043860  5d04          {pc, r13-r4} = [sp++]
0x01E43962  app+0x043862  7404          [--sp] = {rets, r4}
0x01E43964  app+0x043864  0416          r4 = r0
0x01E43966  app+0x043866  bfeaaffd      call -1186
0x01E4396A  app+0x04386A  c0ff002f1e00  r0 = 1978112
0x01E43970  app+0x043870  c0ef2000      [r0+0] &= 0xFFFFFFDF
0x01E43974  app+0x043874  00ef4000      [r0+0] |= 0x40
0x01E43978  app+0x043878  4016          r0 = r4
0x01E4397A  app+0x04387A  8eea3f13      call 1844862
0x01E4397E  app+0x04387E  4016          r0 = r4
0x01E43980  app+0x043880  3404          {rets, r4} = [sp++]
0x01E43982  app+0x043882  e698          goto -1104
0x01E43984  app+0x043884  7604          [--sp] = {rets, r6-r4}
0x01E43986  app+0x043886  03e1b800      r3 = r0 + 184
0x01E4398A  app+0x04388A  4420          r4 = 0
0x01E4398C  app+0x04388C  3516          r5 = r3
0x01E4398E  app+0x04388E  048b          goto 22
0x01E43990  app+0x043890  8dea50ee      call 1825952
0x01E43994  app+0x043894  6840          r0 = b[r6+0] (u)
0x01E43996  app+0x043896  80f80402      if (r0 != 1) goto 8
0x01E4399A  app+0x04389A  6894          r0 = r6 + 20
0x01E4399C  app+0x04389C  bfeab0f8      call -3744
0x01E439A0  app+0x0438A0  ec40          b[r6+0] = r4
0x01E439A2  app+0x0438A2  8dea55ee      call 1825962
0x01E439A6  app+0x0438A6  5560          r5 = [r5+0]
0x01E439A8  app+0x0438A8  36e1d45f      r6 = r5 + -44
0x01E439AC  app+0x0438AC  83e8f051      if (r5 != r3) goto -32
0x01E439B0  app+0x0438B0  5604          {pc, r6-r4} = [sp++]
0x01E439B2  app+0x0438B2  8000          rts
0x01E439B4  app+0x0438B4  1004          [--sp] = rets
0x01E439B6  app+0x0438B6  0316          r3 = r0
0x01E439B8  app+0x0438B8  8dea3cee      call 1825912
0x01E439BC  app+0x0438BC  59ee3c0c      r0 = b[++r3=-52] (u)
0x01E439C0  app+0x0438C0  80f80502      if (r0 != 1) goto 10
0x01E439C4  app+0x0438C4  3894          r0 = r3 + 20
0x01E439C6  app+0x0438C6  bfea9bf8      call -3786
0x01E439CA  app+0x0438CA  4022          r0 = 2
0x01E439CC  app+0x0438CC  b840          b[r3+0] = r0
0x01E439CE  app+0x0438CE  3063          r0 = [r3+12]
0x01E439D0  app+0x0438D0  0045          if (r0 == 0) goto 10
0x01E439D2  app+0x0438D2  3842          r0 = b[r3+2] (u)
0x01E439D4  app+0x0438D4  30e80240      if (r0 == 2) {
0x01E439D8  app+0x0438D8  4021          r0 = 1
0x01E439DA  app+0x0438DA  b842          b[r3+2] = r0
0x01E439DC  app+0x0438DC  8804          {rets} = [sp++]
0x01E439DE  app+0x0438DE  cdea37ee      goto 1825902
0x01E439E2  app+0x0438E2  7604          [--sp] = {rets, r6-r4}
0x01E439E4  app+0x0438E4  0116          r1 = r0
0x01E439E6  app+0x0438E6  1840          r0 = b[r1+0] (u)
0x01E439E8  app+0x0438E8  4220          r2 = 0
0x01E439EA  app+0x0438EA  80f83202      if (r0 != 1) goto 100
0x01E439EE  app+0x0438EE  1064          r0 = [r1+16]
0x01E439F0  app+0x0438F0  c3ffd8d90000  r3 = 55768
0x01E439F6  app+0x0438F6  3460          r4 = [r3+0]
0x01E439F8  app+0x0438F8  d0ec0807      r0 = [r0+120]
0x01E439FC  app+0x0438FC  c3ff082f1e00  r3 = 1978120
0x01E43A02  app+0x043902  3564          r5 = [r3+16]
0x01E43A04  app+0x043904  3660          r6 = [r3+0]
0x01E43A06  app+0x043906  fe3f          r6 += -1
0x01E43A08  app+0x043908  06e82350      if (r5 == r6) goto 70
0x01E43A0C  app+0x04390C  c2ff40420f00  r2 = 1000000
0x01E43A12  app+0x043912  f4e14142      r4 = r4 / r2 (s)
0x01E43A16  app+0x043916  f4e12000      r0 = r2 / r0 (u)
0x01E43A1A  app+0x04391A  c021          r0 += 1
0x01E43A1C  app+0x04391C  f0e14020      r2 = r4 * r0
0x01E43A20  app+0x043920  8dea08ee      call 1825808
0x01E43A24  app+0x043924  3064          r0 = [r3+16]
0x01E43A26  app+0x043926  3464          r4 = [r3+16]
0x01E43A28  app+0x043928  84e80300      if (r0 != r4) goto 6
0x01E43A2C  app+0x04392C  fa3f          r2 += -1
0x01E43A2E  app+0x04392E  02fefa01      ifs (r2 > 0) goto -12
0x01E43A32  app+0x043932  3260          r2 = [r3+0]
0x01E43A34  app+0x043934  3364          r3 = [r3+16]
0x01E43A36  app+0x043936  1468          r4 = [r1+32]
0x01E43A38  app+0x043938  186e          r0 = h[r1+28] (u)
0x01E43A3A  app+0x04393A  4969          r1 = h[r4+18] (u)
0x01E43A3C  app+0x04393C  bfea93f8      call -3802
0x01E43A40  app+0x043940  c1ffffffff00  r1 = 16777215
0x01E43A46  app+0x043946  2118          r1 += r2
0x01E43A48  app+0x043948  991e          r1 = r1 - r3
0x01E43A4A  app+0x04394A  101e          r0 = r1 - r0
0x01E43A4C  app+0x04394C  02a8          r2 = r0 << 8
0x01E43A4E  app+0x04394E  8deaffed      call 1825790
0x01E43A52  app+0x043952  2016          r0 = r2
0x01E43A54  app+0x043954  5604          {pc, r6-r4} = [sp++]
0x01E43A56  app+0x043956  1004          [--sp] = rets
0x01E43A58  app+0x043958  0116          r1 = r0
0x01E43A5A  app+0x04395A  8deaebed      call 1825750
0x01E43A5E  app+0x04395E  1a40          r2 = b[r1+0] (u)
0x01E43A60  app+0x043960  b2e80100      if (r2 != 1) {
0x01E43A64  app+0x043964  4220          r2 = 0
0x01E43A66  app+0x043966  8deaf3ed      call 1825766
0x01E43A6A  app+0x04396A  2016          r0 = r2
0x01E43A6C  app+0x04396C  0004          pc = [sp++]
0x01E43A6E  app+0x04396E  0056          if (r0 == 0) goto 44
0x01E43A70  app+0x043970  c0ffc4191e00  r0 = 1972676
0x01E43A76  app+0x043976  0260          r2 = [r0+0]
0x01E43A78  app+0x043978  31ea0140      if ((r1 & 0x1) != 0) {
0x01E43A7C  app+0x04397C  60e11f20      r0 = r2 & 0x1F
0x01E43A80  app+0x043980  8000          rts
0x01E43A82  app+0x043982  31ea0240      if ((r1 & 0x2) != 0) {
0x01E43A86  app+0x043986  b0e19422      r0 = uextra(r2, p:5, l:5)
0x01E43A8A  app+0x04398A  8000          rts
0x01E43A8C  app+0x04398C  51e80812      if ((r1 & 4) != 0) goto 16
0x01E43A90  app+0x043990  4020          r0 = 0
0x01E43A92  app+0x043992  31ea0800      if ((r1 & 0x8) != 0) {
0x01E43A96  app+0x043996  b0e19427      r0 = uextra(r2, p:15, l:5)
0x01E43A9A  app+0x04399A  8000          rts
0x01E43A9C  app+0x04399C  103f          r0 = -1
0x01E43A9E  app+0x04399E  8000          rts
0x01E43AA0  app+0x0439A0  b0e11425      r0 = uextra(r2, p:10, l:5)
0x01E43AA4  app+0x0439A4  8000          rts
0x01E43AA6  app+0x0439A6  0001          tbb [r0]
0x01E43AA8  app+0x0439A8  0203          rep 2 r2 {
0x01E43AAA  app+0x0439AA  0405          r4 = [r0++=4]
0x01E43AAC  app+0x0439AC  0607          r6 = b[r0++=1] (u)
0x01E43AAE  app+0x0439AE  0102          pfetch [r1]
0x01E43AB0  app+0x0439B0  0304          .byte 0x03, 0x04
0x01E43AB2  app+0x0439B2  0506          r5 = h[r0++=2] (u)
0x01E43AB4  app+0x0439B4  0607          r6 = b[r0++=1] (u)
0x01E43AB6  app+0x0439B6  c1ffc8191e00  r1 = 1972680
0x01E43ABC  app+0x0439BC  70f1fe00      r0 = r0 & 0xFFFFFF01  #
0x01E43AC0  app+0x0439C0  1260          r2 = [r1+0]
0x01E43AC2  app+0x0439C2  ba2f          r2 &= 4294934527
0x01E43AC4  app+0x0439C4  a2e18407      r2 <= insert(r0, p:15, l:1)
0x01E43AC8  app+0x0439C8  9260          [r1+0] = r2
0x01E43ACA  app+0x0439CA  8000          rts
0x01E43ACC  app+0x0439CC  c0ff48501e00  r0 = 1986632
0x01E43AD2  app+0x0439D2  00ef8000      [r0+0] |= 0x80
0x01E43AD6  app+0x0439D6  c1ef8000      [r0+4] &= 0xFFFFFF7F
0x01E43ADA  app+0x0439DA  c1ffc8191e00  r1 = 1972680
0x01E43AE0  app+0x0439E0  00ef2010      [r1+0] |= 0x20
0x01E43AE4  app+0x0439E4  00ef800f      [r0+0] |= 0x100
0x01E43AE8  app+0x0439E8  c1ef800f      [r0+4] &= 0xFFFFFEFF
0x01E43AEC  app+0x0439EC  00ef8010      [r1+0] |= 0x80
0x01E43AF0  app+0x0439F0  00ef001d      [r1+0] |= 0x2000
0x01E43AF4  app+0x0439F4  8000          rts
0x01E43AF6  app+0x0439F6  c0ffc8191e00  r0 = 1972680
0x01E43AFC  app+0x0439FC  00ef800c      [r0+0] |= 0x4000
