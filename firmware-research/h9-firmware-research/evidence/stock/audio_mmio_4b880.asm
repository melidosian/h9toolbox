; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 88; undecoded chunks: 0
0x01E4B980  app+0x04B880  ebf8c6ff      if (r11 != -129) goto -116
0x01E4B984  app+0x04B884  d01a          r0 >>= r5
0x01E4B986  app+0x04B886  0100          idle
0x01E4B988  app+0x04B888  d3ec6402      r0 = [r6+804]
0x01E4B98C  app+0x04B88C  c061          [r4+4] = r0
0x01E4B98E  app+0x04B88E  402a          r0 = 10
0x01E4B990  app+0x04B890  bfeae2f8      call -3644
0x01E4B994  app+0x04B894  4521          r5 = 1
0x01E4B996  app+0x04B896  c565          [r4+20] = r5
0x01E4B998  app+0x04B898  402a          r0 = 10
0x01E4B99A  app+0x04B89A  bfeaddf8      call -3654
0x01E4B99E  app+0x04B89E  05ef004d      [r4+20] |= 0x2000
0x01E4B9A2  app+0x04B8A2  402a          r0 = 10
0x01E4B9A4  app+0x04B8A4  bfead8f8      call -3664
0x01E4B9A8  app+0x04B8A8  02ef0140      [r4+8] |= 0x1
0x01E4B9AC  app+0x04B8AC  00ef1040      [r4+0] |= 0x10
0x01E4B9B0  app+0x04B8B0  402a          r0 = 10
0x01E4B9B2  app+0x04B8B2  bfead1f8      call -3678
0x01E4B9B6  app+0x04B8B6  00ef0140      [r4+0] |= 0x1
0x01E4B9BA  app+0x04B8BA  402a          r0 = 10
0x01E4B9BC  app+0x04B8BC  bfeaccf8      call -3688
0x01E4B9C0  app+0x04B8C0  02ef1040      [r4+8] |= 0x10
0x01E4B9C4  app+0x04B8C4  402a          r0 = 10
0x01E4B9C6  app+0x04B8C6  bfeac7f8      call -3698
0x01E4B9CA  app+0x04B8CA  8dea05c2      call 1803274
0x01E4B9CE  app+0x04B8CE  c0ff002f1e00  r0 = 1978112
0x01E4B9D4  app+0x04B8D4  0160          r1 = [r0+0]
0x01E4B9D6  app+0x04B8D6  21ea8047      if ((r1 & 0x1000000) == 0) {
0x01E4B9DA  app+0x04B8DA  00ef8007      [r0+0] |= 0x1000000
0x01E4B9DE  app+0x04B8DE  4520          r5 = 0
0x01E4B9E0  app+0x04B8E0  52ee6457      b[r6+116] = r5
0x01E4B9E4  app+0x04B8E4  4120          r1 = 0
0x01E4B9E6  app+0x04B8E6  c0ff6dc00000  r0 = 49261
0x01E4B9EC  app+0x04B8EC  8940          b[r0+0] = r1
0x01E4B9EE  app+0x04B8EE  4060          r0 = [r4+0]
0x01E4B9F0  app+0x04B8F0  d3ec6022      r2 = [r6+800]
0x01E4B9F4  app+0x04B8F4  024e          if (r2 == 0) goto 28
0x01E4B9F6  app+0x04B8F6  c0ef1040      [r4+0] &= 0xFFFFFFEF
0x01E4B9FA  app+0x04B8FA  d0ec2b18      [++r2=136] = r1
0x01E4B9FE  app+0x04B8FE  c1ff0000e0ff  r1 = 4292870144
0x01E4BA04  app+0x04B904  a163          [r2+12] = r1
0x01E4BA06  app+0x04B906  44ea0029      [r2+16] = 0x200000
0x01E4BA0A  app+0x04B90A  30ea1000      if ((r0 & 0x10) != 0) {
0x01E4BA0E  app+0x04B90E  00ef1040      [r4+0] |= 0x10
0x01E4BA12  app+0x04B912  0284          sp += 16
0x01E4BA14  app+0x04B914  5f04          {pc, r15-r4} = [sp++]
0x01E4BA16  app+0x04B916  7404          [--sp] = {rets, r4}
0x01E4BA18  app+0x04B918  0416          r4 = r0
0x01E4BA1A  app+0x04B91A  045a          if (r4 == 0) goto 52
0x01E4BA1C  app+0x04B91C  01fc0804      if (r1 > 2) goto 16
0x01E4BA20  app+0x04B920  d0ec4c05      r0 = [r4+92]
0x01E4BA24  app+0x04B924  bfea8dc9      call -27878
0x01E4BA28  app+0x04B928  00e16040      r0 = r4 + 96
0x01E4BA2C  app+0x04B92C  bfea97c9      call -27858
0x01E4BA30  app+0x04B930  40e02902      r0 = 553
0x01E4BA34  app+0x04B934  d8ec4a00      r0 = [r4+r0<<2]
0x01E4BA38  app+0x04B938  0042          if (r0 == 0) goto 4
0x01E4BA3A  app+0x04B93A  8deabcd5      call 1813368
0x01E4BA3E  app+0x04B93E  40e01c02      r0 = 540
0x01E4BA42  app+0x04B942  d8ec4a00      r0 = [r4+r0<<2]
0x01E4BA46  app+0x04B946  bfea0e2b      call -109028
0x01E4BA4A  app+0x04B94A  4016          r0 = r4
0x01E4BA4C  app+0x04B94C  bfea0b2b      call -109034
0x01E4BA50  app+0x04B950  5404          {pc, r4} = [sp++]
0x01E4BA52  app+0x04B952  7604          [--sp] = {rets, r6-r4}
0x01E4BA54  app+0x04B954  c4ffd01a0100  r4 = 72400
0x01E4BA5A  app+0x04B95A  d2ec400f      r0 = [r4+752]
0x01E4BA5E  app+0x04B95E  104d          if (r0 == 0) goto 90
0x01E4BA60  app+0x04B960  01e19001      r1 = r0 + 400
0x01E4BA64  app+0x04B964  1a60          r2 = h[r1+0] (u)
0x01E4BA66  app+0x04B966  52e82800      if ((r2 & 1) == 0) goto 80
0x01E4BA6A  app+0x04B96A  ba20          r2 &= 4294967294
0x01E4BA6C  app+0x04B96C  9a60          h[r1+0] = r2
0x01E4BA6E  app+0x04B96E  c024          r0 += 4
0x01E4BA70  app+0x04B970  8dea24c9      call 1806920
0x01E4BA74  app+0x04B974  8deadead      call 1792956
0x01E4BA78  app+0x04B978  4616          r6 = r4
0x01E4BA7A  app+0x04B97A  d2ec621f      r1 = [++r6=752]
0x01E4BA7E  app+0x04B97E  d1ec1409      r0 = [r1+404]
0x01E4BA82  app+0x04B982  51ed1019      r1 = h[r1+400] (u)
0x01E4BA86  app+0x04B986  b1e1a010      r1 = uextra(r1, p:1, l:8)
0x01E4BA8A  app+0x04B98A  6185          call -118
0x01E4BA8C  app+0x04B98C  6260          r2 = [r6+0]
0x01E4BA8E  app+0x04B98E  4520          r5 = 0
0x01E4BA90  app+0x04B990  d1ec2559      [r2+404] = r5
0x01E4BA94  app+0x04B994  8deadcad      call 1792952
0x01E4BA98  app+0x04B998  d1ec2c06      r0 = [r2+364]
0x01E4BA9C  app+0x04B99C  bfea9ad4      call -22220
