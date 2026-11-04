; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 439; undecoded chunks: 0
0x01E41000  app+0x040F00  0052          if (r0 == 0) goto 36
0x01E41002  app+0x040F02  e1e14860      r1 = r6 * 0x48
0x01E41006  app+0x040F06  1018          r0 += r1
0x01E41008  app+0x040F08  5228          r2 = 72
0x01E4100A  app+0x040F0A  5116          r1 = r5
0x01E4100C  app+0x040F0C  96eacc8b      call 2955160
0x01E41010  app+0x040F10  50ee4601      r0 = b[r4+22] (u)
0x01E41014  app+0x040F14  496a          r1 = h[r4+20] (u)
0x01E41016  app+0x040F16  a1e14008      r1 <= insert(r0, p:16, l:16)
0x01E4101A  app+0x040F1A  90b0          r0 = r1 >> 16
0x01E4101C  app+0x040F1C  52ee4601      b[r4+22] = r0
0x01E41020  app+0x040F20  40e1001f      r0 = r1 | 0x200
0x01E41024  app+0x040F24  c86a          h[r4+20] = r0
0x01E41026  app+0x040F26  3604          {rets, r6-r4} = [sp++]
0x01E41028  app+0x040F28  ceea1203      goto 1836580
0x01E4102C  app+0x040F2C  7504          [--sp] = {rets, r5, r4}
0x01E4102E  app+0x040F2E  2416          r4 = r2
0x01E41030  app+0x040F30  1516          r5 = r1
0x01E41032  app+0x040F32  618e          call -100
0x01E41034  app+0x040F34  0116          r1 = r0
0x01E41036  app+0x040F36  8eeafd02      call 1836538
0x01E4103A  app+0x040F3A  0145          if (r1 == 0) goto 10
0x01E4103C  app+0x040F3C  51d6          r1 = r5  #
0x01E4103E  app+0x040F3E  1067          r0 = [r1+28]
0x01E41040  app+0x040F40  4216          r2 = r4
0x01E41042  app+0x040F42  80eadd0b      call 6074
0x01E41046  app+0x040F46  3504          {rets, r5, r4} = [sp++]
0x01E41048  app+0x040F48  ceea0203      goto 1836548
0x01E4104C  app+0x040F4C  1004          [--sp] = rets
0x01E4104E  app+0x040F4E  6180          call -128
0x01E41050  app+0x040F50  0116          r1 = r0
0x01E41052  app+0x040F52  8eeaef02      call 1836510
0x01E41056  app+0x040F56  0156          if (r1 == 0) goto 44
0x01E41058  app+0x040F58  1062          r0 = [r1+8]
0x01E4105A  app+0x040F5A  0054          if (r0 == 0) goto 40
0x01E4105C  app+0x040F5C  42e05427      r2 = 10068
0x01E41060  app+0x040F60  c3ffd01a0100  r3 = 72400
0x01E41066  app+0x040F66  3218          r2 += r3
0x01E41068  app+0x040F68  1083          rep 4 4 {
0x01E4106A  app+0x040F6A  2305          r3 = [r2++=4]
0x01E4106C  app+0x040F6C  8305          [r0++=4] = r3
0x01E4106E  app+0x040F6E  50ee1601      r0 = b[r1+22] (u)
0x01E41072  app+0x040F72  1a6a          r2 = h[r1+20] (u)
0x01E41074  app+0x040F74  a2e14008      r2 <= insert(r0, p:16, l:16)
0x01E41078  app+0x040F78  a0b0          r0 = r2 >> 16
0x01E4107A  app+0x040F7A  52ee1601      b[r1+22] = r0
0x01E4107E  app+0x040F7E  40e1002f      r0 = r2 | 0x200
0x01E41082  app+0x040F82  986a          h[r1+20] = r0
0x01E41084  app+0x040F84  8804          {rets} = [sp++]
0x01E41086  app+0x040F86  ceeae302      goto 1836486
0x01E4108A  app+0x040F8A  7404          [--sp] = {rets, r4}
0x01E4108C  app+0x040F8C  c4fff0d80000  r4 = 55536
0x01E41092  app+0x040F92  4060          r0 = [r4+0]
0x01E41094  app+0x040F94  004e          if (r0 == 0) goto 28
0x01E41096  app+0x040F96  50ed0805      r0 = h[r0+88] (u)
0x01E4109A  app+0x040F9A  804b          if (r0 != 0) goto 22
0x01E4109C  app+0x040F9C  c1ffc012e401  r1 = 31724224
0x01E410A2  app+0x040FA2  4020          r0 = 0
0x01E410A4  app+0x040FA4  4222          r2 = 2
0x01E410A6  app+0x040FA6  4321          r3 = 1
0x01E410A8  app+0x040FA8  8eea2c14      call 1845336
0x01E410AC  app+0x040FAC  4160          r1 = [r4+0]
0x01E410AE  app+0x040FAE  50ed1905      h[r1+88] = r0
0x01E410B2  app+0x040FB2  5404          {pc, r4} = [sp++]
0x01E410B4  app+0x040FB4  7a04          [--sp] = {rets, r10-r4}
0x01E410B6  app+0x040FB6  c8fff0d80000  r8 = 55536
0x01E410BC  app+0x040FBC  d0ec8020      r2 = [r8+0]
0x01E410C0  app+0x040FC0  0416          r4 = r0
0x01E410C2  app+0x040FC2  2247          if (r2 == 0) goto 142
0x01E410C4  app+0x040FC4  4120          r1 = 0
0x01E410C6  app+0x040FC6  c214          r10 = 0
0x01E410C8  app+0x040FC8  2016          r0 = r2
0x01E410CA  app+0x040FCA  8eea7b0d      call 1841910
0x01E410CE  app+0x040FCE  d0ec8010      r1 = [r8+0]
0x01E410D2  app+0x040FD2  4321          r3 = 1
0x01E410D4  app+0x040FD4  60e0009c      r9 = 0x8000
0x01E410D8  app+0x040FD8  4520          r5 = 0
0x01E410DA  app+0x040FDA  1487          goto 78
0x01E410DC  app+0x040FDC  c8f13067      r6 = r3 << r7  #
0x01E410E0  app+0x040FE0  0261          r2 = [r0+4]
0x01E410E2  app+0x040FE2  62fa2100      if ((r2 & r6) == 0) goto 66
0x01E410E6  app+0x040FE6  e0f10a70      r0 = r7 * 0xA  #
0x01E410EA  app+0x040FEA  0260          r2 = [r0+0]
0x01E410EC  app+0x040FEC  0452          if (r4 == 0) goto 36
0x01E410EE  app+0x040FEE  2018          r0 += r2
0x01E410F0  app+0x040FF0  52ee05a0      b[r0+5] = r10
0x01E410F4  app+0x040FF4  50ed03a0      h[r0+2] = r10
0x01E410F8  app+0x040FF8  6017          r0 = r6.b0 (u)
0x01E410FA  app+0x040FFA  80eac80d      call 7056
0x01E410FE  app+0x040FFE  d0ec8010      r1 = [r8+0]
0x01E41102  app+0x041002  d0ec1025      r2 = [r1+80]
0x01E41106  app+0x041006  e0e13200      r0 = r0 * 0x32
0x01E4110A  app+0x04100A  f4e10109      r0 = r0 / r9 (s)
0x01E4110E  app+0x04100E  c021          r0 += 1
0x01E41110  app+0x041010  0486          goto 12
0x01E41112  app+0x041012  2018          r0 += r2
0x01E41114  app+0x041014  0e44          r6 = b[r0+4] (u)
0x01E41116  app+0x041016  8e45          b[r0+5] = r6
0x01E41118  app+0x041018  0e60          r6 = h[r0+0] (u)
0x01E4111A  app+0x04101A  8e61          h[r0+2] = r6
0x01E4111C  app+0x04101C  4020          r0 = 0
0x01E4111E  app+0x04101E  e6e10a70      r6 = r7 * 0xA
0x01E41122  app+0x041022  6218          r2 += r6
0x01E41124  app+0x041024  a848          b[r2+8] = r0
0x01E41126  app+0x041026  ab47          b[r2+7] = r3
0x01E41128  app+0x041028  c521          r5 += 1
0x01E4112A  app+0x04102A  00e15010      r0 = r1 + 80
0x01E4112E  app+0x04102E  57d7          r7 = r5.b0 (u)  #
0x01E41130  app+0x041030  0a4a          r2 = b[r0+10] (u)
0x01E41132  app+0x041032  82e9d371      if (r7 < r2) goto -90
0x01E41136  app+0x041036  8445          if (r4 != 0) goto 10
0x01E41138  app+0x041038  4121          r1 = 1
0x01E4113A  app+0x04103A  894c          b[r0+12] = r1
0x01E4113C  app+0x04103C  4020          r0 = 0
0x01E4113E  app+0x04103E  80eaba0d      call 7028
0x01E41142  app+0x041042  80eac30d      call 7046
0x01E41146  app+0x041046  0041          if (r0 == 0) goto 2
0x01E41148  app+0x041048  5180          call -192
0x01E4114A  app+0x04104A  d0ec8000      r0 = [r8+0]
0x01E4114E  app+0x04104E  8eea750d      call 1841898
0x01E41152  app+0x041052  5a04          {pc, r10-r4} = [sp++]
0x01E41154  app+0x041054  7504          [--sp] = {rets, r5, r4}
0x01E41156  app+0x041056  c4fff0d80000  r4 = 55536
0x01E4115C  app+0x04105C  00f82208      if (r0 == 4) goto 68
0x01E41160  app+0x041060  00f82504      if (r0 == 2) goto 74
0x01E41164  app+0x041064  9043          if (r0 != 0) goto 70
0x01E41166  app+0x041066  4060          r0 = [r4+0]
0x01E41168  app+0x041068  9041          if (r0 != 0) goto 66
0x01E4116A  app+0x04106A  5820          r0 = 96
0x01E4116C  app+0x04106C  bfea7c7f      call -65800
0x01E41170  app+0x041070  c060          [r4+0] = r0
0x01E41172  app+0x041072  1056          if (r0 == 0) goto 108
0x01E41174  app+0x041074  4124          r1 = 4
0x01E41176  app+0x041076  52ee0a15      b[r0+90] = r1
0x01E4117A  app+0x04107A  40e08001      r0 = 384
0x01E4117E  app+0x04107E  bfea737f      call -65818
0x01E41182  app+0x041082  02d6          r2 = r0  #
0x01E41184  app+0x041084  4160          r1 = [r4+0]
0x01E41186  app+0x041086  d0ec1125      [r1+80] = r2
0x01E4118A  app+0x04108A  124f          if (r2 == 0) goto 94
0x01E4118C  app+0x04108C  783f          r0 = 255
0x01E4118E  app+0x04108E  4120          r1 = 0
0x01E41190  app+0x041090  80ea140d      call 6696
0x01E41194  app+0x041094  783f          r0 = 255
0x01E41196  app+0x041096  4120          r1 = 0
0x01E41198  app+0x041098  80ea450d      call 6794
0x01E4119C  app+0x04109C  4060          r0 = [r4+0]
0x01E4119E  app+0x04109E  3504          {rets, r5, r4} = [sp++]
0x01E411A0  app+0x0410A0  ceea850d      goto 1841930
0x01E411A4  app+0x0410A4  4060          r0 = [r4+0]
0x01E411A6  app+0x0410A6  50ee0b05      r0 = b[r0+91] (u)
0x01E411AA  app+0x0410AA  0044          if (r0 == 0) goto 8
0x01E411AC  app+0x0410AC  5504          {pc, r5, r4} = [sp++]
0x01E411AE  app+0x0410AE  4020          r0 = 0
0x01E411B0  app+0x0410B0  3504          {rets, r5, r4} = [sp++]
0x01E411B2  app+0x0410B2  c780          goto -256
0x01E411B4  app+0x0410B4  4021          r0 = 1
0x01E411B6  app+0x0410B6  4521          r5 = 1
0x01E411B8  app+0x0410B8  bfea7cff      call -264
0x01E411BC  app+0x0410BC  4060          r0 = [r4+0]
0x01E411BE  app+0x0410BE  804c          if (r0 != 0) goto 24
0x01E411C0  app+0x0410C0  4026          r0 = 6
0x01E411C2  app+0x0410C2  8eeade06      call 1838524
0x01E411C6  app+0x0410C6  5504          {pc, r5, r4} = [sp++]
0x01E411C8  app+0x0410C8  4021          r0 = 1
0x01E411CA  app+0x0410CA  8eeada06      call 1838516
0x01E411CE  app+0x0410CE  5017          r0 = r5.b0 (u)
0x01E411D0  app+0x0410D0  00fcec29      if (r0 > 20) goto -40
0x01E411D4  app+0x0410D4  4060          r0 = [r4+0]
0x01E411D6  app+0x0410D6  c521          r5 += 1
0x01E411D8  app+0x0410D8  50ed0805      r0 = h[r0+88] (u)
0x01E411DC  app+0x0410DC  f055          if (r0 != 0) goto -22
0x01E411DE  app+0x0410DE  5504          {pc, r5, r4} = [sp++]
0x01E411E0  app+0x0410E0  5030          r0 = 80
0x01E411E2  app+0x0410E2  0060          r0 = [r0+0]
0x01E411E4  app+0x0410E4  4120          r1 = 0
0x01E411E6  app+0x0410E6  0216          r2 = r0
0x01E411E8  app+0x0410E8  0481          goto 2
0x01E411EA  app+0x0410EA  4020          r0 = 0
0x01E411EC  app+0x0410EC  0245          if (r2 == 0) goto 10
0x01E411EE  app+0x0410EE  bfea3a7f      call -65932
0x01E411F2  app+0x0410F2  4160          r1 = [r4+0]
0x01E411F4  app+0x0410F4  54ea0010      [r1+80] = 0x0
0x01E411F8  app+0x0410F8  6159          if (r1 == 0) goto -78
0x01E411FA  app+0x0410FA  1016          r0 = r1
0x01E411FC  app+0x0410FC  bfea337f      call -65946
0x01E41200  app+0x041100  40ea0040      [r4+0] = 0x0
0x01E41204  app+0x041104  5504          {pc, r5, r4} = [sp++]
0x01E41206  app+0x041106  7b04          [--sp] = {rets, r11-r4}
0x01E41208  app+0x041108  c8fff0d80000  r8 = 55536
0x01E4120E  app+0x04110E  d0ec8020      r2 = [r8+0]
0x01E41212  app+0x041112  1b16          r11 = r1
0x01E41214  app+0x041114  0516          r5 = r0
0x01E41216  app+0x041116  2253          if (r2 == 0) goto 166
0x01E41218  app+0x041118  4120          r1 = 0
0x01E4121A  app+0x04111A  4720          r7 = 0
0x01E4121C  app+0x04111C  2016          r0 = r2
0x01E4121E  app+0x04111E  8eead10c      call 1841570
0x01E41222  app+0x041122  d0ec8030      r3 = [r8+0]
0x01E41226  app+0x041126  4ae00100      r10 = 1
0x01E4122A  app+0x04112A  60e0009c      r9 = 0x8000
0x01E4122E  app+0x04112E  1497          goto 110
0x01E41230  app+0x041130  61ffff703300  if ((r7 & 0xFF) != 0) goto 102
0x01E41236  app+0x041136  c8e1a006      r0 = r10 << r6
0x01E4123A  app+0x04113A  64e80410      [r1+4] |= r0
0x01E4123E  app+0x04113E  05f81c02      if (r5 == 1) goto 56
0x01E41242  app+0x041142  85f82304      if (r5 != 2) goto 70
0x01E41246  app+0x041146  e4f10a60      r4 = r6 * 0xA  #
0x01E4124A  app+0x04114A  1160          r1 = [r1+0]
0x01E4124C  app+0x04114C  121d          r2 = r1 + r4
0x01E4124E  app+0x04114E  50ed23b0      h[r2+2] = r11
0x01E41252  app+0x041152  50ed21b0      h[r2+0] = r11
0x01E41256  app+0x041156  4220          r2 = 0
0x01E41258  app+0x041158  8bf80c00      if (r11 != 0) goto 24
0x01E4125C  app+0x04115C  0017          r0 = r0.b0 (u)
0x01E4125E  app+0x04115E  80ea160d      call 6700
0x01E41262  app+0x041162  d0ec8030      r3 = [r8+0]
0x01E41266  app+0x041166  d0ec3015      r1 = [r3+80]
0x01E4126A  app+0x04116A  e0e13200      r0 = r0 * 0x32
0x01E4126E  app+0x04116E  f4e10109      r0 = r0 / r9 (s)
0x01E41272  app+0x041172  0a81          r2 = r0 + 1
0x01E41274  app+0x041174  101d          r0 = r1 + r4
0x01E41276  app+0x041176  8a48          b[r0+8] = r2
0x01E41278  app+0x041178  048a          goto 20
0x01E4127A  app+0x04117A  e0f10a60      r0 = r6 * 0xA  #
0x01E4127E  app+0x04117E  1160          r1 = [r1+0]
0x01E41280  app+0x041180  1018          r0 += r1
0x01E41282  app+0x041182  52ee05b0      b[r0+5] = r11
0x01E41286  app+0x041186  52ee04b0      b[r0+4] = r11
0x01E4128A  app+0x04118A  0481          goto 2
0x01E4128C  app+0x04118C  1160          r1 = [r1+0]
0x01E4128E  app+0x04118E  e0e10a60      r0 = r6 * 0xA
0x01E41292  app+0x041192  1018          r0 += r1
0x01E41294  app+0x041194  52ee07a0      b[r0+7] = r10
0x01E41298  app+0x041198  52ee06a0      b[r0+6] = r10
0x01E4129C  app+0x04119C  c721          r7 += 1
0x01E4129E  app+0x04119E  01e15030      r1 = r3 + 80
0x01E412A2  app+0x0411A2  76d7          r6 = r7.b0 (u)  #
0x01E412A4  app+0x0411A4  184a          r0 = b[r1+10] (u)
0x01E412A6  app+0x0411A6  80e9c361      if (r6 < r0) goto -122
0x01E412AA  app+0x0411AA  80ea0f0d      call 6686
0x01E412AE  app+0x0411AE  0044          if (r0 == 0) goto 8
0x01E412B0  app+0x0411B0  bfeaebfe      call -554
0x01E412B4  app+0x0411B4  d0ec8030      r3 = [r8+0]
0x01E412B8  app+0x0411B8  3016          r0 = r3
0x01E412BA  app+0x0411BA  8eeabf0c      call 1841534
0x01E412BE  app+0x0411BE  5b04          {pc, r11-r4} = [sp++]
0x01E412C0  app+0x0411C0  7e04          [--sp] = {rets, r14-r4}
0x01E412C2  app+0x0411C2  4320          r3 = 0
0x01E412C4  app+0x0411C4  cafff0d80000  r10 = 55536
0x01E412CA  app+0x0411CA  c8ffbc340100  r8 = 79036
0x01E412D0  app+0x0411D0  4421          r4 = 1
0x01E412D2  app+0x0411D2  048e          goto 28
0x01E412D4  app+0x0411D4  d0ec2425      r2 = [r2+84]
0x01E412D8  app+0x0411D8  c8e14001      r0 = r4 << r1
0x01E412DC  app+0x0411DC  02fa0700      if ((r2 & r0) == 0) goto 14
0x01E412E0  app+0x0411E0  0117          r1 = r0.b0 (u)
0x01E412E2  app+0x0411E2  8016          r0 = r8
0x01E412E4  app+0x0411E4  80eac313      call 10118
0x01E412E8  app+0x0411E8  1016          r0 = r1
0x01E412EA  app+0x0411EA  80ead00c      call 6560
0x01E412EE  app+0x0411EE  c321          r3 += 1
0x01E412F0  app+0x0411F0  d0eca020      r2 = [r10+0]
0x01E412F4  app+0x0411F4  50ee2a05      r0 = b[r2+90] (u)
0x01E412F8  app+0x0411F8  3117          r1 = r3.b0 (u)
0x01E412FA  app+0x0411FA  80e9eb11      if (r1 < r0) goto -42
0x01E412FE  app+0x0411FE  4420          r4 = 0
0x01E41300  app+0x041200  4721          r7 = 1
0x01E41302  app+0x041202  c414          r12 = 0
0x01E41304  app+0x041204  1488          goto 80
0x01E41306  app+0x041206  d0ec2435      r3 = [r2+84]
0x01E4130A  app+0x04120A  c8e17051      r5 = r7 << r1
0x01E4130E  app+0x04120E  53fa2100      if ((r3 & r5) == 0) goto 66
0x01E41312  app+0x041212  d0ec2035      r3 = [r2+80]
0x01E41316  app+0x041216  e6e10a10      r6 = r1 * 0xA
0x01E4131A  app+0x04121A  b11d          r1 = r3 + r6
0x01E4131C  app+0x04121C  58ee1830      r3 = b[++r1=8] (u)
0x01E41320  app+0x041220  0359          if (r3 == 0) goto 50
0x01E41322  app+0x041222  30e1ff3f      r0 = r3 + -1
0x01E41326  app+0x041226  53d7          r3 = r5.b0 (u)  #
0x01E41328  app+0x041228  9840          b[r1+0] = r0
0x01E4132A  app+0x04122A  3016          r0 = r3
0x01E4132C  app+0x04122C  80eaaf0c      call 6494
0x01E41330  app+0x041230  d0eca020      r2 = [r10+0]
0x01E41334  app+0x041234  d0ec2015      r1 = [r2+80]
0x01E41338  app+0x041238  6118          r1 += r6
0x01E4133A  app+0x04123A  80d7          r0 = r0.l (u)  #
0x01E4133C  app+0x04123C  1961          r1 = h[r1+2] (u)
0x01E4133E  app+0x04123E  01e80500      if (r0 == r1) goto 10
0x01E41342  app+0x041242  3016          r0 = r3
0x01E41344  app+0x041244  80ea6f0c      call 6366
0x01E41348  app+0x041248  d0eca020      r2 = [r10+0]
0x01E4134C  app+0x04124C  50ee2a05      r0 = b[r2+90] (u)
0x01E41350  app+0x041250  90e1c0c5      r12 = r12 | r5
0x01E41354  app+0x041254  c421          r4 += 1
0x01E41356  app+0x041256  4117          r1 = r4.b0 (u)
0x01E41358  app+0x041258  0317          r3 = r0.b0 (u)
0x01E4135A  app+0x04125A  83e9d411      if (r1 < r3) goto -88
0x01E4135E  app+0x04125E  4420          r4 = 0
0x01E41360  app+0x041260  49e00100      r9 = 1
0x01E41364  app+0x041264  c314          r11 = 0
0x01E41366  app+0x041266  149c          goto 120
0x01E41368  app+0x041268  c8e19056      r5 = r9 << r6
0x01E4136C  app+0x04126C  c5fb3500      if ((r5 & r12) != 0) goto 106
0x01E41370  app+0x041270  d0ec2405      r0 = [r2+84]
0x01E41374  app+0x041274  94e10206      r0 = r0 & (1 << r6)
0x01E41378  app+0x041278  1050          if (r0 == 0) goto 96
0x01E4137A  app+0x04127A  bee12050      r14 = uextra(r5, p:0, l:8)
0x01E4137E  app+0x04127E  8016          r0 = r8
0x01E41380  app+0x041280  e116          r1 = r14
0x01E41382  app+0x041282  80ea7413      call 9960
0x01E41386  app+0x041286  d0eca020      r2 = [r10+0]
0x01E4138A  app+0x04128A  d0ec2015      r1 = [r2+80]
0x01E4138E  app+0x04128E  e6e10a60      r6 = r6 * 0xA
0x01E41392  app+0x041292  931d          r3 = r1 + r6
0x01E41394  app+0x041294  81d7          r1 = r0.l (u)  #
0x01E41396  app+0x041296  3f45          r7 = b[r3+5] (u)
0x01E41398  app+0x041298  07e81f10      if (r1 == r7) goto 62
0x01E4139C  app+0x04129C  3a47          r2 = b[r3+7] (u)
0x01E4139E  app+0x04129E  0255          if (r2 == 0) goto 42
0x01E413A0  app+0x0412A0  4de0ffff      r13 = -1
0x01E413A4  app+0x0412A4  8141          if (r1 != 0) goto 2
0x01E413A6  app+0x0412A6  1d16          r13 = r1
0x01E413A8  app+0x0412A8  4221          r2 = 1
0x01E413AA  app+0x0412AA  97ec0001      if (r7 <= r1) {
0x01E413AE  app+0x0412AE  d216          r2 = r13
0x01E413B0  app+0x0412B0  271c          r7 = r2 + r0
0x01E413B2  app+0x0412B2  e016          r0 = r14
0x01E413B4  app+0x0412B4  80ea6b0c      call 6358
0x01E413B8  app+0x0412B8  8048          if (r0 != 0) goto 16
0x01E413BA  app+0x0412BA  d0eca000      r0 = [r10+0]
0x01E413BE  app+0x0412BE  d0ec0005      r0 = [r0+80]
0x01E413C2  app+0x0412C2  6018          r0 += r6
0x01E413C4  app+0x0412C4  0961          r1 = h[r0+2] (u)
0x01E413C6  app+0x0412C6  8141          if (r1 != 0) goto 2
0x01E413C8  app+0x0412C8  0f45          r7 = b[r0+5] (u)
0x01E413CA  app+0x0412CA  7117          r1 = r7.b0 (u)
0x01E413CC  app+0x0412CC  e016          r0 = r14
0x01E413CE  app+0x0412CE  80eaf50b      call 6122
0x01E413D2  app+0x0412D2  d0eca020      r2 = [r10+0]
0x01E413D6  app+0x0412D6  90e1b0b5      r11 = r11 | r5
0x01E413DA  app+0x0412DA  50ee2a05      r0 = b[r2+90] (u)
0x01E413DE  app+0x0412DE  c421          r4 += 1
0x01E413E0  app+0x0412E0  4617          r6 = r4.b0 (u)
0x01E413E2  app+0x0412E2  0117          r1 = r0.b0 (u)
0x01E413E4  app+0x0412E4  81e9c061      if (r6 < r1) goto -128
0x01E413E8  app+0x0412E8  4420          r4 = 0
0x01E413EA  app+0x0412EA  4521          r5 = 1
0x01E413EC  app+0x0412EC  1482          goto 68
0x01E413EE  app+0x0412EE  c8e15067      r6 = r5 << r7
0x01E413F2  app+0x0412F2  b6fb1b00      if ((r6 & r11) != 0) goto 54
0x01E413F6  app+0x0412F6  d0ec2405      r0 = [r2+84]
0x01E413FA  app+0x0412FA  94e10207      r0 = r0 & (1 << r7)
0x01E413FE  app+0x0412FE  0056          if (r0 == 0) goto 44
0x01E41400  app+0x041300  6317          r3 = r6.b0 (u)
0x01E41402  app+0x041302  3016          r0 = r3
0x01E41404  app+0x041304  80ea430c      call 6278
0x01E41408  app+0x041308  d0eca020      r2 = [r10+0]
0x01E4140C  app+0x04130C  d0ec2015      r1 = [r2+80]
0x01E41410  app+0x041310  e7e10a70      r7 = r7 * 0xA
0x01E41414  app+0x041314  7118          r1 += r7
0x01E41416  app+0x041316  80d7          r0 = r0.l (u)  #
0x01E41418  app+0x041318  1961          r1 = h[r1+2] (u)
0x01E4141A  app+0x04131A  01e80700      if (r0 == r1) goto 14
0x01E4141E  app+0x04131E  3016          r0 = r3
0x01E41420  app+0x041320  80ea010c      call 6146
0x01E41424  app+0x041324  d0eca020      r2 = [r10+0]
0x01E41428  app+0x041328  90e1c0c6      r12 = r12 | r6
0x01E4142C  app+0x04132C  50ee2a05      r0 = b[r2+90] (u)
0x01E41430  app+0x041330  c421          r4 += 1
0x01E41432  app+0x041332  4717          r7 = r4.b0 (u)
0x01E41434  app+0x041334  0017          r0 = r0.b0 (u)
0x01E41436  app+0x041336  80e9da71      if (r7 < r0) goto -76
0x01E4143A  app+0x04133A  90e1c00b      r0 = r12 | r11
0x01E4143E  app+0x04133E  8056          if (r0 != 0) goto 44
0x01E41440  app+0x041340  50ee2c05      r0 = b[r2+92] (u)
0x01E41444  app+0x041344  80f80802      if (r0 != 1) goto 16
0x01E41448  app+0x041348  4021          r0 = 1
0x01E4144A  app+0x04134A  80ea340c      call 6248
0x01E4144E  app+0x04134E  d0eca020      r2 = [r10+0]
0x01E41452  app+0x041352  4020          r0 = 0
0x01E41454  app+0x041354  52ee2c05      b[r2+92] = r0
0x01E41458  app+0x041358  50ed2805      r0 = h[r2+88] (u)
0x01E4145C  app+0x04135C  0047          if (r0 == 0) goto 14
0x01E4145E  app+0x04135E  8eea7b11      call 1843958
0x01E41462  app+0x041362  d0eca000      r0 = [r10+0]
0x01E41466  app+0x041366  4120          r1 = 0
0x01E41468  app+0x041368  50ed0915      h[r0+88] = r1
0x01E4146C  app+0x04136C  5e04          {pc, r14-r4} = [sp++]
0x01E4146E  app+0x04136E  7404          [--sp] = {rets, r4}
0x01E41470  app+0x041370  c4fff4d80000  r4 = 55540
0x01E41476  app+0x041376  4860          r0 = h[r4+0] (u)
0x01E41478  app+0x041378  8048          if (r0 != 0) goto 16
0x01E4147A  app+0x04137A  c1ffd214e401  r1 = 31724754
0x01E41480  app+0x041380  4020          r0 = 0
0x01E41482  app+0x041382  5a24          r2 = 100
0x01E41484  app+0x041384  bfeaaf1a      call -117410
0x01E41488  app+0x041388  c860          h[r4+0] = r0
0x01E4148A  app+0x04138A  5404          {pc, r4} = [sp++]
0x01E4148C  app+0x04138C  7404          [--sp] = {rets, r4}
0x01E4148E  app+0x04138E  0416          r4 = r0
0x01E41490  app+0x041390  00e12c40      r0 = r4 + 44
0x01E41494  app+0x041394  bfea54d5      call -21848
0x01E41498  app+0x041398  8eeacc00      call 1835416
0x01E4149C  app+0x04139C  4868          r0 = h[r4+16] (u)
0x01E4149E  app+0x04139E  0044          if (r0 == 0) goto 8
0x01E414A0  app+0x0413A0  8eea5a11      call 1843892
0x01E414A4  app+0x0413A4  4020          r0 = 0
0x01E414A6  app+0x0413A6  c868          h[r4+16] = r0
0x01E414A8  app+0x0413A8  8eead200      call 1835428
0x01E414AC  app+0x0413AC  4023          r0 = 3
0x01E414AE  app+0x0413AE  c848          b[r4+8] = r0
0x01E414B0  app+0x0413B0  c0fff8d80000  r0 = 55544
0x01E414B6  app+0x0413B6  40ea0000      [r0+0] = 0x0
0x01E414BA  app+0x0413BA  5404          {pc, r4} = [sp++]
0x01E414BC  app+0x0413BC  7404          [--sp] = {rets, r4}
0x01E414BE  app+0x0413BE  c4fff4d80000  r4 = 55540
0x01E414C4  app+0x0413C4  4860          r0 = h[r4+0] (u)
0x01E414C6  app+0x0413C6  0044          if (r0 == 0) goto 8
0x01E414C8  app+0x0413C8  bfea8c1a      call -117480
0x01E414CC  app+0x0413CC  4020          r0 = 0
0x01E414CE  app+0x0413CE  c860          h[r4+0] = r0
0x01E414D0  app+0x0413D0  5404          {pc, r4} = [sp++]
0x01E414D2  app+0x0413D2  7504          [--sp] = {rets, r5, r4}
0x01E414D4  app+0x0413D4  e29f          sp += -4
0x01E414D6  app+0x0413D6  beea97b8      call -167634
0x01E414DA  app+0x0413DA  80fd0f04      ifs (r0 < 2) goto 30
0x01E414DE  app+0x0413DE  34e1ff0f      r4 = r0 + -1
0x01E414E2  app+0x0413E2  4520          r5 = 0
0x01E414E4  app+0x0413E4  0489          goto 18
0x01E414E6  app+0x0413E6  8880          r0 = sp + 0
0x01E414E8  app+0x0413E8  beea9cb8      call -167624
0x01E414EC  app+0x0413EC  80fd0602      ifs (r0 < 1) goto 12
0x01E414F0  app+0x0413F0  0020          r0 = [sp]
0x01E414F2  app+0x0413F2  beeaa7b8      call -167602
0x01E414F6  app+0x0413F6  c521          r5 += 1
0x01E414F8  app+0x0413F8  84edf551      ifs (r5 < r4) goto -22
0x01E414FC  app+0x0413FC  0281          sp += 4
0x01E414FE  app+0x0413FE  5504          {pc, r5, r4} = [sp++]
