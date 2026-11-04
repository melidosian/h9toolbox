; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 243; undecoded chunks: 0
0x01E42B00  app+0x042A00  43ea0000      [r0+12] = 0x0
0x01E42B04  app+0x042A04  c030          r0 += 16
0x01E42B06  app+0x042A06  c0ea871c      goto 14606
0x01E42B0A  app+0x042A0A  0998          r1 = r0 + 24
0x01E42B0C  app+0x042A0C  4020          r0 = 0
0x01E42B0E  app+0x042A0E  1216          r2 = r1
0x01E42B10  app+0x042A10  0481          goto 2
0x01E42B12  app+0x042A12  c021          r0 += 1
0x01E42B14  app+0x042A14  2260          r2 = [r2+0]
0x01E42B16  app+0x042A16  81e8fc21      if (r2 != r1) goto -8
0x01E42B1A  app+0x042A1A  8000          rts
0x01E42B1C  app+0x042A1C  7504          [--sp] = {rets, r5, r4}
0x01E42B1E  app+0x042A1E  e29f          sp += -4
0x01E42B20  app+0x042A20  04d6          r4 = r0  #
0x01E42B22  app+0x042A22  0024          r0 = [sp+16]
0x01E42B24  app+0x042A24  40d6          r0 = r4  #
0x01E42B26  app+0x042A26  8020          [sp] = r0
0x01E42B28  app+0x042A28  bfeaf7fe      call -530
0x01E42B2C  app+0x042A2C  4898          r0 = r4 + 24
0x01E42B2E  app+0x042A2E  41e0d007      r1 = 2000
0x01E42B32  app+0x042A32  0216          r2 = r0
0x01E42B34  app+0x042A34  0493          goto 38
0x01E42B36  app+0x042A36  4b65          r3 = h[r4+10] (u)
0x01E42B38  app+0x042A38  ab7b          h[r2+-10] = r3
0x01E42B3A  app+0x042A3A  4b66          r3 = h[r4+12] (u)
0x01E42B3C  app+0x042A3C  ab7a          h[r2+-12] = r3
0x01E42B3E  app+0x042A3E  4b69          r3 = h[r4+18] (u)
0x01E42B40  app+0x042A40  ab7c          h[r2+-8] = r3
0x01E42B42  app+0x042A42  4365          r3 = [r4+20]
0x01E42B44  app+0x042A44  2d79          r5 = h[r2+-14] (u)
0x01E42B46  app+0x042A46  531b          r3 *= r5
0x01E42B48  app+0x042A48  f4e13131      r3 = r3 / r1 (s)
0x01E42B4C  app+0x042A4C  35a1          r5 = r3 << 1
0x01E42B4E  app+0x042A4E  d5d7          r5 = r5.l (u)  #
0x01E42B50  app+0x042A50  ad7d          h[r2+-6] = r5
0x01E42B52  app+0x042A52  4b64          r3 = h[r4+8] (u)
0x01E42B54  app+0x042A54  15e90043      if (r5 >= r3) {
0x01E42B58  app+0x042A58  fb3f          r3 += -1
0x01E42B5A  app+0x042A5A  ab7d          h[r2+-6] = r3
0x01E42B5C  app+0x042A5C  2260          r2 = [r2+0]
0x01E42B5E  app+0x042A5E  80e8ea21      if (r2 != r0) goto -44
0x01E42B62  app+0x042A62  0281          sp += 4
0x01E42B64  app+0x042A64  5504          {pc, r5, r4} = [sp++]
0x01E42B66  app+0x042A66  8817          r0 = r0.l (s)
0x01E42B68  app+0x042A68  9917          r1 = r1.l (s)
0x01E42B6A  app+0x042A6A  101e          r0 = r1 - r0
0x01E42B6C  app+0x042A6C  8000          rts
0x01E42B6E  app+0x042A6E  7504          [--sp] = {rets, r5, r4}
0x01E42B70  app+0x042A70  4120          r1 = 0
0x01E42B72  app+0x042A72  4224          r2 = 4
0x01E42B74  app+0x042A74  c3ffec12eb01  r3 = 32183020
0x01E42B7A  app+0x042A7A  4420          r4 = 0
0x01E42B7C  app+0x042A7C  0486          goto 12
0x01E42B7E  app+0x042A7E  d8ec3a54      r5 = [r3+r4<<2]
0x01E42B82  app+0x042A82  00e80550      if (r5 == r0) goto 10
0x01E42B86  app+0x042A86  c221          r2 += 1
0x01E42B88  app+0x042A88  c421          r4 += 1
0x01E42B8A  app+0x042A8A  84fcf815      if (r4 <= 10) goto -16
0x01E42B8E  app+0x042A8E  0481          goto 2
0x01E42B90  app+0x042A90  2116          r1 = r2
0x01E42B92  app+0x042A92  1017          r0 = r1.b0 (u)
0x01E42B94  app+0x042A94  5504          {pc, r5, r4} = [sp++]
0x01E42B96  app+0x042A96  40e07c06      r0 = 1660
0x01E42B9A  app+0x042A9A  c1ffd01a0100  r1 = 72400
0x01E42BA0  app+0x042AA0  d8ec1a00      r0 = [r1+r0<<2]
0x01E42BA4  app+0x042AA4  0062          r0 = [r0+8]
0x01E42BA6  app+0x042AA6  60e10f00      r0 = r0 & 0xF
0x01E42BAA  app+0x042AAA  8000          rts
0x01E42BAC  app+0x042AAC  c0ff0cd90000  r0 = 55564
0x01E42BB2  app+0x042AB2  0160          r1 = [r0+0]
0x01E42BB4  app+0x042AB4  4021          r0 = 1
0x01E42BB6  app+0x042AB6  8141          if (r1 != 0) goto 2
0x01E42BB8  app+0x042AB8  1016          r0 = r1
0x01E42BBA  app+0x042ABA  8000          rts
0x01E42BBC  app+0x042ABC  7504          [--sp] = {rets, r5, r4}
0x01E42BBE  app+0x042ABE  31e91f00      if (r1 >= 31) {
0x01E42BC2  app+0x042AC2  413f          r1 = 31
0x01E42BC4  app+0x042AC4  c2ffc4191e00  r2 = 1972676
0x01E42BCA  app+0x042ACA  2360          r3 = [r2+0]
0x01E42BCC  app+0x042ACC  c4ffd01a0100  r4 = 72400
0x01E42BD2  app+0x042AD2  30ea01c0      if ((r0 & 0x1) != 0) {
0x01E42BD6  app+0x042AD6  45e0521a      r5 = 6738
0x01E42BDA  app+0x042ADA  d8ee4115      b[r4+r5] = r1
0x01E42BDE  app+0x042ADE  73e11f30      r3 = r3 & 0xFFFFFFE0
0x01E42BE2  app+0x042AE2  1319          r3 |= r1
0x01E42BE4  app+0x042AE4  50e80808      if ((r0 & 2) == 0) goto 16
0x01E42BE8  app+0x042AE8  45e0531a      r5 = 6739
0x01E42BEC  app+0x042AEC  d8ee4115      b[r4+r5] = r1
0x01E42BF0  app+0x042AF0  15a5          r5 = r1 << 5
0x01E42BF2  app+0x042AF2  73e1783f      r3 = r3 & 0xFFFFFC1F
0x01E42BF6  app+0x042AF6  5319          r3 |= r5
0x01E42BF8  app+0x042AF8  50e80810      if ((r0 & 4) == 0) goto 16
0x01E42BFC  app+0x042AFC  45e0541a      r5 = 6740
0x01E42C00  app+0x042B00  d8ee4115      b[r4+r5] = r1
0x01E42C04  app+0x042B04  15aa          r5 = r1 << 10
0x01E42C06  app+0x042B06  73e1f83c      r3 = r3 & 0xFFFF83FF
0x01E42C0A  app+0x042B0A  5319          r3 |= r5
0x01E42C0C  app+0x042B0C  50e80918      if ((r0 & 8) == 0) goto 18
0x01E42C10  app+0x042B10  40e0551a      r0 = 6741
0x01E42C14  app+0x042B14  d8ee4110      b[r4+r0] = r1
0x01E42C18  app+0x042B18  10af          r0 = r1 << 15
0x01E42C1A  app+0x042B1A  71e1783a      r1 = r3 & 0xFFF07FFF
0x01E42C1E  app+0x042B1E  90e11030      r3 = r1 | r0
0x01E42C22  app+0x042B22  a360          [r2+0] = r3
0x01E42C24  app+0x042B24  5504          {pc, r5, r4} = [sp++]
0x01E42C26  app+0x042B26  7504          [--sp] = {rets, r5, r4}
0x01E42C28  app+0x042B28  c2ff202f1e00  r2 = 1978144
0x01E42C2E  app+0x042B2E  2460          r4 = [r2+0]
0x01E42C30  app+0x042B30  c3ffd01a0100  r3 = 72400
0x01E42C36  app+0x042B36  50e80900      if ((r0 & 1) == 0) goto 18
0x01E42C3A  app+0x042B3A  45e02d0d      r5 = 3373
0x01E42C3E  app+0x042B3E  d8ed3915      h[r3+r5<<1] = r1
0x01E42C42  app+0x042B42  c5ff0000ffff  r5 = 4294901760
0x01E42C48  app+0x042B48  d419          r4 &= r5
0x01E42C4A  app+0x042B4A  1419          r4 |= r1
0x01E42C4C  app+0x042B4C  30ea02c0      if ((r0 & 0x2) != 0) {
0x01E42C50  app+0x042B50  45e02e0d      r5 = 3374
0x01E42C54  app+0x042B54  d8ed3915      h[r3+r5<<1] = r1
0x01E42C58  app+0x042B58  c417          r4 = r4.l (u)
0x01E42C5A  app+0x042B5A  a4e14018      r4 <= insert(r1, p:16, l:16)
0x01E42C5E  app+0x042B5E  a460          [r2+0] = r4
0x01E42C60  app+0x042B60  2461          r4 = [r2+4]
0x01E42C62  app+0x042B62  50e80910      if ((r0 & 4) == 0) goto 18
0x01E42C66  app+0x042B66  45e02f0d      r5 = 3375
0x01E42C6A  app+0x042B6A  d8ed3915      h[r3+r5<<1] = r1
0x01E42C6E  app+0x042B6E  c5ff0000ffff  r5 = 4294901760
0x01E42C74  app+0x042B74  d419          r4 &= r5
0x01E42C76  app+0x042B76  1419          r4 |= r1
0x01E42C78  app+0x042B78  30ea08c0      if ((r0 & 0x8) != 0) {
0x01E42C7C  app+0x042B7C  40e0300d      r0 = 3376
0x01E42C80  app+0x042B80  d8ed3910      h[r3+r0<<1] = r1
0x01E42C84  app+0x042B84  c417          r4 = r4.l (u)
0x01E42C86  app+0x042B86  a4e14018      r4 <= insert(r1, p:16, l:16)
0x01E42C8A  app+0x042B8A  a461          [r2+4] = r4
0x01E42C8C  app+0x042B8C  5504          {pc, r5, r4} = [sp++]
0x01E42C8E  app+0x042B8E  c2ff202f1e00  r2 = 1978144
0x01E42C94  app+0x042B94  2160          r1 = [r2+0]
0x01E42C96  app+0x042B96  50e80c02      if ((r0 & 1) != 0) goto 24
0x01E42C9A  app+0x042B9A  30ea0240      if ((r0 & 0x2) != 0) {
0x01E42C9E  app+0x042B9E  90b0          r0 = r1 >> 16
0x01E42CA0  app+0x042BA0  8000          rts
0x01E42CA2  app+0x042BA2  2161          r1 = [r2+4]
0x01E42CA4  app+0x042BA4  20ea04c0      if ((r0 & 0x4) == 0) {
0x01E42CA8  app+0x042BA8  91b0          r1 = r1 >> 16
0x01E42CAA  app+0x042BAA  b0e18501      r0 = sextra(r0, p:3, l:1)
0x01E42CAE  app+0x042BAE  9019          r0 &= r1
0x01E42CB0  app+0x042BB0  8000          rts
0x01E42CB2  app+0x042BB2  9017          r0 = r1.l (u)
0x01E42CB4  app+0x042BB4  8000          rts
0x01E42CB6  app+0x042BB6  c1ffc8191e00  r1 = 1972680
0x01E42CBC  app+0x042BBC  1260          r2 = [r1+0]
0x01E42CBE  app+0x042BBE  73e10024      r3 = r2 & 0x7FFFFFFF
0x01E42CC2  app+0x042BC2  323f          r2 |= 2147483648
0x01E42CC4  app+0x042BC4  8041          if (r0 != 0) goto 2
0x01E42CC6  app+0x042BC6  3216          r2 = r3
0x01E42CC8  app+0x042BC8  9260          [r1+0] = r2
0x01E42CCA  app+0x042BCA  8000          rts
0x01E42CCC  app+0x042BCC  40e04d1a      r0 = 6733
0x01E42CD0  app+0x042BD0  c1ffd01a0100  r1 = 72400
0x01E42CD6  app+0x042BD6  d8ee1020      r2 = b[r1+r0] (u)
0x01E42CDA  app+0x042BDA  02f80b06      if (r2 == 3) goto 22
0x01E42CDE  app+0x042BDE  d8ee1000      r0 = b[r1+r0] (u)
0x01E42CE2  app+0x042BE2  00f80704      if (r0 == 2) goto 14
0x01E42CE6  app+0x042BE6  c0ffc8191e00  r0 = 1972680
0x01E42CEC  app+0x042BEC  0160          r1 = [r0+0]
0x01E42CEE  app+0x042BEE  4020          r0 = 0
0x01E42CF0  app+0x042BF0  51e80168      if ((r1 & 8192) == 0) goto 2
0x01E42CF4  app+0x042BF4  4021          r0 = 1
0x01E42CF6  app+0x042BF6  8000          rts
0x01E42CF8  app+0x042BF8  7604          [--sp] = {rets, r6-r4}
0x01E42CFA  app+0x042BFA  40e04d1a      r0 = 6733
0x01E42CFE  app+0x042BFE  c4ffd01a0100  r4 = 72400
0x01E42D04  app+0x042C04  d8ee4010      r1 = b[r4+r0] (u)
0x01E42D08  app+0x042C08  01f86906      if (r1 == 3) goto 210
0x01E42D0C  app+0x042C0C  d8ee4000      r0 = b[r4+r0] (u)
0x01E42D10  app+0x042C10  00f86504      if (r0 == 2) goto 202
0x01E42D14  app+0x042C14  4026          r0 = 6
0x01E42D16  app+0x042C16  bfea1df2      call -7110
0x01E42D1A  app+0x042C1A  8dea8bf4      call 1829142
0x01E42D1E  app+0x042C1E  40e04d1a      r0 = 6733
0x01E42D22  app+0x042C22  d8ee4000      r0 = b[r4+r0] (u)
0x01E42D26  app+0x042C26  00f8580c      if (r0 == 6) goto 176
0x01E42D2A  app+0x042C2A  80ea6010      call 8384
0x01E42D2E  app+0x042C2E  41e07c06      r1 = 1660
0x01E42D32  app+0x042C32  d8ec4a21      r2 = [r4+r1<<2]
0x01E42D36  app+0x042C36  41f0ec19      r1 = 6636  #
0x01E42D3A  app+0x042C3A  2662          r6 = [r2+8]
0x01E42D3C  app+0x042C3C  4320          r3 = 0
0x01E42D3E  app+0x042C3E  d8ee4131      b[r4+r1] = r3
0x01E42D42  app+0x042C42  2161          r1 = [r2+4]
0x01E42D44  app+0x042C44  0144          if (r1 == 0) goto 8
0x01E42D46  app+0x042C46  10e1ec49      r0 = r4 + 6636
0x01E42D4A  app+0x042C4A  c100          call r1
0x01E42D4C  app+0x042C4C  149c          goto 120
0x01E42D4E  app+0x042C4E  c1ffc0191e00  r1 = 1972672
0x01E42D54  app+0x042C54  03ef801e      [r1+12] |= 0x400
0x01E42D58  app+0x042C58  02ef0019      [r1+8] |= 0x200000
0x01E42D5C  app+0x042C5C  c228          r2 += 8
0x01E42D5E  app+0x042C5E  4321          r3 = 1
0x01E42D60  app+0x042C60  4521          r5 = 1
0x01E42D62  app+0x042C62  36ea800a      if ((r6 & 0x40000) != 0) {
0x01E42D66  app+0x042C66  4520          r5 = 0
0x01E42D68  app+0x042C68  66e10f60      r6 = r6 & 0xF
0x01E42D6C  app+0x042C6C  36e80300      if (r6 == 3) {
0x01E42D70  app+0x042C70  4320          r3 = 0
0x01E42D72  app+0x042C72  53fa0400      if ((r3 & r5) == 0) goto 8
0x01E42D76  app+0x042C76  c0ffffffdfff  r0 = 4292870143
0x01E42D7C  app+0x042C7C  0486          goto 12
0x01E42D7E  app+0x042C7E  61ffff000500  if ((r0 & 0xFF) != 0) goto 10
0x01E42D84  app+0x042C84  c0fffcffdfff  r0 = 4292870140
0x01E42D8A  app+0x042C8A  64e80210      [r1+0] &= r0
0x01E42D8E  app+0x042C8E  c1ef4c17      [r1+4] &= 0xFCCFFFFF
0x01E42D92  app+0x042C92  c1ef4c16      [r1+4] &= 0xF33FFFFF
0x01E42D96  app+0x042C96  2842          r0 = b[r2+2] (u)
0x01E42D98  app+0x042C98  50e80712      if ((r0 & 4) != 0) goto 14
0x01E42D9C  app+0x042C9C  c1ef8816      [r1+4] &= 0xFBBFFFFF
0x01E42DA0  app+0x042CA0  106a          r0 = [r1+40]
0x01E42DA2  app+0x042CA2  20ea8009      if ((r0 & 0x100000) == 0) {
0x01E42DA6  app+0x042CA6  c1ef0816      [r1+4] &= 0xF77FFFFF
0x01E42DAA  app+0x042CAA  90f13205      r0 = r3 & r5  #
0x01E42DAE  app+0x042CAE  2a42          r2 = b[r2+2] (u)
0x01E42DB0  app+0x042CB0  22ea0800      if ((r2 & 0x8) == 0) {
0x01E42DB4  app+0x042CB4  c1ef8816      [r1+4] &= 0xFBBFFFFF
0x01E42DB8  app+0x042CB8  0042          if (r0 == 0) goto 4
0x01E42DBA  app+0x042CBA  03ef001a      [r1+12] |= 0x80000
0x01E42DBE  app+0x042CBE  c0ef2018      [r1+0] &= 0xFF5FFFFF
0x01E42DC2  app+0x042CC2  c3ef801e      [r1+12] &= 0xFFFFFBFF
0x01E42DC6  app+0x042CC6  c0ff002f1e00  r0 = 1978112
0x01E42DCC  app+0x042CCC  c0ef880f      [r0+0] &= 0xFFFFFEEF
0x01E42DD0  app+0x042CD0  40e04d1a      r0 = 6733
0x01E42DD4  app+0x042CD4  4126          r1 = 6
0x01E42DD6  app+0x042CD6  d8ee4110      b[r4+r0] = r1
0x01E42DDA  app+0x042CDA  8dea39f4      call 1828978
0x01E42DDE  app+0x042CDE  5604          {pc, r6-r4} = [sp++]
0x01E42DE0  app+0x042CE0  7704          [--sp] = {rets, r7-r4}
0x01E42DE2  app+0x042CE2  c2ff6438e401  r2 = 31733860
0x01E42DE8  app+0x042CE8  402c          r0 = 12
0x01E42DEA  app+0x042CEA  4122          r1 = 2
0x01E42DEC  app+0x042CEC  bfeab50c      call -124566
0x01E42DF0  app+0x042CF0  c5ffd01a0100  r5 = 72400
0x01E42DF6  app+0x042CF6  14e1ec59      r4 = r5 + 6636
0x01E42DFA  app+0x042CFA  4120          r1 = 0
0x01E42DFC  app+0x042CFC  42e01001      r2 = 272
