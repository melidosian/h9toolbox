; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 275; undecoded chunks: 1
0x01E42D80  app+0x042C80  ff00          sti r15
0x01E42D82  app+0x042C82  0500          .byte 0x05, 0x00
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
0x01E42E00  app+0x042D00  4620          r6 = 0
0x01E42E02  app+0x042D02  4016          r0 = r4
0x01E42E04  app+0x042D04  96ead67c      call 2947500
0x01E42E08  app+0x042D08  c1ff3c670000  r1 = 26428
0x01E42E0E  app+0x042D0E  1062          r0 = [r1+8]
0x01E42E10  app+0x042D10  c2ff0cd90000  r2 = 55564
0x01E42E16  app+0x042D16  42f07c06      r2 = 1660  #
0x01E42E1A  app+0x042D1A  a460          [r2+0] = r4
0x01E42E1C  app+0x042D1C  d8ec5b12      [r5+r2<<2] = r1
0x01E42E20  app+0x042D20  30ea0088      if ((r0 & 0x800000) != 0) {
0x01E42E24  app+0x042D24  b1e11002      r1 = uextra(r0, p:4, l:4)
0x01E42E28  app+0x042D28  42e0ed19      r2 = 6637
0x01E42E2C  app+0x042D2C  d8ee5112      b[r5+r2] = r1
0x01E42E30  app+0x042D30  12e14c5a      r2 = r5 + 6732
0x01E42E34  app+0x042D34  e6f04062      r6 = r6 + 0x40004000  #
0x01E42E38  app+0x042D38  ae41          b[r2+1] = r6
0x01E42E3A  app+0x042D3A  6716          r7 = r6
0x01E42E3C  app+0x042D3C  50ec2d60      d[r2+12] = r7_r6
0x01E42E40  app+0x042D40  43e00040      r3 = 16384
0x01E42E44  app+0x042D44  61f10f00      r1 = r0 & 0xF  #
0x01E42E48  app+0x042D48  ab6a          h[r2+20] = r3
0x01E42E4A  app+0x042D4A  01fc1812      if (r1 > 9) goto 48
0x01E42E4E  app+0x042D4E  5824          r0 = 100
0x01E42E50  app+0x042D50  94e10201      r0 = r0 & (1 << r1)
0x01E42E54  app+0x042D54  8047          if (r0 != 0) goto 14
0x01E42E56  app+0x042D56  40e08001      r0 = 384
0x01E42E5A  app+0x042D5A  94e10201      r0 = r0 & (1 << r1)
0x01E42E5E  app+0x042D5E  004c          if (r0 == 0) goto 24
0x01E42E60  app+0x042D60  4023          r0 = 3
0x01E42E62  app+0x042D62  0496          goto 44
0x01E42E64  app+0x042D64  40e02f0d      r0 = 3375
0x01E42E68  app+0x042D68  4120          r1 = 0
0x01E42E6A  app+0x042D6A  d8ed5910      h[r5+r0<<1] = r1
0x01E42E6E  app+0x042D6E  c021          r0 += 1
0x01E42E70  app+0x042D70  d8ed5910      h[r5+r0<<1] = r1
0x01E42E74  app+0x042D74  4022          r0 = 2
0x01E42E76  app+0x042D76  048c          goto 24
0x01E42E78  app+0x042D78  4024          r0 = 4
0x01E42E7A  app+0x042D7A  01f80912      if (r1 == 9) goto 18
0x01E42E7E  app+0x042D7E  40e02f0d      r0 = 3375
0x01E42E82  app+0x042D82  4120          r1 = 0
0x01E42E84  app+0x042D84  d8ed5910      h[r5+r0<<1] = r1
0x01E42E88  app+0x042D88  c021          r0 += 1
0x01E42E8A  app+0x042D8A  d8ed5910      h[r5+r0<<1] = r1
0x01E42E8E  app+0x042D8E  4021          r0 = 1
0x01E42E90  app+0x042D90  14e1545a      r4 = r5 + 6740
0x01E42E94  app+0x042D94  c842          b[r4+2] = r0
0x01E42E96  app+0x042D96  4020          r0 = 0
0x01E42E98  app+0x042D98  c064          [r4+16] = r0
0x01E42E9A  app+0x042D9A  c1ff44001e00  r1 = 1966148
0x01E42EA0  app+0x042DA0  00ef8014      [r1+0] |= 0x40000000
0x01E42EA4  app+0x042DA4  11e17c19      r1 = r1 + 6524
0x01E42EA8  app+0x042DA8  00ef801d      [r1+0] |= 0x1000
0x01E42EAC  app+0x042DAC  c0ef001d      [r1+0] &= 0xFFFFDFFF
0x01E42EB0  app+0x042DB0  c0ef801c      [r1+0] &= 0xFFFFBFFF
0x01E42EB4  app+0x042DB4  c0ef001c      [r1+0] &= 0xFFFF7FFF
0x01E42EB8  app+0x042DB8  c0ef801b      [r1+0] &= 0xFFFEFFFF
0x01E42EBC  app+0x042DBC  1260          r2 = [r1+0]
0x01E42EBE  app+0x042DBE  72e1c02a      r2 = r2 & 0xFFF9FFFF
0x01E42EC2  app+0x042DC2  3232          r2 |= 262144
0x01E42EC4  app+0x042DC4  12f14015      r2 = r1 + 5440  #
0x01E42EC8  app+0x042DC8  9260          [r1+0] = r2
0x01E42ECA  app+0x042DCA  c0ef802f      [r2+0] &= 0xFFFFFEFF
0x01E42ECE  app+0x042DCE  c3ef4010      [r1+12] &= 0xFFFFFFBF
0x01E42ED2  app+0x042DD2  a060          [r2+0] = r0
0x01E42ED4  app+0x042DD4  50ed2103      h[r2+48] = r0
0x01E42ED8  app+0x042DD8  bfea3cf1      call -7560
0x01E42EDC  app+0x042DDC  00e15840      r0 = r4 + 88
0x01E42EE0  app+0x042DE0  8deae5fe      call 1834442
0x01E42EE4  app+0x042DE4  3704          {rets, r7-r4} = [sp++]
0x01E42EE6  app+0x042DE6  c0ea051a      goto 13322
0x01E42EEA  app+0x042DEA  42e0ff7f      r2 = 32767
0x01E42EEE  app+0x042DEE  41e02823      r1 = 9000
0x01E42EF2  app+0x042DF2  90e80002      if (r0 != r2) {
0x01E42EF6  app+0x042DF6  0116          r1 = r0
0x01E42EF8  app+0x042DF8  c0ff342f1e00  r0 = 1978164
0x01E42EFE  app+0x042DFE  8960          h[r0+0] = r1
0x01E42F00  app+0x042E00  8000          rts
0x01E42F02  app+0x042E02  40e09d06      r0 = 1693
0x01E42F06  app+0x042E06  c1ffd01a0100  r1 = 72400
0x01E42F0C  app+0x042E0C  c2ff24e50000  r2 = 58660
0x01E42F12  app+0x042E12  d8ec1b20      [r1+r0<<2] = r2
0x01E42F16  app+0x042E16  40e03c0d      r0 = 3388
0x01E42F1A  app+0x042E1A  42e00020      r2 = 8192
0x01E42F1E  app+0x042E1E  d8ed1920      h[r1+r0<<1] = r2
0x01E42F22  app+0x042E22  8000          rts
0x01E42F24  app+0x042E24  7504          [--sp] = {rets, r5, r4}
0x01E42F26  app+0x042E26  c1ff002f1e00  r1 = 1978112
0x01E42F2C  app+0x042E2C  00ef2016      [r1+0] |= 0xA000000
0x01E42F30  app+0x042E30  0261          r2 = [r0+4]
0x01E42F32  app+0x042E32  2562          r5 = [r2+8]
0x01E42F34  app+0x042E34  1460          r4 = [r1+0]
0x01E42F36  app+0x042E36  2b88          r3 = r2 + 8
0x01E42F38  app+0x042E38  55e805a2      if ((r5 & 1048576) != 0) goto 10
0x01E42F3C  app+0x042E3C  343a          r4 |= 67108864
0x01E42F3E  app+0x042E3E  9460          [r1+0] = r4
0x01E42F40  app+0x042E40  1460          r4 = [r1+0]
0x01E42F42  app+0x042E42  3438          r4 |= 16777216
0x01E42F44  app+0x042E44  0484          goto 8
0x01E42F46  app+0x042E46  bc3a          r4 &= 4227858431
0x01E42F48  app+0x042E48  9460          [r1+0] = r4
0x01E42F4A  app+0x042E4A  1460          r4 = [r1+0]
0x01E42F4C  app+0x042E4C  bc38          r4 &= 4278190079
0x01E42F4E  app+0x042E4E  9460          [r1+0] = r4
0x01E42F50  app+0x042E50  4fea7f10      [r1+60] = 0x7F
0x01E42F54  app+0x042E54  3b42          r3 = b[r3+2] (u)
0x01E42F56  app+0x042E56  33ea0400      if ((r3 & 0x4) != 0) {
0x01E42F5A  app+0x042E5A  00ef8018      [r1+0] |= 0x400000
0x01E42F5E  app+0x042E5E  50ee0a06      r0 = b[r0+106] (u)
0x01E42F62  app+0x042E62  f83f          r0 += -1
0x01E42F64  app+0x042E64  00fc4006      if (r0 > 3) goto 128
0x01E42F68  app+0x042E68  0001          tbb [r0]
0x01E42F6A  app+0x042E6A  0212          r2 = b[r0++=r12] (u)
0x01E42F6C  app+0x042E6C  202a          r0 = [sp+168]
0x01E42F6E  app+0x042E6E  2062          r0 = [r2+8]
0x01E42F70  app+0x042E70  62e10f00      r2 = r0 & 0xF
0x01E42F74  app+0x042E74  02fc4012      if (r2 > 9) goto 128
0x01E42F78  app+0x042E78  60e0800b      r0 = 0x10000
0x01E42F7C  app+0x042E7C  0201          tbb [r2]
0x01E42F7E  app+0x042E7E  332b          r3 |= 2048
0x01E42F80  app+0x042E80  052e          r5 = [sp+56]
0x01E42F82  app+0x042E82  3d05          r5 = [r3++=-4]
0x01E42F84  app+0x042E84  131d          r3 = r1 + r4
0x01E42F86  app+0x042E86  2831          r8 = [sp+196]
0x01E42F88  app+0x042E88  60e0400b      r0 = 0x30000
0x01E42F8C  app+0x042E8C  148b          goto 86
0x01E42F8E  app+0x042E8E  2062          r0 = [r2+8]
0x01E42F90  app+0x042E90  62e10f00      r2 = r0 & 0xF
0x01E42F94  app+0x042E94  60e0400b      r0 = 0x30000
0x01E42F98  app+0x042E98  02f82404      if (r2 == 2) goto 72
0x01E42F9C  app+0x042E9C  02f8220a      if (r2 == 5) goto 68
0x01E42FA0  app+0x042EA0  82f82a0c      if (r2 != 6) goto 84
0x01E42FA4  app+0x042EA4  60e01409      r0 = 0x250000
0x01E42FA8  app+0x042EA8  049d          goto 58
0x01E42FAA  app+0x042EAA  2062          r0 = [r2+8]
0x01E42FAC  app+0x042EAC  60e10f00      r0 = r0 & 0xF
0x01E42FB0  app+0x042EB0  00f80d10      if (r0 == 8) goto 26
0x01E42FB4  app+0x042EB4  80f8200e      if (r0 != 7) goto 64
0x01E42FB8  app+0x042EB8  60e0e00a      r0 = 0x70000
0x01E42FBC  app+0x042EBC  0493          goto 38
0x01E42FBE  app+0x042EBE  2062          r0 = [r2+8]
0x01E42FC0  app+0x042EC0  62e10f00      r2 = r0 & 0xF
0x01E42FC4  app+0x042EC4  60e0700a      r0 = 0xF0000
0x01E42FC8  app+0x042EC8  02f80c12      if (r2 == 9) goto 24
0x01E42FCC  app+0x042ECC  5504          {pc, r5, r4} = [sp++]
0x01E42FCE  app+0x042ECE  60e0300a      r0 = 0xB0000
0x01E42FD2  app+0x042ED2  0488          goto 16
0x01E42FD4  app+0x042ED4  60e0000b      r0 = 0x20000
0x01E42FD8  app+0x042ED8  0485          goto 10
0x01E42FDA  app+0x042EDA  60e00409      r0 = 0x210000
0x01E42FDE  app+0x042EDE  0482          goto 4
0x01E42FE0  app+0x042EE0  60e0700a      r0 = 0xF0000
0x01E42FE4  app+0x042EE4  64e80010      [r1+0] |= r0
0x01E42FE8  app+0x042EE8  4027          r0 = 7
0x01E42FEA  app+0x042EEA  0483          goto 6
0x01E42FEC  app+0x042EEC  4eea801c      [r1+56] = 0x4000
0x01E42FF0  app+0x042EF0  f83f          r0 += -1
0x01E42FF2  app+0x042EF2  f05c          if (r0 != 0) goto -8
0x01E42FF4  app+0x042EF4  00ef601c      [r1+0] |= 0xE000
0x01E42FF8  app+0x042EF8  5504          {pc, r5, r4} = [sp++]
0x01E42FFA  app+0x042EFA  7504          [--sp] = {rets, r5, r4}
0x01E42FFC  app+0x042EFC  0416          r4 = r0
0x01E42FFE  app+0x042EFE  4840          r0 = b[r4+0] (u)
0x01E43000  app+0x042F00  80f8b100      if (r0 != 0) goto 354
0x01E43004  app+0x042F04  4061          r0 = [r4+4]
0x01E43006  app+0x042F06  0160          r1 = [r0+0]
0x01E43008  app+0x042F08  0143          if (r1 == 0) goto 6
0x01E4300A  app+0x042F0A  4016          r0 = r4
0x01E4300C  app+0x042F0C  c100          call r1
0x01E4300E  app+0x042F0E  5489          goto 338
0x01E43010  app+0x042F10  c5ffc0191e00  r5 = 1972672
0x01E43016  app+0x042F16  0befe654      [r5+44] |= 0x73000000
0x01E4301A  app+0x042F1A  c2ef0059      [r5+8] &= 0xFFDFFFFF
0x01E4301E  app+0x042F1E  40e0e803      r0 = 1000
0x01E43022  app+0x042F22  80ea993d      call 31538
0x01E43026  app+0x042F26  03ef805e      [r5+12] |= 0x400
0x01E4302A  app+0x042F2A  40e0e803      r0 = 1000
0x01E4302E  app+0x042F2E  80ea933d      call 31526
0x01E43032  app+0x042F32  03ef8059      [r5+12] |= 0x100000
0x01E43036  app+0x042F36  c0ff03feaff7  r0 = 4155506179
0x01E4303C  app+0x042F3C  64e80250      [r5+0] &= r0
0x01E43040  app+0x042F40  7028          r0 = 200
0x01E43042  app+0x042F42  80ea893d      call 31506
0x01E43046  app+0x042F46  5063          r0 = [r5+12]
0x01E43048  app+0x042F48  4161          r1 = [r4+4]
0x01E4304A  app+0x042F4A  1262          r2 = [r1+8]
0x01E4304C  app+0x042F4C  a2b5          r2 = r2 >> 21
0x01E4304E  app+0x042F4E  a0e18428      r0 <= insert(r2, p:17, l:1)
0x01E43052  app+0x042F52  d063          [r5+12] = r0
0x01E43054  app+0x042F54  03ef805b      [r5+12] |= 0x10000
0x01E43058  app+0x042F58  5062          r0 = [r5+8]
0x01E4305A  app+0x042F5A  1162          r1 = [r1+8]
0x01E4305C  app+0x042F5C  11a5          r1 = r1 << 5
0x01E4305E  app+0x042F5E  91b1          r1 = r1 >> 17
0x01E43060  app+0x042F60  a0e19018      r0 <= insert(r1, p:17, l:4)
0x01E43064  app+0x042F64  40f0e803      r0 = 1000  #
0x01E43068  app+0x042F68  d062          [r5+8] = r0
0x01E4306A  app+0x042F6A  80ea753d      call 31466
0x01E4306E  app+0x042F6E  4061          r0 = [r4+4]
0x01E43070  app+0x042F70  0162          r1 = [r0+8]
0x01E43072  app+0x042F72  62e10f10      r2 = r1 & 0xF
0x01E43076  app+0x042F76  02fc1d12      if (r2 > 9) goto 58
0x01E4307A  app+0x042F7A  0988          r1 = r0 + 8
0x01E4307C  app+0x042F7C  0201          tbb [r2]
0x01E4307E  app+0x042F7E  0b0e          h[r0++=r12] = r3
0x01E43080  app+0x042F80  0505          r5 = [r0++=4]
0x01E43082  app+0x042F82  1b05          r3 = [r1++=-4]
0x01E43084  app+0x042F84  0811          b[r0++=r10] = r0
0x01E43086  app+0x042F86  1408          r4 = [r1++=r8]
0x01E43088  app+0x042F88  01ef4c57      [r5+4] |= 0x3300000
0x01E4308C  app+0x042F8C  048e          goto 28
0x01E4308E  app+0x042F8E  01ef7f56      [r5+4] |= 0xFF00000
0x01E43092  app+0x042F92  0490          goto 32
0x01E43094  app+0x042F94  01ef8857      [r5+4] |= 0x1100000
0x01E43098  app+0x042F98  0488          goto 16
0x01E4309A  app+0x042F9A  01ef0857      [r5+4] |= 0x2200000
0x01E4309E  app+0x042F9E  0485          goto 10
0x01E430A0  app+0x042FA0  01efee56      [r5+4] |= 0x7700000
0x01E430A4  app+0x042FA4  0487          goto 14
0x01E430A6  app+0x042FA6  01ef3b56      [r5+4] |= 0xBB00000
0x01E430AA  app+0x042FAA  1942          r1 = b[r1+2] (u)
0x01E430AC  app+0x042FAC  31ea0400      if ((r1 & 0x4) != 0) {
0x01E430B0  app+0x042FB0  01ef8856      [r5+4] |= 0x4400000
0x01E430B4  app+0x042FB4  5161          r1 = [r5+4]
0x01E430B6  app+0x042FB6  0262          r2 = [r0+8]
0x01E430B8  app+0x042FB8  22ac          r2 = r2 << 12
0x01E430BA  app+0x042FBA  a2bc          r2 = r2 >> 28
0x01E430BC  app+0x042FBC  a1e1082e      r1 <= insert(r2, p:28, l:2)
0x01E430C0  app+0x042FC0  d161          [r5+4] = r1
0x01E430C2  app+0x042FC2  5162          r1 = [r5+8]
0x01E430C4  app+0x042FC4  0262          r2 = [r0+8]
0x01E430C6  app+0x042FC6  22b4          r2 = r2 << 20
0x01E430C8  app+0x042FC8  a2bc          r2 = r2 >> 28
0x01E430CA  app+0x042FCA  a1e10c2e      r1 <= insert(r2, p:28, l:3)
0x01E430CE  app+0x042FCE  d162          [r5+8] = r1
0x01E430D0  app+0x042FD0  5162          r1 = [r5+8]
0x01E430D2  app+0x042FD2  71e17016      r1 = r1 & 0xF0FFFFFF
0x01E430D6  app+0x042FD6  41e14017      r1 = r1 | 0x3000000
0x01E430DA  app+0x042FDA  d162          [r5+8] = r1
0x01E430DC  app+0x042FDC  00ef0058      [r5+0] |= 0x800000
