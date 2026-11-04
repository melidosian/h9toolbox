; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 1440; undecoded chunks: 171
0x01E00100  app+0x000000  81ea156c      call 186410
0x01E00104  app+0x000004  eeff90e20000  sp = 58000
0x01E0010A  app+0x00000A  edff90e20000  ssp = 58000
0x01E00110  app+0x000010  d8e80700      [--sp] = {r2-r0}
0x01E00114  app+0x000014  c0ff043ded01  r0 = 32324868
0x01E0011A  app+0x00001A  c1ff00008000  r1 = 8388608
0x01E00120  app+0x000020  c2ff00000000  r2 = 0
0x01E00126  app+0x000026  a2a2          r2 = r2 >> 2
0x01E00128  app+0x000028  1203          rep 4 r2 {
0x01E0012A  app+0x00002A  0305          r3 = [r0++=4]
0x01E0012C  app+0x00002C  9305          [r1++=4] = r3
0x01E0012E  app+0x00002E  02fcfb01      if (r2 > 0) goto -10
0x01E00132  app+0x000032  d4e80700      {r2-r0} = [sp++]
0x01E00136  app+0x000036  c3ffe0e20000  r3 = 58080
0x01E0013C  app+0x00003C  4120          r1 = 0
0x01E0013E  app+0x00003E  c2fff4010100  r2 = 66036
0x01E00144  app+0x000044  a2a2          r2 = r2 >> 2
0x01E00146  app+0x000046  0203          rep 2 r2 {
0x01E00148  app+0x000048  b105          [r3++=4] = r1
0x01E0014A  app+0x00004A  f25d          if (r2 != 0) goto -6
0x01E0014C  app+0x00004C  c3ff00d20200  r3 = 184832
0x01E00152  app+0x000052  4120          r1 = 0
0x01E00154  app+0x000054  c2ff00000000  r2 = 0
0x01E0015A  app+0x00005A  a2a2          r2 = r2 >> 2
0x01E0015C  app+0x00005C  0203          rep 2 r2 {
0x01E0015E  app+0x00005E  b105          [r3++=4] = r1
0x01E00160  app+0x000060  f25d          if (r2 != 0) goto -6
0x01E00162  app+0x000062  c4ff00000000  r4 = 0
0x01E00168  app+0x000068  c1ff8c62ec01  r1 = 32268940
0x01E0016E  app+0x00006E  c2ff78da0000  r2 = 55928
0x01E00174  app+0x000074  a2a2          r2 = r2 >> 2
0x01E00176  app+0x000076  1203          rep 4 r2 {
0x01E00178  app+0x000078  1305          r3 = [r1++=4]
0x01E0017A  app+0x00007A  c305          [r4++=4] = r3
0x01E0017C  app+0x00007C  f25c          if (r2 != 0) goto -8
0x01E0017E  app+0x00007E  64e0002b      r2 = icfg
0x01E00182  app+0x000082  72e1402f      r2 = r2 & 0xFFFFFCFF
0x01E00186  app+0x000086  64e0802b      icfg = r2
0x01E0018A  app+0x00008A  84ea1760      call 573486
0x01E0018E  app+0x00008E  82ea01f2      call 386050
0x01E00192  app+0x000092  c0ff7cfbe501  r0 = 31849340
0x01E00198  app+0x000098  d000          goto r0
0x01E0019A  app+0x00009A  0000          nop
0x01E0019C  app+0x00009C  f83b          r0 += -5
0x01E0019E  app+0x00009E  4221          r2 = 1
0x01E001A0  app+0x0000A0  021a          r2 <<= r0
0x01E001A2  app+0x0000A2  c0ff00511e00  r0 = 1986816
0x01E001A8  app+0x0000A8  31e80060      if (r1 == 0) {
0x01E001AC  app+0x0000AC  0160          r1 = [r0+0]
0x01E001AE  app+0x0000AE  90e11312      r1 = r1 & ~r2
0x01E001B2  app+0x0000B2  0160          r1 = [r0+0]
0x01E001B4  app+0x0000B4  2119          r1 |= r2
0x01E001B6  app+0x0000B6  8160          [r0+0] = r1
0x01E001B8  app+0x0000B8  8000          rts
0x01E001BA  app+0x0000BA  0116          r1 = r0
0x01E001BC  app+0x0000BC  4020          r0 = 0
0x01E001BE  app+0x0000BE  b1ec38c0      if (r1 <= 56) {
0x01E001C2  app+0x0000C2  90a4          r0 = r1 >> 4
0x01E001C4  app+0x0000C4  00a2          r0 = r0 << 2
0x01E001C6  app+0x0000C6  c1ff5406e001  r1 = 31458900
0x01E001CC  app+0x0000CC  d8ec0201      r0 = [r0+r1]
0x01E001D0  app+0x0000D0  8000          rts
0x01E001D2  app+0x0000D2  7704          [--sp] = {rets, r7-r4}
0x01E001D4  app+0x0000D4  1316          r3 = r1
0x01E001D6  app+0x0000D6  0416          r4 = r0
0x01E001D8  app+0x0000D8  84fc0182      if (r4 <= 65) goto 2
0x01E001DC  app+0x0000DC  5704          {pc, r7-r4} = [sp++]
0x01E001DE  app+0x0000DE  84f90670      if (r4 < 56) goto 12
0x01E001E2  app+0x0000E2  84f9fb7b      if (r4 < 61) goto -10
0x01E001E6  app+0x0000E6  30e1c84f      r0 = r4 + -56
0x01E001EA  app+0x0000EA  3704          {rets, r7-r4} = [sp++]
0x01E001EC  app+0x0000EC  e797          goto -82
0x01E001EE  app+0x0000EE  7185          call -54
0x01E001F0  app+0x0000F0  0516          r5 = r0
0x01E001F2  app+0x0000F2  7554          if (r5 == 0) goto -24
0x01E001F4  app+0x0000F4  c6ff00021e00  r6 = 1966592
0x01E001FA  app+0x0000FA  61f10f40      r1 = r4 & 0xF  #
0x01E001FE  app+0x0000FE  6060          r0 = [r6+0]
0x01E00200  app+0x000100  4221          r2 = 1
0x01E00202  app+0x000102  c8e12071      r7 = r2 << r1
0x01E00206  app+0x000106  84f80d4c      if (r4 != 38) goto 26
0x01E0020A  app+0x00010A  60e10f00      r0 = r0 & 0xF
0x01E0020E  app+0x00010E  00fc0904      if (r0 > 2) goto 18
0x01E00212  app+0x000112  5060          r0 = [r5+0]
0x01E00214  app+0x000114  f019          r0 &= r7
0x01E00216  app+0x000116  3019          r0 |= r3
0x01E00218  app+0x000118  0043          if (r0 == 0) goto 6
0x01E0021A  app+0x00011A  4021          r0 = 1
0x01E0021C  app+0x00011C  82eab225      call 281444
0x01E00220  app+0x000120  64e80b57      [r5+8] &= ~r7
0x01E00224  app+0x000124  0345          if (r3 == 0) goto 10
0x01E00226  app+0x000126  33e80100      if (r3 == 1) {
0x01E0022A  app+0x00012A  64e80057      [r5+0] |= r7
0x01E0022E  app+0x00012E  0482          goto 4
0x01E00230  app+0x000130  64e80357      [r5+0] &= ~r7
0x01E00234  app+0x000134  4021          r0 = 1
0x01E00236  app+0x000136  4121          r1 = 1
0x01E00238  app+0x000138  34e82600      if (r4 == 38) {
0x01E0023C  app+0x00013C  4120          r1 = 0
0x01E0023E  app+0x00013E  6260          r2 = [r6+0]
0x01E00240  app+0x000140  62e10f20      r2 = r2 & 0xF
0x01E00244  app+0x000144  b2ec0200      if (r2 <= 2) {
0x01E00248  app+0x000148  4020          r0 = 0
0x01E0024A  app+0x00014A  e348          if (r3 != 0) goto -112
0x01E0024C  app+0x00014C  1019          r0 |= r1
0x01E0024E  app+0x00014E  e046          if (r0 != 0) goto -116
0x01E00250  app+0x000150  4020          r0 = 0
0x01E00252  app+0x000152  82ea9725      call 281390
0x01E00256  app+0x000156  5704          {pc, r7-r4} = [sp++]
0x01E00258  app+0x000158  c024          r0 += 4
0x01E0025A  app+0x00015A  4221          r2 = 1
0x01E0025C  app+0x00015C  021a          r2 <<= r0
0x01E0025E  app+0x00015E  c0ff00511e00  r0 = 1986816
0x01E00264  app+0x000164  31e80060      if (r1 == 0) {
0x01E00268  app+0x000168  0160          r1 = [r0+0]
0x01E0026A  app+0x00016A  90e11312      r1 = r1 & ~r2
0x01E0026E  app+0x00016E  0160          r1 = [r0+0]
0x01E00270  app+0x000170  2119          r1 |= r2
0x01E00272  app+0x000172  8160          [r0+0] = r1
0x01E00274  app+0x000174  8000          rts
0x01E00276  app+0x000176  7504          [--sp] = {rets, r5, r4}
0x01E00278  app+0x000178  1216          r2 = r1
0x01E0027A  app+0x00017A  0316          r3 = r0
0x01E0027C  app+0x00017C  03fc2b82      if (r3 > 65) goto 86
0x01E00280  app+0x000180  83f90670      if (r3 < 56) goto 12
0x01E00284  app+0x000184  83f9277a      if (r3 < 61) goto 78
0x01E00288  app+0x000188  30e1c83f      r0 = r3 + -56
0x01E0028C  app+0x00018C  3504          {rets, r5, r4} = [sp++]
0x01E0028E  app+0x00018E  f784          goto -56
0x01E00290  app+0x000190  4194          call -216
0x01E00292  app+0x000192  1041          if (r0 == 0) goto 66
0x01E00294  app+0x000194  c1ff00021e00  r1 = 1966592
0x01E0029A  app+0x00019A  61f10f30      r1 = r3 & 0xF  #
0x01E0029E  app+0x00019E  1460          r4 = [r1+0]
0x01E002A0  app+0x0001A0  4521          r5 = 1
0x01E002A2  app+0x0001A2  c8e15011      r1 = r5 << r1
0x01E002A6  app+0x0001A6  83f8104c      if (r3 != 38) goto 32
0x01E002AA  app+0x0001AA  63e10f40      r3 = r4 & 0xF
0x01E002AE  app+0x0001AE  03fc0c04      if (r3 > 2) goto 24
0x01E002B2  app+0x0001B2  64e81301      [r0+16] &= ~r1
0x01E002B6  app+0x0001B6  64e80301      [r0+0] &= ~r1
0x01E002BA  app+0x0001BA  64e80b01      [r0+8] &= ~r1
0x01E002BE  app+0x0001BE  64e80c01      [r0+12] |= r1
0x01E002C2  app+0x0001C2  4020          r0 = 0
0x01E002C4  app+0x0001C4  3504          {rets, r5, r4} = [sp++]
0x01E002C6  app+0x0001C6  c2ea5d25      goto 281274
0x01E002CA  app+0x0001CA  0243          if (r2 == 0) goto 6
0x01E002CC  app+0x0001CC  64e80c01      [r0+12] |= r1
0x01E002D0  app+0x0001D0  5504          {pc, r5, r4} = [sp++]
0x01E002D2  app+0x0001D2  64e80f01      [r0+12] &= ~r1
0x01E002D6  app+0x0001D6  5504          {pc, r5, r4} = [sp++]
0x01E002D8  app+0x0001D8  f83d          r0 += -3
0x01E002DA  app+0x0001DA  4221          r2 = 1
0x01E002DC  app+0x0001DC  021a          r2 <<= r0
0x01E002DE  app+0x0001DE  c0ff00511e00  r0 = 1986816
0x01E002E4  app+0x0001E4  31e80060      if (r1 == 0) {
0x01E002E8  app+0x0001E8  0160          r1 = [r0+0]
0x01E002EA  app+0x0001EA  90e11312      r1 = r1 & ~r2
0x01E002EE  app+0x0001EE  0160          r1 = [r0+0]
0x01E002F0  app+0x0001F0  2119          r1 |= r2
0x01E002F2  app+0x0001F2  8160          [r0+0] = r1
0x01E002F4  app+0x0001F4  8000          rts
0x01E002F6  app+0x0001F6  7504          [--sp] = {rets, r5, r4}
0x01E002F8  app+0x0001F8  1216          r2 = r1
0x01E002FA  app+0x0001FA  0416          r4 = r0
0x01E002FC  app+0x0001FC  84fc0182      if (r4 <= 65) goto 2
0x01E00300  app+0x000200  5504          {pc, r5, r4} = [sp++]
0x01E00302  app+0x000202  84f90f70      if (r4 < 56) goto 30
0x01E00306  app+0x000206  84f9fb7b      if (r4 < 61) goto -10
0x01E0030A  app+0x00020A  c0ff00181e00  r0 = 1972224
0x01E00310  app+0x000210  40ea0000      [r0+0] = 0x0
0x01E00314  app+0x000214  e0e0640d      r0 = r0 + 0x3900
0x01E00318  app+0x000218  00ef100e      [r0+0] |= 0x900
0x01E0031C  app+0x00021C  30e1c84f      r0 = r4 + -56
0x01E00320  app+0x000220  3504          {rets, r5, r4} = [sp++]
0x01E00322  app+0x000222  e79a          goto -76
0x01E00324  app+0x000224  bfea49ff      call -366
0x01E00328  app+0x000228  0316          r3 = r0
0x01E0032A  app+0x00022A  734a          if (r3 == 0) goto -44
0x01E0032C  app+0x00022C  c0ff00021e00  r0 = 1966592
0x01E00332  app+0x000232  61f10f40      r1 = r4 & 0xF  #
0x01E00336  app+0x000236  0060          r0 = [r0+0]
0x01E00338  app+0x000238  4521          r5 = 1
0x01E0033A  app+0x00023A  151a          r5 <<= r1
0x01E0033C  app+0x00023C  84f80c4c      if (r4 != 38) goto 24
0x01E00340  app+0x000240  60e10f00      r0 = r0 & 0xF
0x01E00344  app+0x000244  00fc0804      if (r0 > 2) goto 16
0x01E00348  app+0x000248  3060          r0 = [r3+0]
0x01E0034A  app+0x00024A  10ea0015      if ((r0 & r5) == 0) {
0x01E0034E  app+0x00024E  4020          r0 = 0
0x01E00350  app+0x000250  4021          r0 = 1
0x01E00352  app+0x000252  82ea1725      call 281134
0x01E00356  app+0x000256  0484          goto 8
0x01E00358  app+0x000258  0243          if (r2 == 0) goto 6
0x01E0035A  app+0x00025A  64e80835      [r3+8] |= r5
0x01E0035E  app+0x00025E  5504          {pc, r5, r4} = [sp++]
0x01E00360  app+0x000260  64e80b35      [r3+8] &= ~r5
0x01E00364  app+0x000264  5504          {pc, r5, r4} = [sp++]
0x01E00366  app+0x000266  c021          r0 += 1
0x01E00368  app+0x000268  4221          r2 = 1
0x01E0036A  app+0x00026A  021a          r2 <<= r0
0x01E0036C  app+0x00026C  c0ff00511e00  r0 = 1986816
0x01E00372  app+0x000272  31e80060      if (r1 == 0) {
0x01E00376  app+0x000276  0160          r1 = [r0+0]
0x01E00378  app+0x000278  90e11312      r1 = r1 & ~r2
0x01E0037C  app+0x00027C  0160          r1 = [r0+0]
0x01E0037E  app+0x00027E  2119          r1 |= r2
0x01E00380  app+0x000280  8160          [r0+0] = r1
0x01E00382  app+0x000282  8000          rts
0x01E00384  app+0x000284  1004          [--sp] = rets
0x01E00386  app+0x000286  1216          r2 = r1
0x01E00388  app+0x000288  0316          r3 = r0
0x01E0038A  app+0x00028A  03fc1782      if (r3 > 65) goto 46
0x01E0038E  app+0x00028E  83f90770      if (r3 < 56) goto 14
0x01E00392  app+0x000292  33e93d80      if (r3 >= 61) {
0x01E00396  app+0x000296  30e1c83f      r0 = r3 + -56
0x01E0039A  app+0x00029A  8804          {rets} = [sp++]
0x01E0039C  app+0x00029C  f784          goto -56
0x01E0039E  app+0x00029E  0004          pc = [sp++]
0x01E003A0  app+0x0002A0  bfea0bff      call -490
0x01E003A4  app+0x0002A4  004b          if (r0 == 0) goto 22
0x01E003A6  app+0x0002A6  61e10f30      r1 = r3 & 0xF
0x01E003AA  app+0x0002AA  4321          r3 = 1
0x01E003AC  app+0x0002AC  c8e13011      r1 = r3 << r1
0x01E003B0  app+0x0002B0  0243          if (r2 == 0) goto 6
0x01E003B2  app+0x0002B2  64e81001      [r0+16] |= r1
0x01E003B6  app+0x0002B6  0004          pc = [sp++]
0x01E003B8  app+0x0002B8  64e81301      [r0+16] &= ~r1
0x01E003BC  app+0x0002BC  0004          pc = [sp++]
0x01E003BE  app+0x0002BE  f83f          r0 += -1
0x01E003C0  app+0x0002C0  4221          r2 = 1
0x01E003C2  app+0x0002C2  021a          r2 <<= r0
0x01E003C4  app+0x0002C4  c0ff00511e00  r0 = 1986816
0x01E003CA  app+0x0002CA  31e80060      if (r1 == 0) {
0x01E003CE  app+0x0002CE  0160          r1 = [r0+0]
0x01E003D0  app+0x0002D0  90e11312      r1 = r1 & ~r2
0x01E003D4  app+0x0002D4  0160          r1 = [r0+0]
0x01E003D6  app+0x0002D6  2119          r1 |= r2
0x01E003D8  app+0x0002D8  8160          [r0+0] = r1
0x01E003DA  app+0x0002DA  8000          rts
0x01E003DC  app+0x0002DC  1004          [--sp] = rets
0x01E003DE  app+0x0002DE  1216          r2 = r1
0x01E003E0  app+0x0002E0  0316          r3 = r0
0x01E003E2  app+0x0002E2  03fc1782      if (r3 > 65) goto 46
0x01E003E6  app+0x0002E6  83f90770      if (r3 < 56) goto 14
0x01E003EA  app+0x0002EA  33e93d80      if (r3 >= 61) {
0x01E003EE  app+0x0002EE  30e1c83f      r0 = r3 + -56
0x01E003F2  app+0x0002F2  8804          {rets} = [sp++]
0x01E003F4  app+0x0002F4  f784          goto -56
0x01E003F6  app+0x0002F6  0004          pc = [sp++]
0x01E003F8  app+0x0002F8  bfeadffe      call -578
0x01E003FC  app+0x0002FC  004b          if (r0 == 0) goto 22
0x01E003FE  app+0x0002FE  61e10f30      r1 = r3 & 0xF
0x01E00402  app+0x000302  4321          r3 = 1
0x01E00404  app+0x000304  c8e13011      r1 = r3 << r1
0x01E00408  app+0x000308  0243          if (r2 == 0) goto 6
0x01E0040A  app+0x00030A  64e81401      [r0+20] |= r1
0x01E0040E  app+0x00030E  0004          pc = [sp++]
0x01E00410  app+0x000310  64e81701      [r0+20] &= ~r1
0x01E00414  app+0x000314  0004          pc = [sp++]
0x01E00416  app+0x000316  7704          [--sp] = {rets, r7-r4}
0x01E00418  app+0x000318  1316          r3 = r1
0x01E0041A  app+0x00031A  0416          r4 = r0
0x01E0041C  app+0x00031C  84fc0182      if (r4 <= 65) goto 2
0x01E00420  app+0x000320  5704          {pc, r7-r4} = [sp++]
0x01E00422  app+0x000322  84f90b70      if (r4 < 56) goto 22
0x01E00426  app+0x000326  84f9fb7b      if (r4 < 61) goto -10
0x01E0042A  app+0x00032A  f428          r4 += -56
0x01E0042C  app+0x00032C  4120          r1 = 0
0x01E0042E  app+0x00032E  4016          r0 = r4
0x01E00430  app+0x000330  bfea52ff      call -348
0x01E00434  app+0x000334  4016          r0 = r4
0x01E00436  app+0x000336  3116          r1 = r3
0x01E00438  app+0x000338  3704          {rets, r7-r4} = [sp++]
0x01E0043A  app+0x00033A  5790          goto -672
0x01E0043C  app+0x00033C  bfeabdfe      call -646
0x01E00440  app+0x000340  0516          r5 = r0
0x01E00442  app+0x000342  754e          if (r5 == 0) goto -36
0x01E00444  app+0x000344  c6ff00021e00  r6 = 1966592
0x01E0044A  app+0x00034A  61f10f40      r1 = r4 & 0xF  #
0x01E0044E  app+0x00034E  6060          r0 = [r6+0]
0x01E00450  app+0x000350  4221          r2 = 1
0x01E00452  app+0x000352  c8e12071      r7 = r2 << r1
0x01E00456  app+0x000356  84f80d4c      if (r4 != 38) goto 26
0x01E0045A  app+0x00035A  60e10f00      r0 = r0 & 0xF
0x01E0045E  app+0x00035E  00fc0904      if (r0 > 2) goto 18
0x01E00462  app+0x000362  5060          r0 = [r5+0]
0x01E00464  app+0x000364  f019          r0 &= r7
0x01E00466  app+0x000366  3019          r0 |= r3
0x01E00468  app+0x000368  0043          if (r0 == 0) goto 6
0x01E0046A  app+0x00036A  4021          r0 = 1
0x01E0046C  app+0x00036C  82ea8a24      call 280852
0x01E00470  app+0x000370  64e80b57      [r5+8] &= ~r7
0x01E00474  app+0x000374  0345          if (r3 == 0) goto 10
0x01E00476  app+0x000376  33e80100      if (r3 == 1) {
0x01E0047A  app+0x00037A  64e80057      [r5+0] |= r7
0x01E0047E  app+0x00037E  0482          goto 4
0x01E00480  app+0x000380  64e80357      [r5+0] &= ~r7
0x01E00484  app+0x000384  64e80b57      [r5+8] &= ~r7
0x01E00488  app+0x000388  4021          r0 = 1
0x01E0048A  app+0x00038A  4121          r1 = 1
0x01E0048C  app+0x00038C  34e82600      if (r4 == 38) {
0x01E00490  app+0x000390  4120          r1 = 0
0x01E00492  app+0x000392  6260          r2 = [r6+0]
0x01E00494  app+0x000394  62e10f20      r2 = r2 & 0xF
0x01E00498  app+0x000398  b2ec0200      if (r2 <= 2) {
0x01E0049C  app+0x00039C  4020          r0 = 0
0x01E0049E  app+0x00039E  e340          if (r3 != 0) goto -128
0x01E004A0  app+0x0003A0  1019          r0 |= r1
0x01E004A2  app+0x0003A2  d05e          if (r0 != 0) goto -132
0x01E004A4  app+0x0003A4  4020          r0 = 0
0x01E004A6  app+0x0003A6  82ea6d24      call 280794
0x01E004AA  app+0x0003AA  5704          {pc, r7-r4} = [sp++]
0x01E004AC  app+0x0003AC  7404          [--sp] = {rets, r4}
0x01E004AE  app+0x0003AE  1316          r3 = r1
0x01E004B0  app+0x0003B0  00fc3082      if (r0 > 65) goto 96
0x01E004B4  app+0x0003B4  80f91270      if (r0 < 56) goto 36
0x01E004B8  app+0x0003B8  80f92c7a      if (r0 < 61) goto 88
0x01E004BC  app+0x0003BC  4420          r4 = 0
0x01E004BE  app+0x0003BE  048b          goto 22
0x01E004C0  app+0x0003C0  94e13214      r1 = r3 & (1 << r4)
0x01E004C4  app+0x0003C4  00e13d40      r0 = r4 + 61
0x01E004C8  app+0x0003C8  31e80010      if (r1 == 0) {
0x01E004CC  app+0x0003CC  4120          r1 = 0
0x01E004CE  app+0x0003CE  4121          r1 = 1
0x01E004D0  app+0x0003D0  bfea02ff      call -508
0x01E004D4  app+0x0003D4  c421          r4 += 1
0x01E004D6  app+0x0003D6  84f8f321      if (r4 != 16) goto -26
0x01E004DA  app+0x0003DA  5404          {pc, r4} = [sp++]
0x01E004DC  app+0x0003DC  bfea6dfe      call -806
0x01E004E0  app+0x0003E0  0416          r4 = r0
0x01E004E2  app+0x0003E2  0458          if (r4 == 0) goto 48
0x01E004E4  app+0x0003E4  4062          r0 = [r4+8]
0x01E004E6  app+0x0003E6  c1ff00021e00  r1 = 1966592
0x01E004EC  app+0x0003EC  1160          r1 = [r1+0]
0x01E004EE  app+0x0003EE  61e10f10      r1 = r1 & 0xF
0x01E004F2  app+0x0003F2  0319          r3 |= r0
0x01E004F4  app+0x0003F4  01fc0d04      if (r1 > 2) goto 26
0x01E004F8  app+0x0003F8  c0ff80501e00  r0 = 1986688
0x01E004FE  app+0x0003FE  80e80840      if (r4 != r0) goto 16
0x01E00502  app+0x000402  0060          r0 = [r0+0]
0x01E00504  app+0x000404  bb26          r3 &= 4294967231
0x01E00506  app+0x000406  20ea4010      if ((r0 & 0x40) == 0) {
0x01E0050A  app+0x00040A  4020          r0 = 0
0x01E0050C  app+0x00040C  4021          r0 = 1
0x01E0050E  app+0x00040E  82ea3924      call 280690
0x01E00512  app+0x000412  c362          [r4+8] = r3
0x01E00514  app+0x000414  5404          {pc, r4} = [sp++]
0x01E00516  app+0x000416  7404          [--sp] = {rets, r4}
0x01E00518  app+0x000418  1316          r3 = r1
0x01E0051A  app+0x00041A  00fc1982      if (r0 > 65) goto 50
0x01E0051E  app+0x00041E  80f91270      if (r0 < 56) goto 36
0x01E00522  app+0x000422  80f9157a      if (r0 < 61) goto 42
0x01E00526  app+0x000426  4420          r4 = 0
0x01E00528  app+0x000428  048b          goto 22
0x01E0052A  app+0x00042A  94e13214      r1 = r3 & (1 << r4)
0x01E0052E  app+0x00042E  00e13d40      r0 = r4 + 61
0x01E00532  app+0x000432  31e80010      if (r1 == 0) {
0x01E00536  app+0x000436  4120          r1 = 0
0x01E00538  app+0x000438  4121          r1 = 1
0x01E0053A  app+0x00043A  bfea14ff      call -472
0x01E0053E  app+0x00043E  c421          r4 += 1
0x01E00540  app+0x000440  84f8f321      if (r4 != 16) goto -26
0x01E00544  app+0x000444  5404          {pc, r4} = [sp++]
0x01E00546  app+0x000446  bfea38fe      call -912
0x01E0054A  app+0x00044A  0042          if (r0 == 0) goto 4
0x01E0054C  app+0x00044C  64e81203      [r0+16] &= r3
0x01E00550  app+0x000450  5404          {pc, r4} = [sp++]
0x01E00552  app+0x000452  7404          [--sp] = {rets, r4}
0x01E00554  app+0x000454  1316          r3 = r1
0x01E00556  app+0x000456  00fc1982      if (r0 > 65) goto 50
0x01E0055A  app+0x00045A  80f91270      if (r0 < 56) goto 36
0x01E0055E  app+0x00045E  80f9157a      if (r0 < 61) goto 42
0x01E00562  app+0x000462  4420          r4 = 0
0x01E00564  app+0x000464  048b          goto 22
0x01E00566  app+0x000466  94e13214      r1 = r3 & (1 << r4)
0x01E0056A  app+0x00046A  00e13d40      r0 = r4 + 61
0x01E0056E  app+0x00046E  31e80010      if (r1 == 0) {
0x01E00572  app+0x000472  4120          r1 = 0
0x01E00574  app+0x000474  4121          r1 = 1
0x01E00576  app+0x000476  bfea22ff      call -444
0x01E0057A  app+0x00047A  c421          r4 += 1
0x01E0057C  app+0x00047C  84f8f321      if (r4 != 16) goto -26
0x01E00580  app+0x000480  5404          {pc, r4} = [sp++]
0x01E00582  app+0x000482  bfea1afe      call -972
0x01E00586  app+0x000486  0042          if (r0 == 0) goto 4
0x01E00588  app+0x000488  64e81603      [r0+20] &= r3
0x01E0058C  app+0x00048C  5404          {pc, r4} = [sp++]
0x01E0058E  app+0x00048E  7404          [--sp] = {rets, r4}
0x01E00590  app+0x000490  1316          r3 = r1
0x01E00592  app+0x000492  00fc3282      if (r0 > 65) goto 100
0x01E00596  app+0x000496  80f91270      if (r0 < 56) goto 36
0x01E0059A  app+0x00049A  80f92e7a      if (r0 < 61) goto 92
0x01E0059E  app+0x00049E  4420          r4 = 0
0x01E005A0  app+0x0004A0  048b          goto 22
0x01E005A2  app+0x0004A2  94e13214      r1 = r3 & (1 << r4)
0x01E005A6  app+0x0004A6  00e13d40      r0 = r4 + 61
0x01E005AA  app+0x0004AA  31e80010      if (r1 == 0) {
0x01E005AE  app+0x0004AE  4120          r1 = 0
0x01E005B0  app+0x0004B0  4121          r1 = 1
0x01E005B2  app+0x0004B2  bfea51fe      call -862
0x01E005B6  app+0x0004B6  c421          r4 += 1
0x01E005B8  app+0x0004B8  84f8f321      if (r4 != 16) goto -26
0x01E005BC  app+0x0004BC  5404          {pc, r4} = [sp++]
0x01E005BE  app+0x0004BE  bfeafcfd      call -1032
0x01E005C2  app+0x0004C2  0416          r4 = r0
0x01E005C4  app+0x0004C4  045a          if (r4 == 0) goto 52
0x01E005C6  app+0x0004C6  4063          r0 = [r4+12]
0x01E005C8  app+0x0004C8  c1ff00021e00  r1 = 1966592
0x01E005CE  app+0x0004CE  1160          r1 = [r1+0]
0x01E005D0  app+0x0004D0  61e10f10      r1 = r1 & 0xF
0x01E005D4  app+0x0004D4  8319          r3 &= r0
0x01E005D6  app+0x0004D6  01fc0f04      if (r1 > 2) goto 30
0x01E005DA  app+0x0004DA  c0ff80501e00  r0 = 1986688
0x01E005E0  app+0x0004E0  80e80a40      if (r4 != r0) goto 20
0x01E005E4  app+0x0004E4  c4ef4000      [r0+16] &= 0xFFFFFFBF
0x01E005E8  app+0x0004E8  c0ef4000      [r0+0] &= 0xFFFFFFBF
0x01E005EC  app+0x0004EC  c2ef4000      [r0+8] &= 0xFFFFFFBF
0x01E005F0  app+0x0004F0  4020          r0 = 0
0x01E005F2  app+0x0004F2  82eac723      call 280462
0x01E005F6  app+0x0004F6  3326          r3 |= 64
0x01E005F8  app+0x0004F8  c363          [r4+12] = r3
0x01E005FA  app+0x0004FA  5404          {pc, r4} = [sp++]
0x01E005FC  app+0x0004FC  c028          r0 += 8
0x01E005FE  app+0x0004FE  4221          r2 = 1
0x01E00600  app+0x000500  021a          r2 <<= r0
0x01E00602  app+0x000502  c0ff00511e00  r0 = 1986816
0x01E00608  app+0x000508  31e80060      if (r1 == 0) {
0x01E0060C  app+0x00050C  0160          r1 = [r0+0]
0x01E0060E  app+0x00050E  90e11312      r1 = r1 & ~r2
0x01E00612  app+0x000512  0160          r1 = [r0+0]
0x01E00614  app+0x000514  2119          r1 |= r2
0x01E00616  app+0x000516  8160          [r0+0] = r1
0x01E00618  app+0x000518  8000          rts
0x01E0061A  app+0x00051A  7404          [--sp] = {rets, r4}
0x01E0061C  app+0x00051C  1316          r3 = r1
0x01E0061E  app+0x00051E  00fc1882      if (r0 > 65) goto 48
0x01E00622  app+0x000522  80f91170      if (r0 < 56) goto 34
0x01E00626  app+0x000526  80f9147a      if (r0 < 61) goto 40
0x01E0062A  app+0x00052A  4420          r4 = 0
0x01E0062C  app+0x00052C  048a          goto 20
0x01E0062E  app+0x00052E  94e13214      r1 = r3 & (1 << r4)
0x01E00632  app+0x000532  00e13d40      r0 = r4 + 61
0x01E00636  app+0x000536  31e80010      if (r1 == 0) {
0x01E0063A  app+0x00053A  4120          r1 = 0
0x01E0063C  app+0x00053C  4121          r1 = 1
0x01E0063E  app+0x00053E  619e          call -68
0x01E00640  app+0x000540  c421          r4 += 1
0x01E00642  app+0x000542  84f8f421      if (r4 != 16) goto -24
0x01E00646  app+0x000546  5404          {pc, r4} = [sp++]
0x01E00648  app+0x000548  bfeab7fd      call -1170
0x01E0064C  app+0x00054C  0042          if (r0 == 0) goto 4
0x01E0064E  app+0x00054E  64e82203      [r0+32] &= r3
0x01E00652  app+0x000552  5404          {pc, r4} = [sp++]
0x01E00654  app+0x000554  0050          if (r0 == 0) goto 32
0x01E00656  app+0x000556  1e00          .byte 0x1E, 0x00
0x01E00658  app+0x000558  4050          if (r0 == 0) goto -224
0x01E0065A  app+0x00055A  1e00          .byte 0x1E, 0x00
0x01E0065C  app+0x00055C  8050          if (r0 != 0) goto 32
0x01E0065E  app+0x00055E  1e00          .byte 0x1E, 0x00
0x01E00660  app+0x000560  c050          if (r0 != 0) goto -224
0x01E00662  app+0x000562  1e00          .byte 0x1E, 0x00
0x01E00664  app+0x000564  8009          r0 = [r0++=r11]
0x01E00666  app+0x000566  e001          .byte 0xE0, 0x01
0x01E00668  app+0x000568  8a06          h[r0++=-2] = r2
0x01E0066A  app+0x00056A  e001          .byte 0xE0, 0x01
0x01E0066C  app+0x00056C  6607          r6 = b[r6++=1] (u)
0x01E0066E  app+0x00056E  e001          .byte 0xE0, 0x01
0x01E00670  app+0x000570  8409          r4 = [r0++=r11]
0x01E00672  app+0x000572  e001          .byte 0xE0, 0x01
0x01E00674  app+0x000574  120b          r2 = [r1++=r14]
0x01E00676  app+0x000576  e001          .byte 0xE0, 0x01
0x01E00678  app+0x000578  5400          .byte 0x54, 0x00
0x01E0067A  app+0x00057A  1800          .byte 0x18, 0x00
0x01E0067C  app+0x00057C  4d00          .byte 0x4D, 0x00
0x01E0067E  app+0x00057E  6000          cli
0x01E00680  app+0x000580  2000          csync
0x01E00682  app+0x000582  6000          cli
0x01E00684  app+0x000584  4000          lockclr
0x01E00686  app+0x000586  2000          csync
0x01E00688  app+0x000588  4000          lockclr
0x01E0068A  app+0x00058A  7504          [--sp] = {rets, r5, r4}
0x01E0068C  app+0x00058C  0416          r4 = r0
0x01E0068E  app+0x00058E  4120          r1 = 0
0x01E00690  app+0x000590  6a2c          r2 = 172
0x01E00692  app+0x000592  4520          r5 = 0
0x01E00694  app+0x000594  98ea8e90      call 3219740
0x01E00698  app+0x000598  40e00028      r0 = 10240
0x01E0069C  app+0x00059C  01e14440      r1 = r4 + 68
0x01E006A0  app+0x0005A0  9062          [r1+8] = r0
0x01E006A2  app+0x0005A2  4221          r2 = 1
0x01E006A4  app+0x0005A4  9a44          b[r1+4] = r2
0x01E006A6  app+0x0005A6  9560          [r1+0] = r5
0x01E006A8  app+0x0005A8  9066          [r1+24] = r0
0x01E006AA  app+0x0005AA  52ee4825      b[r4+88] = r2
0x01E006AE  app+0x0005AE  4020          r0 = 0
0x01E006B0  app+0x0005B0  9564          [r1+16] = r5
0x01E006B2  app+0x0005B2  5504          {pc, r5, r4} = [sp++]
0x01E006B4  app+0x0005B4  7704          [--sp] = {rets, r7-r4}
0x01E006B6  app+0x0005B6  c1ff20ce0000  r1 = 52768
0x01E006BC  app+0x0005BC  1260          r2 = [r1+0]
0x01E006BE  app+0x0005BE  c4ff447e0100  r4 = 97860
0x01E006C4  app+0x0005C4  4620          r6 = 0
0x01E006C6  app+0x0005C6  4720          r7 = 0
0x01E006C8  app+0x0005C8  50ec4160      d[r4+0] = r7_r6
0x01E006CC  app+0x0005CC  c061          [r4+4] = r0
0x01E006CE  app+0x0005CE  8251          if (r2 != 0) goto 34
0x01E006D0  app+0x0005D0  c5ff1cce0000  r5 = 52764
0x01E006D6  app+0x0005D6  5060          r0 = [r5+0]
0x01E006D8  app+0x0005D8  c2ff6406e001  r2 = 31458916
0x01E006DE  app+0x0005DE  9260          [r1+0] = r2
0x01E006E0  app+0x0005E0  8048          if (r0 != 0) goto 16
0x01E006E2  app+0x0005E2  682c          r0 = 172
0x01E006E4  app+0x0005E4  81eac084      call 199040
0x01E006E8  app+0x0005E8  d060          [r5+0] = r0
0x01E006EA  app+0x0005EA  30e80010      if (r0 == 0) {
0x01E006EE  app+0x0005EE  4420          r4 = 0
0x01E006F0  app+0x0005F0  618c          call -104
0x01E006F2  app+0x0005F2  4016          r0 = r4
0x01E006F4  app+0x0005F4  5704          {pc, r7-r4} = [sp++]
0x01E006F6  app+0x0005F6  4020          r0 = 0
0x01E006F8  app+0x0005F8  8000          rts
0x01E006FA  app+0x0005FA  01d6          r1 = r0  #
0x01E006FC  app+0x0005FC  1a41          r2 = b[r1+1] (u)
0x01E006FE  app+0x0005FE  4020          r0 = 0
0x01E00700  app+0x000600  9a40          b[r1+0] = r2
0x01E00702  app+0x000602  8000          rts
0x01E00704  app+0x000604  7a04          [--sp] = {rets, r10-r4}
0x01E00706  app+0x000606  4420          r4 = 0
0x01E00708  app+0x000608  148b          goto 86
0x01E0070A  app+0x00060A  d8ec0a54      r5 = [r0+r4<<2]
0x01E0070E  app+0x00060E  1660          r6 = [r1+0]
0x01E00710  app+0x000610  54ed2070      r7 = h[r2+0] (s)
0x01E00714  app+0x000614  b4e06285      r8 = r6 - r5
0x01E00718  app+0x000618  871b          r7 *= r8
0x01E0071A  app+0x00061A  cf20          r7 += 32
0x01E0071C  app+0x00061C  ffa6          r7 = r7 >>> 6
0x01E0071E  app+0x00061E  b4e07296      r9 = r7 - r6
0x01E00722  app+0x000622  d0ec1190      [r1+0] = r9
0x01E00726  app+0x000626  1762          r7 = [r1+8]
0x01E00728  app+0x000628  1561          r5 = [r1+4]
0x01E0072A  app+0x00062A  54ed2460      r6 = h[r2+4] (s)
0x01E0072E  app+0x00062E  df1f          r7 = r5 - r7
0x01E00730  app+0x000630  761b          r6 *= r7
0x01E00732  app+0x000632  ce20          r6 += 32
0x01E00734  app+0x000634  eea6          r6 = r6 >>> 6
0x01E00736  app+0x000636  6d1f          r5 = r6 - r5
0x01E00738  app+0x000638  dddf          r5 = r5 - r7  #
0x01E0073A  app+0x00063A  9561          [r1+4] = r5
0x01E0073C  app+0x00063C  d8ec0aa4      r10 = [r0+r4<<2]
0x01E00740  app+0x000640  54ed2260      r6 = h[r2+2] (s)
0x01E00744  app+0x000644  b4e0527a      r7 = r5 - r10
0x01E00748  app+0x000648  761b          r6 *= r7
0x01E0074A  app+0x00064A  ce20          r6 += 32
0x01E0074C  app+0x00064C  eea6          r6 = r6 >>> 6
0x01E0074E  app+0x00064E  6d1f          r5 = r6 - r5
0x01E00750  app+0x000650  b4f09268      r6 = r9 - r8  #
0x01E00754  app+0x000654  9562          [r1+8] = r5
0x01E00756  app+0x000656  ee1f          r6 = r6 - r7
0x01E00758  app+0x000658  6518          r5 += r6
0x01E0075A  app+0x00065A  d8ec0b54      [r0+r4<<2] = r5
0x01E0075E  app+0x00065E  c421          r4 += 1
0x01E00760  app+0x000660  83edd341      ifs (r4 < r3) goto -90
0x01E00764  app+0x000664  5a04          {pc, r10-r4} = [sp++]
0x01E00766  app+0x000666  7f04          [--sp] = {rets, r15-r4}
0x01E00768  app+0x000668  e29a          sp += -24
0x01E0076A  app+0x00066A  3fd6          r15 = r3  #
0x01E0076C  app+0x00066C  8225          [sp+20] = r2
0x01E0076E  app+0x00066E  46f0ff03      r6 = 1023  #
0x01E00772  app+0x000672  8124          [sp+16] = r1
0x01E00774  app+0x000674  0998          r1 = r0 + 24
0x01E00776  app+0x000676  c1d4          r9 = 0  #
0x01E00778  app+0x000678  8123          [sp+12] = r1
0x01E0077A  app+0x00067A  0988          r1 = r0 + 8
0x01E0077C  app+0x00067C  01f17c00      r1 = r0 + 124  #
0x01E00780  app+0x000680  8122          [sp+8] = r1
0x01E00782  app+0x000682  01f17000      r1 = r0 + 112  #
0x01E00786  app+0x000686  8121          [sp+4] = r1
0x01E00788  app+0x000688  0ef16400      r14 = r0 + 100  #
0x01E0078C  app+0x00068C  8120          [sp] = r1
0x01E0078E  app+0x00068E  4520          r5 = 0
0x01E00790  app+0x000690  caff7806e001  r10 = 31458936
0x01E00796  app+0x000696  cdff00fcff01  r13 = 33553408
0x01E0079C  app+0x00069C  c8ffff7fffff  r8 = 4294934527
0x01E007A2  app+0x0006A2  5483          goto 326
0x01E007A4  app+0x0006A4  a2d6          r2 = r10  #
0x01E007A6  app+0x0006A6  0124          r1 = [sp+16]
0x01E007A8  app+0x0006A8  d8ed1a19      r1 = h[r1+r9<<1] (s)
0x01E007AC  app+0x0006AC  8560          [r0+0] = r5
0x01E007AE  app+0x0006AE  e1d6          r1 = r14  #
0x01E007B0  app+0x0006B0  8161          [r0+4] = r1
0x01E007B2  app+0x0006B2  4322          r3 = 2
0x01E007B4  app+0x0006B4  5187          call -178
0x01E007B6  app+0x0006B6  4121          r1 = 1
0x01E007B8  app+0x0006B8  0222          r2 = [sp+8]
0x01E007BA  app+0x0006BA  0486          goto 12
0x01E007BC  app+0x0006BC  d8ec0a31      r3 = [r0+r1<<2]
0x01E007C0  app+0x0006C0  a361          [r2+4] = r3
0x01E007C2  app+0x0006C2  dfec295f      [r2++=-8] = r5
0x01E007C6  app+0x0006C6  f93f          r1 += -1
0x01E007C8  app+0x0006C8  01fdf801      ifs (r1 >= 0) goto -16
0x01E007CC  app+0x0006CC  02f106a0      r2 = r10 + 6  #
0x01E007D0  app+0x0006D0  0120          r1 = [sp]
0x01E007D2  app+0x0006D2  4324          r3 = 4
0x01E007D4  app+0x0006D4  4197          call -210
0x01E007D6  app+0x0006D6  4123          r1 = 3
0x01E007D8  app+0x0006D8  0223          r2 = [sp+12]
0x01E007DA  app+0x0006DA  0486          goto 12
0x01E007DC  app+0x0006DC  d8ec0a31      r3 = [r0+r1<<2]
0x01E007E0  app+0x0006E0  a361          [r2+4] = r3
0x01E007E2  app+0x0006E2  dfec295f      [r2++=-8] = r5
0x01E007E6  app+0x0006E6  f93f          r1 += -1
0x01E007E8  app+0x0006E8  01fdf801      ifs (r1 >= 0) goto -16
0x01E007EC  app+0x0006EC  02f10ca0      r2 = r10 + 12  #
0x01E007F0  app+0x0006F0  0121          r1 = [sp+4]
0x01E007F2  app+0x0006F2  4328          r3 = 8
0x01E007F4  app+0x0006F4  4187          call -242
0x01E007F6  app+0x0006F6  4120          r1 = 0
0x01E007F8  app+0x0006F8  048e          goto 28
0x01E007FA  app+0x0006FA  d8ec0a31      r3 = [r0+r1<<2]
0x01E007FE  app+0x0006FE  42e0ff7f      r2 = 32767
0x01E00802  app+0x000702  2aff003c0400  ifs (r3 >= 32768) goto 8
0x01E00808  app+0x000708  42e00080      r2 = -32768
0x01E0080C  app+0x00070C  08ee0230      ifs (r3 > r8) goto 4
0x01E00810  app+0x000710  d8ec0b21      [r0+r1<<2] = r2
0x01E00814  app+0x000714  c121          r1 += 1
0x01E00816  app+0x000716  81f8f011      if (r1 != 8) goto -32
0x01E0081A  app+0x00071A  4120          r1 = 0
0x01E0081C  app+0x00071C  02e14000      r2 = r0 + 64
0x01E00820  app+0x000720  c3d4          r11 = 0  #
0x01E00822  app+0x000722  a940          b[r2+0] = r1
0x01E00824  app+0x000724  4321          r3 = 1
0x01E00826  app+0x000726  249a          goto 180
0x01E00828  app+0x000728  2c48          r4 = b[r2+8] (u)
0x01E0082A  app+0x00072A  44a1          r4 = r4 << 1
0x01E0082C  app+0x00072C  64e10e40      r4 = r4 & 0xE
0x01E00830  app+0x000730  ac48          b[r2+8] = r4
0x01E00832  app+0x000732  d8ec0a7b      r7 = [r0+r11<<2]
0x01E00836  app+0x000736  d0ec24c0      r12 = [r2+4]
0x01E0083A  app+0x00073A  77aa          r7 = r7 << 10
0x01E0083C  app+0x00073C  0ced1170      ifs (r7 >= r12) goto 34
0x01E00840  app+0x000740  47e10140      r7 = r4 | 0x1
0x01E00844  app+0x000744  11d7          r1 = r1.b0 (u)  #
0x01E00846  app+0x000746  af48          b[r2+8] = r7
0x01E00848  app+0x000748  3119          r1 |= r3
0x01E0084A  app+0x00074A  a940          b[r2+0] = r1
0x01E0084C  app+0x00074C  2463          r4 = [r2+12]
0x01E0084E  app+0x00074E  87f8181e      if (r7 != 15) goto 48
0x01E00852  app+0x000752  e7e0204d      r7 = r4 + 0x2800
0x01E00856  app+0x000756  a763          [r2+12] = r7
0x01E00858  app+0x000758  27e9a009      if (r7 >= 1310720) {
0x01E0085C  app+0x00075C  60e0a079      r7 = 0x140000
0x01E00860  app+0x000760  049d          goto 58
0x01E00862  app+0x000762  2763          r7 = [r2+12]
0x01E00864  app+0x000764  045f          if (r4 == 0) goto 62
0x01E00866  app+0x000766  f4aa          r4 = r7 >> 10
0x01E00868  app+0x000768  641b          r4 *= r6
0x01E0086A  app+0x00076A  e719          r7 &= r6
0x01E0086C  app+0x00076C  671b          r7 *= r6
0x01E0086E  app+0x00076E  07e10072      r7 = r7 + 512
0x01E00872  app+0x000772  f7aa          r7 = r7 >> 10
0x01E00874  app+0x000774  4718          r7 += r4
0x01E00876  app+0x000776  a763          [r2+12] = r7
0x01E00878  app+0x000778  a7ec200d      if (r7 <= 10240) {
0x01E0087C  app+0x00077C  47e00028      r7 = 10240
0x01E00880  app+0x000780  0498          goto 48
0x01E00882  app+0x000782  c7aa          r7 = r4 >> 10
0x01E00884  app+0x000784  671b          r7 *= r6
0x01E00886  app+0x000786  e419          r4 &= r6
0x01E00888  app+0x000788  641b          r4 *= r6
0x01E0088A  app+0x00078A  04e10042      r4 = r4 + 512
0x01E0088E  app+0x00078E  c4aa          r4 = r4 >> 10
0x01E00890  app+0x000790  4718          r7 += r4
0x01E00892  app+0x000792  a763          [r2+12] = r7
0x01E00894  app+0x000794  a7ec200d      if (r7 <= 10240) {
0x01E00898  app+0x000798  47e00028      r7 = 10240
0x01E0089C  app+0x00079C  b4f0c277      r7 = r12 - r7  #
0x01E008A0  app+0x0007A0  a763          [r2+12] = r7
0x01E008A2  app+0x0007A2  0489          goto 18
0x01E008A4  app+0x0007A4  e7e0207d      r7 = r7 + 0x2800
0x01E008A8  app+0x0007A8  a763          [r2+12] = r7
0x01E008AA  app+0x0007AA  27e9a009      if (r7 >= 1310720) {
0x01E008AE  app+0x0007AE  60e0a079      r7 = 0x140000
0x01E008B2  app+0x0007B2  c7d8          r7 += r12  #
0x01E008B4  app+0x0007B4  a763          [r2+12] = r7
0x01E008B6  app+0x0007B6  d4d6          r4 = r13  #
0x01E008B8  app+0x0007B8  a761          [r2+4] = r7
0x01E008BA  app+0x0007BA  04ee0570      ifs (r7 > r4) goto 10
0x01E008BE  app+0x0007BE  60e07e44      r4 = 0xFE000000
0x01E008C2  app+0x0007C2  2aff7e740200  ifs (r7 >= 4261412864) goto 4
0x01E008C8  app+0x0007C8  47d6          r7 = r4  #
0x01E008CA  app+0x0007CA  a461          [r2+4] = r4
0x01E008CC  app+0x0007CC  e4e11f70      r4 = r7 * 0x1F
0x01E008D0  app+0x0007D0  c430          r4 += 16
0x01E008D2  app+0x0007D2  cca5          r4 = r4 >>> 5
0x01E008D4  app+0x0007D4  0bf101b0      r11 = r11 + 1  #
0x01E008D8  app+0x0007D8  a461          [r2+4] = r4
0x01E008DA  app+0x0007DA  33a1          r3 = r3 << 1
0x01E008DC  app+0x0007DC  8bf8a411      if (r11 != 8) goto -184
0x01E008E0  app+0x0007E0  0225          r2 = [sp+20]
0x01E008E2  app+0x0007E2  d8ee2119      b[r2+r9] = r1
0x01E008E6  app+0x0007E6  09e10190      r9 = r9 + 1
0x01E008EA  app+0x0007EA  8fed5b91      ifs (r9 < r15) goto -330
0x01E008EE  app+0x0007EE  4020          r0 = 0
0x01E008F0  app+0x0007F0  0286          sp += 24
0x01E008F2  app+0x0007F2  5f04          {pc, r15-r4} = [sp++]
0x01E008F4  app+0x0007F4  7804          [--sp] = {rets, r8-r4}
0x01E008F6  app+0x0007F6  e29e          sp += -8
0x01E008F8  app+0x0007F8  0516          r5 = r0
0x01E008FA  app+0x0007FA  142a          r4 = -22
0x01E008FC  app+0x0007FC  154c          if (r5 == 0) goto 88
0x01E008FE  app+0x0007FE  c7ff1cce0000  r7 = 52764
0x01E00904  app+0x000804  7060          r0 = [r7+0]
0x01E00906  app+0x000806  1047          if (r0 == 0) goto 78
0x01E00908  app+0x000808  5061          r0 = [r5+4]
0x01E0090A  app+0x00080A  5940          r1 = b[r5+0] (u)
0x01E0090C  app+0x00080C  12a1          r2 = r1 << 1
0x01E0090E  app+0x00080E  8984          r1 = sp + 4
0x01E00910  app+0x000810  81eabbde      call 245110
0x01E00914  app+0x000814  0616          r6 = r0
0x01E00916  app+0x000816  065c          if (r6 == 0) goto 56
0x01E00918  app+0x000818  0021          r0 = [sp+4]
0x01E0091A  app+0x00081A  005d          if (r0 == 0) goto 58
0x01E0091C  app+0x00081C  86fd1a02      ifs (r6 < 1) goto 52
0x01E00920  app+0x000820  5061          r0 = [r5+4]
0x01E00922  app+0x000822  8980          r1 = sp + 0
0x01E00924  app+0x000824  81eacdde      call 245146
0x01E00928  app+0x000828  e6a1          r6 = r6 >> 1
0x01E0092A  app+0x00082A  86ed1400      ifs (r0 < r6) goto 40
0x01E0092E  app+0x00082E  d4e90080      r8 = [sp]
0x01E00932  app+0x000832  08f81000      if (r8 == 0) goto 32
0x01E00936  app+0x000836  ebd7          r3 = r6.l (s)  #
0x01E00938  app+0x000838  7060          r0 = [r7+0]
0x01E0093A  app+0x00083A  82d6          r2 = r8  #
0x01E0093C  app+0x00083C  0121          r1 = [sp+4]
0x01E0093E  app+0x00083E  bfea12ff      call -476
0x01E00942  app+0x000842  81d6          r1 = r8  #
0x01E00944  app+0x000844  5061          r0 = [r5+4]
0x01E00946  app+0x000846  6216          r2 = r6
0x01E00948  app+0x000848  81ea87de      call 245006
0x01E0094C  app+0x00084C  4420          r4 = 0
0x01E0094E  app+0x00084E  0483          goto 6
0x01E00950  app+0x000850  4c23          r4 = 35
0x01E00952  app+0x000852  0481          goto 2
0x01E00954  app+0x000854  4c21          r4 = 33
0x01E00956  app+0x000856  4016          r0 = r4
0x01E00958  app+0x000858  0282          sp += 8
0x01E0095A  app+0x00085A  5804          {pc, r8-r4} = [sp++]
0x01E0095C  app+0x00085C  4020          r0 = 0
0x01E0095E  app+0x00085E  8000          rts
0x01E00960  app+0x000860  7404          [--sp] = {rets, r4}
0x01E00962  app+0x000862  c4ff1cce0000  r4 = 52764
0x01E00968  app+0x000868  4060          r0 = [r4+0]
0x01E0096A  app+0x00086A  0048          if (r0 == 0) goto 16
0x01E0096C  app+0x00086C  81ea7b83      call 198390
0x01E00970  app+0x000870  4020          r0 = 0
0x01E00972  app+0x000872  c060          [r4+0] = r0
0x01E00974  app+0x000874  c1ff20ce0000  r1 = 52768
0x01E0097A  app+0x00087A  9060          [r1+0] = r0
0x01E0097C  app+0x00087C  4020          r0 = 0
0x01E0097E  app+0x00087E  5404          {pc, r4} = [sp++]
0x01E00980  app+0x000880  682c          r0 = 172
0x01E00982  app+0x000882  8000          rts
0x01E00984  app+0x000884  7f04          [--sp] = {rets, r15-r4}
0x01E00986  app+0x000886  e29d          sp += -12
0x01E00988  app+0x000888  3dd6          r13 = r3  #
0x01E0098A  app+0x00088A  8222          [sp+8] = r2
0x01E0098C  app+0x00088C  08d6          r8 = r0  #
0x01E0098E  app+0x00088E  8121          [sp+4] = r1
0x01E00990  app+0x000890  c820          r0 += 32
0x01E00992  app+0x000892  01e16800      r1 = r0 + 104
0x01E00996  app+0x000896  0bf17400      r11 = r0 + 116  #
0x01E0099A  app+0x00089A  8120          [sp] = r1
0x01E0099C  app+0x00089C  0ce18000      r12 = r0 + 128
0x01E009A0  app+0x0008A0  4520          r5 = 0
0x01E009A2  app+0x0008A2  caff7806e001  r10 = 31458936
0x01E009A8  app+0x0008A8  4422          r4 = 2
0x01E009AA  app+0x0008AA  47e0ff03      r7 = 1023
0x01E009AE  app+0x0008AE  ceff00fcff01  r14 = 33553408
0x01E009B4  app+0x0008B4  5489          goto 338
0x01E009B6  app+0x0008B6  8fd6          r15 = r8  #
0x01E009B8  app+0x0008B8  0121          r1 = [sp+4]
0x01E009BA  app+0x0008BA  d8ee1015      r1 = b[r1+r5] (u)
0x01E009BE  app+0x0008BE  5aeef015      b[++r15=80] = r1
0x01E009C2  app+0x0008C2  4120          r1 = 0
0x01E009C4  app+0x0008C4  249c          goto 184
0x01E009C6  app+0x0008C6  50eef020      r2 = b[r15+0] (u)
0x01E009CA  app+0x0008CA  50eef830      r3 = b[r15+8] (u)
0x01E009CE  app+0x0008CE  c8e12291      r9 = r2 >> r1
0x01E009D2  app+0x0008D2  62e10190      r2 = r9 & 0x1
0x01E009D6  app+0x0008D6  33a1          r3 = r3 << 1
0x01E009D8  app+0x0008D8  63e10e30      r3 = r3 & 0xE
0x01E009DC  app+0x0008DC  3219          r2 |= r3
0x01E009DE  app+0x0008DE  52eef820      b[r15+8] = r2
0x01E009E2  app+0x0008E2  59e81002      if ((r9 & 1) != 0) goto 32
0x01E009E6  app+0x0008E6  036f          r3 = [r0+60]
0x01E009E8  app+0x0008E8  124a          if (r2 == 0) goto 84
0x01E009EA  app+0x0008EA  b2aa          r2 = r3 >> 10
0x01E009EC  app+0x0008EC  721b          r2 *= r7
0x01E009EE  app+0x0008EE  f319          r3 &= r7
0x01E009F0  app+0x0008F0  731b          r3 *= r7
0x01E009F2  app+0x0008F2  03e10032      r3 = r3 + 512
0x01E009F6  app+0x0008F6  b3aa          r3 = r3 >> 10
0x01E009F8  app+0x0008F8  3218          r2 += r3
0x01E009FA  app+0x0008FA  826f          [r0+60] = r2
0x01E009FC  app+0x0008FC  a2ec200d      if (r2 <= 10240) {
0x01E00A00  app+0x000900  42e00028      r2 = 10240
0x01E00A04  app+0x000904  1483          goto 70
0x01E00A06  app+0x000906  036f          r3 = [r0+60]
0x01E00A08  app+0x000908  82f8081e      if (r2 != 15) goto 16
0x01E00A0C  app+0x00090C  e2e0203d      r2 = r3 + 0x2800
0x01E00A10  app+0x000910  826f          [r0+60] = r2
0x01E00A12  app+0x000912  22e9a009      if (r2 >= 1310720) {
0x01E00A16  app+0x000916  60e0a029      r2 = 0x140000
0x01E00A1A  app+0x00091A  048d          goto 26
0x01E00A1C  app+0x00091C  b2aa          r2 = r3 >> 10
0x01E00A1E  app+0x00091E  721b          r2 *= r7
0x01E00A20  app+0x000920  f319          r3 &= r7
0x01E00A22  app+0x000922  731b          r3 *= r7
0x01E00A24  app+0x000924  03e10032      r3 = r3 + 512
0x01E00A28  app+0x000928  b3aa          r3 = r3 >> 10
0x01E00A2A  app+0x00092A  3218          r2 += r3
0x01E00A2C  app+0x00092C  826f          [r0+60] = r2
0x01E00A2E  app+0x00092E  a2ec200d      if (r2 <= 10240) {
0x01E00A32  app+0x000932  42e00028      r2 = 10240
0x01E00A36  app+0x000936  826f          [r0+60] = r2
0x01E00A38  app+0x000938  036d          r3 = [r0+52]
0x01E00A3A  app+0x00093A  b21e          r2 = r3 - r2
0x01E00A3C  app+0x00093C  048a          goto 20
0x01E00A3E  app+0x00093E  e2e0203d      r2 = r3 + 0x2800
0x01E00A42  app+0x000942  826f          [r0+60] = r2
0x01E00A44  app+0x000944  22e9a009      if (r2 >= 1310720) {
0x01E00A48  app+0x000948  60e0a029      r2 = 0x140000
0x01E00A4C  app+0x00094C  826f          [r0+60] = r2
0x01E00A4E  app+0x00094E  036d          r3 = [r0+52]
0x01E00A50  app+0x000950  3218          r2 += r3
0x01E00A52  app+0x000952  e3d6          r3 = r14  #
0x01E00A54  app+0x000954  826d          [r0+52] = r2
0x01E00A56  app+0x000956  03ee0520      ifs (r2 > r3) goto 10
0x01E00A5A  app+0x00095A  60e07e34      r3 = 0xFE000000
0x01E00A5E  app+0x00095E  2aff7e240200  ifs (r2 >= 4261412864) goto 4
0x01E00A64  app+0x000964  32d6          r2 = r3  #
0x01E00A66  app+0x000966  836d          [r0+52] = r3
0x01E00A68  app+0x000968  e2e11f20      r2 = r2 * 0x1F
0x01E00A6C  app+0x00096C  c230          r2 += 16
0x01E00A6E  app+0x00096E  aaa5          r2 = r2 >>> 5
0x01E00A70  app+0x000970  02f10022      r2 = r2 + 512  #
0x01E00A74  app+0x000974  826d          [r0+52] = r2
0x01E00A76  app+0x000976  aaaa          r2 = r2 >>> 10
0x01E00A78  app+0x000978  d8ec0b21      [r0+r1<<2] = r2
0x01E00A7C  app+0x00097C  c121          r1 += 1
0x01E00A7E  app+0x00097E  81f8a211      if (r1 != 8) goto -188
0x01E00A82  app+0x000982  02e10ca0      r2 = r10 + 12
0x01E00A86  app+0x000986  4328          r3 = 8
0x01E00A88  app+0x000988  c116          r1 = r12
0x01E00A8A  app+0x00098A  bfea3bfe      call -906
0x01E00A8E  app+0x00098E  4120          r1 = 0
0x01E00A90  app+0x000990  4220          r2 = 0
0x01E00A92  app+0x000992  0488          goto 16
0x01E00A94  app+0x000994  d8ec0231      r3 = [r0+r1]
0x01E00A98  app+0x000998  f4e13134      r3 = r3 / r4 (s)
0x01E00A9C  app+0x00099C  d8ec0b32      [r0+r2<<2] = r3
0x01E00AA0  app+0x0009A0  c128          r1 += 8
0x01E00AA2  app+0x0009A2  c221          r2 += 1
0x01E00AA4  app+0x0009A4  82f8f609      if (r2 != 4) goto -20
0x01E00AA8  app+0x0009A8  02e106a0      r2 = r10 + 6
0x01E00AAC  app+0x0009AC  4324          r3 = 4
0x01E00AAE  app+0x0009AE  b116          r1 = r11
0x01E00AB0  app+0x0009B0  bfea28fe      call -944
0x01E00AB4  app+0x0009B4  4120          r1 = 0
0x01E00AB6  app+0x0009B6  4220          r2 = 0
0x01E00AB8  app+0x0009B8  0488          goto 16
0x01E00ABA  app+0x0009BA  d8ec0231      r3 = [r0+r1]
0x01E00ABE  app+0x0009BE  f4e13134      r3 = r3 / r4 (s)
0x01E00AC2  app+0x0009C2  d8ec0b32      [r0+r2<<2] = r3
0x01E00AC6  app+0x0009C6  c128          r1 += 8
0x01E00AC8  app+0x0009C8  c221          r2 += 1
0x01E00ACA  app+0x0009CA  82f8f605      if (r2 != 2) goto -20
0x01E00ACE  app+0x0009CE  4322          r3 = 2
0x01E00AD0  app+0x0009D0  a2d6          r2 = r10  #
0x01E00AD2  app+0x0009D2  0120          r1 = [sp]
0x01E00AD4  app+0x0009D4  bfea16fe      call -980
0x01E00AD8  app+0x0009D8  42f0ff7f      r2 = 32767  #
0x01E00ADC  app+0x0009DC  0360          r3 = [r0+0]
0x01E00ADE  app+0x0009DE  f4e13114      r1 = r3 / r4 (s)
0x01E00AE2  app+0x0009E2  8160          [r0+0] = r1
0x01E00AE4  app+0x0009E4  2aff803b0700  ifs (r3 >= 65536) goto 14
0x01E00AEA  app+0x0009EA  42e00080      r2 = -32768
0x01E00AEE  app+0x0009EE  c6fffefffeff  r6 = 4294901758
0x01E00AF4  app+0x0009F4  06ee0230      ifs (r3 > r6) goto 4
0x01E00AF8  app+0x0009F8  21d6          r1 = r2  #
0x01E00AFA  app+0x0009FA  8260          [r0+0] = r2
0x01E00AFC  app+0x0009FC  50edf311      h[r15+18] = r1
0x01E00B00  app+0x000A00  0222          r2 = [sp+8]
0x01E00B02  app+0x000A02  d8ed2915      h[r2+r5<<1] = r1
0x01E00B06  app+0x000A06  c521          r5 += 1
0x01E00B08  app+0x000A08  8ded5551      ifs (r5 < r13) goto -342
0x01E00B0C  app+0x000A0C  4020          r0 = 0
0x01E00B0E  app+0x000A0E  0283          sp += 12
0x01E00B10  app+0x000A10  5f04          {pc, r15-r4} = [sp++]
0x01E00B12  app+0x000A12  8000          rts
0x01E00B14  app+0x000A14  acaf          r4 = r2 >>> 15
0x01E00B16  app+0x000A16  e401          .byte 0xE4, 0x01
0x01E00B18  app+0x000A18  5cbb          r4 = r5 <<< 27
0x01E00B1A  app+0x000A1A  e401          .byte 0xE4, 0x01
0x01E00B1C  app+0x000A1C  4abc          r2 = r4 <<< 28
0x01E00B1E  app+0x000A1E  e401          .byte 0xE4, 0x01
0x01E00B20  app+0x000A20  7ebc          r6 = r7 <<< 28
0x01E00B22  app+0x000A22  e401          .byte 0xE4, 0x01
0x01E00B24  app+0x000A24  88bc          r0 = r0 >>> 28
0x01E00B26  app+0x000A26  e401          .byte 0xE4, 0x01
0x01E00B28  app+0x000A28  666d          r6 = [r6+52]
0x01E00B2A  app+0x000A2A  5f69          r7 = h[r5+18] (u)
0x01E00B2C  app+0x000A2C  6e73          r6 = h[r6+-26] (u)
0x01E00B2E  app+0x000A2E  6964          r1 = h[r6+8] (u)
0x01E00B30  app+0x000A30  6500          .byte 0x65, 0x00
0x01E00B32  app+0x000A32  0000          nop
0x01E00B34  app+0x000A34  0000          nop
0x01E00B36  app+0x000A36  0000          nop
0x01E00B38  app+0x000A38  0000          nop
0x01E00B3A  app+0x000A3A  0000          nop
0x01E00B3C  app+0x000A3C  1100          .byte 0x11, 0x00
0x01E00B3E  app+0x000A3E  0000          nop
0x01E00B40  app+0x000A40  7488          goto 464
0x01E00B42  app+0x000A42  e401          .byte 0xE4, 0x01
0x01E00B44  app+0x000A44  0411          r4 = b[r0++=r10] (u)
0x01E00B46  app+0x000A46  0000          nop
0x01E00B48  app+0x000A48  ca85          r2 = sp + 69
0x01E00B4A  app+0x000A4A  e401          .byte 0xE4, 0x01
0x01E00B4C  app+0x000A4C  0811          b[r0++=r10] = r0
0x01E00B4E  app+0x000A4E  0000          nop
0x01E00B50  app+0x000A50  1285          .byte 0x12, 0x85
0x01E00B52  app+0x000A52  e401          .byte 0xE4, 0x01
0x01E00B54  app+0x000A54  0311          r3 = b[r0++=r10] (u)
0x01E00B56  app+0x000A56  0000          nop
0x01E00B58  app+0x000A58  ca85          r2 = sp + 69
0x01E00B5A  app+0x000A5A  e401          .byte 0xE4, 0x01
0x01E00B5C  app+0x000A5C  0711          r7 = b[r0++=r10] (u)
0x01E00B5E  app+0x000A5E  0000          nop
0x01E00B60  app+0x000A60  1285          .byte 0x12, 0x85
0x01E00B62  app+0x000A62  e401          .byte 0xE4, 0x01
0x01E00B64  app+0x000A64  0e00          .byte 0x0E, 0x00
0x01E00B66  app+0x000A66  0000          nop
0x01E00B68  app+0x000A68  0289          sp += 36
0x01E00B6A  app+0x000A6A  e401          .byte 0xE4, 0x01
0x01E00B6C  app+0x000A6C  1520          r5 = -32
0x01E00B6E  app+0x000A6E  0000          nop
0x01E00B70  app+0x000A70  fe88          .byte 0xFE, 0x88
0x01E00B72  app+0x000A72  e401          .byte 0xE4, 0x01
0x01E00B74  app+0x000A74  0e10          b[r0++=r8] = r6
0x01E00B76  app+0x000A76  0000          nop
0x01E00B78  app+0x000A78  fa88          .byte 0xFA, 0x88
0x01E00B7A  app+0x000A7A  e401          .byte 0xE4, 0x01
0x01E00B7C  app+0x000A7C  0c00          .byte 0x0C, 0x00
0x01E00B7E  app+0x000A7E  0000          nop
0x01E00B80  app+0x000A80  0a88          r2 = r0 + 8
0x01E00B82  app+0x000A82  e401          .byte 0xE4, 0x01
0x01E00B84  app+0x000A84  0b00          .byte 0x0B, 0x00
0x01E00B86  app+0x000A86  0000          nop
0x01E00B88  app+0x000A88  ca87          r2 = sp + 71
0x01E00B8A  app+0x000A8A  e401          .byte 0xE4, 0x01
0x01E00B8C  app+0x000A8C  1510          r5 = b[r1++=r8] (u)
0x01E00B8E  app+0x000A8E  0000          nop
0x01E00B90  app+0x000A90  c687          goto -1266
0x01E00B92  app+0x000A92  e401          .byte 0xE4, 0x01
0x01E00B94  app+0x000A94  0400          .byte 0x04, 0x00
0x01E00B96  app+0x000A96  0000          nop
0x01E00B98  app+0x000A98  ae87          r6 = sp + 39
0x01E00B9A  app+0x000A9A  e401          .byte 0xE4, 0x01
0x01E00B9C  app+0x000A9C  3120          r1 |= 1
0x01E00B9E  app+0x000A9E  0000          nop
0x01E00BA0  app+0x000AA0  aa87          r2 = sp + 39
0x01E00BA2  app+0x000AA2  e401          .byte 0xE4, 0x01
0x01E00BA4  app+0x000AA4  3020          r0 |= 1
0x01E00BA6  app+0x000AA6  0000          nop
0x01E00BA8  app+0x000AA8  a687          goto -1394
0x01E00BAA  app+0x000AAA  e401          .byte 0xE4, 0x01
0x01E00BAC  app+0x000AAC  3220          r2 |= 1
0x01E00BAE  app+0x000AAE  0000          nop
0x01E00BB0  app+0x000AB0  a287          sp += -356
0x01E00BB2  app+0x000AB2  e401          .byte 0xE4, 0x01
0x01E00BB4  app+0x000AB4  2020          r0 = [sp+128]
0x01E00BB6  app+0x000AB6  0000          nop
0x01E00BB8  app+0x000AB8  9e87          .byte 0x9E, 0x87
0x01E00BBA  app+0x000ABA  e401          .byte 0xE4, 0x01
0x01E00BBC  app+0x000ABC  1420          r4 = -32
0x01E00BBE  app+0x000ABE  0000          nop
0x01E00BC0  app+0x000AC0  1687          goto -1970
0x01E00BC2  app+0x000AC2  e401          .byte 0xE4, 0x01
0x01E00BC4  app+0x000AC4  1320          r3 = -32
0x01E00BC6  app+0x000AC6  0000          nop
0x01E00BC8  app+0x000AC8  1687          goto -1970
0x01E00BCA  app+0x000ACA  e401          .byte 0xE4, 0x01
0x01E00BCC  app+0x000ACC  1220          r2 = -32
0x01E00BCE  app+0x000ACE  0000          nop
0x01E00BD0  app+0x000AD0  1687          goto -1970
0x01E00BD2  app+0x000AD2  e401          .byte 0xE4, 0x01
0x01E00BD4  app+0x000AD4  0610          r6 = b[r0++=r8] (u)
0x01E00BD6  app+0x000AD6  0000          nop
0x01E00BD8  app+0x000AD8  9a87          .byte 0x9A, 0x87
0x01E00BDA  app+0x000ADA  e401          .byte 0xE4, 0x01
0x01E00BDC  app+0x000ADC  0710          r7 = b[r0++=r8] (u)
0x01E00BDE  app+0x000ADE  0000          nop
0x01E00BE0  app+0x000AE0  9a87          .byte 0x9A, 0x87
0x01E00BE2  app+0x000AE2  e401          .byte 0xE4, 0x01
0x01E00BE4  app+0x000AE4  0810          b[r0++=r8] = r0
0x01E00BE6  app+0x000AE6  0000          nop
0x01E00BE8  app+0x000AE8  9a87          .byte 0x9A, 0x87
0x01E00BEA  app+0x000AEA  e401          .byte 0xE4, 0x01
0x01E00BEC  app+0x000AEC  0910          b[r0++=r8] = r1
0x01E00BEE  app+0x000AEE  0000          nop
0x01E00BF0  app+0x000AF0  9a87          .byte 0x9A, 0x87
0x01E00BF2  app+0x000AF2  e401          .byte 0xE4, 0x01
0x01E00BF4  app+0x000AF4  0a10          b[r0++=r8] = r2
0x01E00BF6  app+0x000AF6  0000          nop
0x01E00BF8  app+0x000AF8  9a87          .byte 0x9A, 0x87
0x01E00BFA  app+0x000AFA  e401          .byte 0xE4, 0x01
0x01E00BFC  app+0x000AFC  0110          r1 = b[r0++=r8] (u)
0x01E00BFE  app+0x000AFE  0000          nop
0x01E00C00  app+0x000B00  9687          goto -1458
0x01E00C02  app+0x000B02  e401          .byte 0xE4, 0x01
0x01E00C04  app+0x000B04  0210          r2 = b[r0++=r8] (u)
0x01E00C06  app+0x000B06  0000          nop
0x01E00C08  app+0x000B08  9687          goto -1458
0x01E00C0A  app+0x000B0A  e401          .byte 0xE4, 0x01
0x01E00C0C  app+0x000B0C  0310          r3 = b[r0++=r8] (u)
0x01E00C0E  app+0x000B0E  0000          nop
0x01E00C10  app+0x000B10  9687          goto -1458
0x01E00C12  app+0x000B12  e401          .byte 0xE4, 0x01
0x01E00C14  app+0x000B14  0410          r4 = b[r0++=r8] (u)
0x01E00C16  app+0x000B16  0000          nop
0x01E00C18  app+0x000B18  9687          goto -1458
0x01E00C1A  app+0x000B1A  e401          .byte 0xE4, 0x01
0x01E00C1C  app+0x000B1C  0510          r5 = b[r0++=r8] (u)
0x01E00C1E  app+0x000B1E  0000          nop
0x01E00C20  app+0x000B20  9687          goto -1458
0x01E00C22  app+0x000B22  e401          .byte 0xE4, 0x01
0x01E00C24  app+0x000B24  1820          .byte 0x18, 0x20
0x01E00C26  app+0x000B26  0000          nop
0x01E00C28  app+0x000B28  9287          .byte 0x92, 0x87
0x01E00C2A  app+0x000B2A  e401          .byte 0xE4, 0x01
0x01E00C2C  app+0x000B2C  1710          r7 = b[r1++=r8] (u)
0x01E00C2E  app+0x000B2E  0000          nop
0x01E00C30  app+0x000B30  8e87          r6 = sp + 7
0x01E00C32  app+0x000B32  e401          .byte 0xE4, 0x01
0x01E00C34  app+0x000B34  0220          r2 = [sp]
0x01E00C36  app+0x000B36  0000          nop
0x01E00C38  app+0x000B38  1687          goto -1970
0x01E00C3A  app+0x000B3A  e401          .byte 0xE4, 0x01
0x01E00C3C  app+0x000B3C  1120          r1 = -32
0x01E00C3E  app+0x000B3E  0000          nop
0x01E00C40  app+0x000B40  1687          goto -1970
0x01E00C42  app+0x000B42  e401          .byte 0xE4, 0x01
0x01E00C44  app+0x000B44  1620          r6 = -32
0x01E00C46  app+0x000B46  0000          nop
0x01E00C48  app+0x000B48  5686          goto -1716
0x01E00C4A  app+0x000B4A  e401          .byte 0xE4, 0x01
0x01E00C4C  app+0x000B4C  0120          r1 = [sp]
0x01E00C4E  app+0x000B4E  0000          nop
0x01E00C50  app+0x000B50  5686          goto -1716
0x01E00C52  app+0x000B52  e401          .byte 0xE4, 0x01
0x01E00C54  app+0x000B54  1720          r7 = -32
0x01E00C56  app+0x000B56  0000          nop
0x01E00C58  app+0x000B58  f286          .byte 0xF2, 0x86
0x01E00C5A  app+0x000B5A  e401          .byte 0xE4, 0x01
0x01E00C5C  app+0x000B5C  3720          r7 |= 1
0x01E00C5E  app+0x000B5E  0000          nop
0x01E00C60  app+0x000B60  5686          goto -1716
0x01E00C62  app+0x000B62  e401          .byte 0xE4, 0x01
0x01E00C64  app+0x000B64  1920          .byte 0x19, 0x20
0x01E00C66  app+0x000B66  0000          nop
0x01E00C68  app+0x000B68  5286          .byte 0x52, 0x86
0x01E00C6A  app+0x000B6A  e401          .byte 0xE4, 0x01
0x01E00C6C  app+0x000B6C  1410          r4 = b[r1++=r8] (u)
0x01E00C6E  app+0x000B6E  0000          nop
0x01E00C70  app+0x000B70  4e86          r6 = r4 + 6
0x01E00C72  app+0x000B72  e401          .byte 0xE4, 0x01
0x01E00C74  app+0x000B74  1610          r6 = b[r1++=r8] (u)
0x01E00C76  app+0x000B76  0000          nop
0x01E00C78  app+0x000B78  4a86          r2 = r4 + 6
0x01E00C7A  app+0x000B7A  e401          .byte 0xE4, 0x01
0x01E00C7C  app+0x000B7C  0c10          b[r0++=r8] = r4
0x01E00C7E  app+0x000B7E  0000          nop
0x01E00C80  app+0x000B80  4686          goto -1780
0x01E00C82  app+0x000B82  e401          .byte 0xE4, 0x01
0x01E00C84  app+0x000B84  0111          r1 = b[r0++=r10] (u)
0x01E00C86  app+0x000B86  0000          nop
0x01E00C88  app+0x000B88  ca85          r2 = sp + 69
0x01E00C8A  app+0x000B8A  e401          .byte 0xE4, 0x01
0x01E00C8C  app+0x000B8C  0611          r6 = b[r0++=r10] (u)
0x01E00C8E  app+0x000B8E  0000          nop
0x01E00C90  app+0x000B90  1285          .byte 0x12, 0x85
0x01E00C92  app+0x000B92  e401          .byte 0xE4, 0x01
0x01E00C94  app+0x000B94  0211          r2 = b[r0++=r10] (u)
0x01E00C96  app+0x000B96  0000          nop
0x01E00C98  app+0x000B98  ca85          r2 = sp + 69
0x01E00C9A  app+0x000B9A  e401          .byte 0xE4, 0x01
0x01E00C9C  app+0x000B9C  0511          r5 = b[r0++=r10] (u)
0x01E00C9E  app+0x000B9E  0000          nop
0x01E00CA0  app+0x000BA0  1285          .byte 0x12, 0x85
0x01E00CA2  app+0x000BA2  e401          .byte 0xE4, 0x01
0x01E00CA4  app+0x000BA4  3000          .byte 0x30, 0x00
0x01E00CA6  app+0x000BA6  0000          nop
0x01E00CA8  app+0x000BA8  b684          goto -1336
0x01E00CAA  app+0x000BAA  e401          .byte 0xE4, 0x01
0x01E00CAC  app+0x000BAC  2800          .byte 0x28, 0x00
0x01E00CAE  app+0x000BAE  0000          nop
0x01E00CB0  app+0x000BB0  a284          sp += -368
0x01E00CB2  app+0x000BB2  e401          .byte 0xE4, 0x01
0x01E00CB4  app+0x000BB4  0620          r6 = [sp]
0x01E00CB6  app+0x000BB6  0000          nop
0x01E00CB8  app+0x000BB8  9e84          .byte 0x9E, 0x84
0x01E00CBA  app+0x000BBA  e401          .byte 0xE4, 0x01
0x01E00CBC  app+0x000BBC  3420          r4 |= 1
0x01E00CBE  app+0x000BBE  0000          nop
0x01E00CC0  app+0x000BC0  9a84          .byte 0x9A, 0x84
0x01E00CC2  app+0x000BC2  e401          .byte 0xE4, 0x01
0x01E00CC4  app+0x000BC4  3520          r5 |= 1
0x01E00CC6  app+0x000BC6  0000          nop
0x01E00CC8  app+0x000BC8  9684          goto -1464
0x01E00CCA  app+0x000BCA  e401          .byte 0xE4, 0x01
0x01E00CCC  app+0x000BCC  0500          .byte 0x05, 0x00
0x01E00CCE  app+0x000BCE  0000          nop
0x01E00CD0  app+0x000BD0  6684          goto -1656
0x01E00CD2  app+0x000BD2  e401          .byte 0xE4, 0x01
0x01E00CD4  app+0x000BD4  7e04          [--sp] = {rets, r14-r4}
0x01E00CD6  app+0x000BD6  50ed06e0      r14 = h[r0+6] (u)
0x01E00CDA  app+0x000BDA  54ed0440      r4 = h[r0+4] (s)
0x01E00CDE  app+0x000BDE  3af1ff4f      r10 = r4 + -1  #
0x01E00CE2  app+0x000BE2  0563          r5 = [r0+12]
0x01E00CE4  app+0x000BE4  54ed0860      r6 = h[r0+8] (s)
0x01E00CE8  app+0x000BE8  d0ec0090      r9 = [r0+0]
0x01E00CEC  app+0x000BEC  c0e151b0      r11 = r5 << 1
0x01E00CF0  app+0x000BF0  66a2          r6 = r6 << 2
0x01E00CF2  app+0x000BF2  48e1006b      r8 = r6 | 0x20000
0x01E00CF6  app+0x000BF6  4e28          r6 = 40
0x01E00CF8  app+0x000BF8  9416          r4 = r9
0x01E00CFA  app+0x000BFA  1516          r5 = r1
0x01E00CFC  app+0x000BFC  e8e858d4      r13_r12 = h[r5 ++= r8.h]*[r4 ++= r8.l] (s)
0x01E00D00  app+0x000C00  a716          r7 = r10
0x01E00D02  app+0x000C02  1703          rep 4 r7 {
0x01E00D04  app+0x000C04  ece858d4      r13_r12 += h[r5 ++= r8.h]*[r4 ++= r8.l] (s)
0x01E00D08  app+0x000C08  f75c          if (r7 != 0) goto -8
0x01E00D0A  app+0x000C0A  dfe1c13e      r3 = r13_r12 >> r14 (s)
0x01E00D0E  app+0x000C0E  78e00133      r3 = sat16(r3) (s)
0x01E00D12  app+0x000C12  a306          h[r2++=2] = r3
0x01E00D14  app+0x000C14  b118          r1 += r11
0x01E00D16  app+0x000C16  06eaefff      if (--r6 != 0) goto -34
0x01E00D1A  app+0x000C1A  50ed07e0      h[r0+6] = r14
0x01E00D1E  app+0x000C1E  5e04          {pc, r14-r4} = [sp++]
0x01E00D20  app+0x000C20  7504          [--sp] = {rets, r5, r4}
0x01E00D22  app+0x000C22  4420          r4 = 0
0x01E00D24  app+0x000C24  4520          r5 = 0
0x01E00D26  app+0x000C26  3203          rep 8 r2 {
0x01E00D28  app+0x000C28  d4ed1230      r3 = h[r1++=2] (s)
0x01E00D2C  app+0x000C2C  ecee0153      r5_r4 += h[r0 ++= 2]*r3 (s)
0x01E00D30  app+0x000C30  f25a          if (r2 != 0) goto -12
0x01E00D32  app+0x000C32  4121          r1 = 1
0x01E00D34  app+0x000C34  dfe14101      r0 = r5_r4 >> r1 (s)
0x01E00D38  app+0x000C38  5504          {pc, r5, r4} = [sp++]
0x01E00D3A  app+0x000C3A  7b04          [--sp] = {rets, r11-r4}
0x01E00D3C  app+0x000C3C  48e00a00      r8 = 10
0x01E00D40  app+0x000C40  452c          r5 = 12
0x01E00D42  app+0x000C42  f7e00220      r7 = r2 - 0x2
0x01E00D46  app+0x000C46  0416          r4 = r0
0x01E00D48  app+0x000C48  d4ed1290      r9 = h[r1++=2] (s)
0x01E00D4C  app+0x000C4C  e8ee41b9      r11_r10 = h[r4 ++= 2]*r9 (s)
0x01E00D50  app+0x000C50  8616          r6 = r8
0x01E00D52  app+0x000C52  3603          rep 8 r6 {
0x01E00D54  app+0x000C54  d7ed7e9f      r9 = h[r7++=-2] (s)
0x01E00D58  app+0x000C58  eeee41b9      r11_r10 -= h[r4 ++= 2]*r9 (s)
0x01E00D5C  app+0x000C5C  f65a          if (r6 != 0) goto -12
0x01E00D5E  app+0x000C5E  dfe1a195      r9 = r11_r10 >> r5 (s)
0x01E00D62  app+0x000C62  78e00199      r9 = sat16(r9) (s)
0x01E00D66  app+0x000C66  d0ed2390      h[r2++=2] = r9
0x01E00D6A  app+0x000C6A  03eaeaff      if (--r3 != 0) goto -44
0x01E00D6E  app+0x000C6E  5b04          {pc, r11-r4} = [sp++]
0x01E00D70  app+0x000C70  7b04          [--sp] = {rets, r11-r4}
0x01E00D72  app+0x000C72  48e00a00      r8 = 10
0x01E00D76  app+0x000C76  49e02800      r9 = 40
0x01E00D7A  app+0x000C7A  452c          r5 = 12
0x01E00D7C  app+0x000C7C  f7e00210      r7 = r1 - 0x2
0x01E00D80  app+0x000C80  0316          r3 = r0
0x01E00D82  app+0x000C82  d4ed1240      r4 = h[r1++=2] (s)
0x01E00D86  app+0x000C86  e8ee31b4      r11_r10 = h[r3 ++= 2]*r4 (s)
0x01E00D8A  app+0x000C8A  8616          r6 = r8
0x01E00D8C  app+0x000C8C  3603          rep 8 r6 {
0x01E00D8E  app+0x000C8E  d7ed7e4f      r4 = h[r7++=-2] (s)
0x01E00D92  app+0x000C92  ecee31b4      r11_r10 += h[r3 ++= 2]*r4 (s)
0x01E00D96  app+0x000C96  f65a          if (r6 != 0) goto -12
0x01E00D98  app+0x000C98  dfe1a145      r4 = r11_r10 >> r5 (s)
0x01E00D9C  app+0x000C9C  78e00144      r4 = sat16(r4) (s)
0x01E00DA0  app+0x000CA0  a406          h[r2++=2] = r4
0x01E00DA2  app+0x000CA2  09eaebff      if (--r9 != 0) goto -42
0x01E00DA6  app+0x000CA6  5b04          {pc, r11-r4} = [sp++]
0x01E00DA8  app+0x000CA8  7b04          [--sp] = {rets, r11-r4}
0x01E00DAA  app+0x000CAA  50ec0080      r9_r8 = d[r0+0]
0x01E00DAE  app+0x000CAE  c028          r0 += 8
0x01E00DB0  app+0x000CB0  4d28          r5 = 40
0x01E00DB2  app+0x000CB2  d4ed1260      r6 = h[r1++=2] (s)
0x01E00DB6  app+0x000CB6  0716          r7 = r0
0x01E00DB8  app+0x000CB8  8316          r3 = r8
0x01E00DBA  app+0x000CBA  e0ee31b6      r11_r10 = [r3 ++= 4]*r6 (s)
0x01E00DBE  app+0x000CBE  1083          rep 4 4 {
0x01E00DC0  app+0x000CC0  e1e971b3      r11_r10 += [r7 ++= 4]*[r3 ++= 4] (s)
0x01E00DC4  app+0x000CC4  0460          r4 = [r0+0]
0x01E00DC6  app+0x000CC6  8660          [r0+0] = r6
0x01E00DC8  app+0x000CC8  8461          [r0+4] = r4
0x01E00DCA  app+0x000CCA  dfe1a149      r4 = r11_r10 >> r9 (s)
0x01E00DCE  app+0x000CCE  0662          r6 = [r0+8]
0x01E00DD0  app+0x000CD0  8462          [r0+8] = r4
0x01E00DD2  app+0x000CD2  8663          [r0+12] = r6
0x01E00DD4  app+0x000CD4  78e00144      r4 = sat16(r4) (s)
0x01E00DD8  app+0x000CD8  a406          h[r2++=2] = r4
0x01E00DDA  app+0x000CDA  05eaeaff      if (--r5 != 0) goto -44
0x01E00DDE  app+0x000CDE  5b04          {pc, r11-r4} = [sp++]
0x01E00DE0  app+0x000CE0  5555          if (r5 == 0) goto -150
0x01E00DE2  app+0x000CE2  0500          .byte 0x05, 0x00
0x01E00DE4  app+0x000CE4  0000          nop
0x01E00DE6  app+0x000CE6  1000          .byte 0x10, 0x00
0x01E00DE8  app+0x000CE8  abaa          r3 = r2 >>> 10
0x01E00DEA  app+0x000CEA  1a00          .byte 0x1A, 0x00
0x01E00DEC  app+0x000CEC  5555          if (r5 == 0) goto -150
0x01E00DEE  app+0x000CEE  2500          .byte 0x25, 0x00
0x01E00DF0  app+0x000CF0  0000          nop
0x01E00DF2  app+0x000CF2  3000          .byte 0x30, 0x00
0x01E00DF4  app+0x000CF4  abaa          r3 = r2 >>> 10
0x01E00DF6  app+0x000CF6  3a00          ssync
0x01E00DF8  app+0x000CF8  5555          if (r5 == 0) goto -150
0x01E00DFA  app+0x000CFA  4500          .byte 0x45, 0x00
0x01E00DFC  app+0x000CFC  0000          nop
0x01E00DFE  app+0x000CFE  5000          .byte 0x50, 0x00
0x01E00E00  app+0x000D00  abaa          r3 = r2 >>> 10
0x01E00E02  app+0x000D02  5a00          .byte 0x5A, 0x00
0x01E00E04  app+0x000D04  5555          if (r5 == 0) goto -150
0x01E00E06  app+0x000D06  6500          .byte 0x65, 0x00
0x01E00E08  app+0x000D08  0000          nop
0x01E00E0A  app+0x000D0A  7000          .byte 0x70, 0x00
0x01E00E0C  app+0x000D0C  abaa          r3 = r2 >>> 10
0x01E00E0E  app+0x000D0E  7a00          .byte 0x7A, 0x00
0x01E00E10  app+0x000D10  5555          if (r5 == 0) goto -150
0x01E00E12  app+0x000D12  8500          rtns
0x01E00E14  app+0x000D14  0000          nop
0x01E00E16  app+0x000D16  9000          callns r0
0x01E00E18  app+0x000D18  abaa          r3 = r2 >>> 10
0x01E00E1A  app+0x000D1A  9a00          callns r10
0x01E00E1C  app+0x000D1C  5555          if (r5 == 0) goto -150
0x01E00E1E  app+0x000D1E  a500          swi 5
0x01E00E20  app+0x000D20  0000          nop
0x01E00E22  app+0x000D22  b000          testset b[r0]
0x01E00E24  app+0x000D24  abaa          r3 = r2 >>> 10
0x01E00E26  app+0x000D26  ba00          testset b[r10]
0x01E00E28  app+0x000D28  5555          if (r5 == 0) goto -150
0x01E00E2A  app+0x000D2A  c500          call r5
0x01E00E2C  app+0x000D2C  0000          nop
0x01E00E2E  app+0x000D2E  d000          goto r0
0x01E00E30  app+0x000D30  abaa          r3 = r2 >>> 10
0x01E00E32  app+0x000D32  da00          goto r10
0x01E00E34  app+0x000D34  5555          if (r5 == 0) goto -150
0x01E00E36  app+0x000D36  e500          cli r5
0x01E00E38  app+0x000D38  0000          nop
0x01E00E3A  app+0x000D3A  f000          sti r0
0x01E00E3C  app+0x000D3C  abaa          r3 = r2 >>> 10
0x01E00E3E  app+0x000D3E  fa00          sti r10
0x01E00E40  app+0x000D40  5555          if (r5 == 0) goto -150
0x01E00E42  app+0x000D42  0500          .byte 0x05, 0x00
0x01E00E44  app+0x000D44  0000          nop
0x01E00E46  app+0x000D46  1000          .byte 0x10, 0x00
0x01E00E48  app+0x000D48  abaa          r3 = r2 >>> 10
0x01E00E4A  app+0x000D4A  1a00          .byte 0x1A, 0x00
0x01E00E4C  app+0x000D4C  5555          if (r5 == 0) goto -150
0x01E00E4E  app+0x000D4E  2500          .byte 0x25, 0x00
0x01E00E50  app+0x000D50  0000          nop
0x01E00E52  app+0x000D52  3000          .byte 0x30, 0x00
0x01E00E54  app+0x000D54  abaa          r3 = r2 >>> 10
0x01E00E56  app+0x000D56  3a00          ssync
0x01E00E58  app+0x000D58  5555          if (r5 == 0) goto -150
0x01E00E5A  app+0x000D5A  4500          .byte 0x45, 0x00
0x01E00E5C  app+0x000D5C  0000          nop
0x01E00E5E  app+0x000D5E  5000          .byte 0x50, 0x00
0x01E00E60  app+0x000D60  abaa          r3 = r2 >>> 10
0x01E00E62  app+0x000D62  5a00          .byte 0x5A, 0x00
0x01E00E64  app+0x000D64  5555          if (r5 == 0) goto -150
0x01E00E66  app+0x000D66  6500          .byte 0x65, 0x00
0x01E00E68  app+0x000D68  0000          nop
0x01E00E6A  app+0x000D6A  7000          .byte 0x70, 0x00
0x01E00E6C  app+0x000D6C  abaa          r3 = r2 >>> 10
0x01E00E6E  app+0x000D6E  7a00          .byte 0x7A, 0x00
0x01E00E70  app+0x000D70  5555          if (r5 == 0) goto -150
0x01E00E72  app+0x000D72  8500          rtns
0x01E00E74  app+0x000D74  0000          nop
0x01E00E76  app+0x000D76  9000          callns r0
0x01E00E78  app+0x000D78  abaa          r3 = r2 >>> 10
0x01E00E7A  app+0x000D7A  9a00          callns r10
0x01E00E7C  app+0x000D7C  5555          if (r5 == 0) goto -150
0x01E00E7E  app+0x000D7E  a500          swi 5
0x01E00E80  app+0x000D80  0000          nop
0x01E00E82  app+0x000D82  b000          testset b[r0]
0x01E00E84  app+0x000D84  abaa          r3 = r2 >>> 10
0x01E00E86  app+0x000D86  ba00          testset b[r10]
0x01E00E88  app+0x000D88  5555          if (r5 == 0) goto -150
0x01E00E8A  app+0x000D8A  c500          call r5
0x01E00E8C  app+0x000D8C  0000          nop
0x01E00E8E  app+0x000D8E  d000          goto r0
0x01E00E90  app+0x000D90  abaa          r3 = r2 >>> 10
0x01E00E92  app+0x000D92  da00          goto r10
0x01E00E94  app+0x000D94  5555          if (r5 == 0) goto -150
0x01E00E96  app+0x000D96  e500          cli r5
0x01E00E98  app+0x000D98  0000          nop
0x01E00E9A  app+0x000D9A  f000          sti r0
0x01E00E9C  app+0x000D9C  abaa          r3 = r2 >>> 10
0x01E00E9E  app+0x000D9E  fa00          sti r10
0x01E00EA0  app+0x000DA0  5555          if (r5 == 0) goto -150
0x01E00EA2  app+0x000DA2  0500          .byte 0x05, 0x00
0x01E00EA4  app+0x000DA4  0000          nop
0x01E00EA6  app+0x000DA6  1000          .byte 0x10, 0x00
0x01E00EA8  app+0x000DA8  abaa          r3 = r2 >>> 10
0x01E00EAA  app+0x000DAA  1a00          .byte 0x1A, 0x00
0x01E00EAC  app+0x000DAC  5555          if (r5 == 0) goto -150
0x01E00EAE  app+0x000DAE  2500          .byte 0x25, 0x00
0x01E00EB0  app+0x000DB0  0000          nop
0x01E00EB2  app+0x000DB2  3000          .byte 0x30, 0x00
0x01E00EB4  app+0x000DB4  abaa          r3 = r2 >>> 10
0x01E00EB6  app+0x000DB6  3a00          ssync
0x01E00EB8  app+0x000DB8  5555          if (r5 == 0) goto -150
0x01E00EBA  app+0x000DBA  4500          .byte 0x45, 0x00
0x01E00EBC  app+0x000DBC  0000          nop
0x01E00EBE  app+0x000DBE  5000          .byte 0x50, 0x00
0x01E00EC0  app+0x000DC0  abaa          r3 = r2 >>> 10
0x01E00EC2  app+0x000DC2  5a00          .byte 0x5A, 0x00
0x01E00EC4  app+0x000DC4  5555          if (r5 == 0) goto -150
0x01E00EC6  app+0x000DC6  6500          .byte 0x65, 0x00
0x01E00EC8  app+0x000DC8  0000          nop
0x01E00ECA  app+0x000DCA  7000          .byte 0x70, 0x00
0x01E00ECC  app+0x000DCC  abaa          r3 = r2 >>> 10
0x01E00ECE  app+0x000DCE  7a00          .byte 0x7A, 0x00
0x01E00ED0  app+0x000DD0  5555          if (r5 == 0) goto -150
0x01E00ED2  app+0x000DD2  8500          rtns
0x01E00ED4  app+0x000DD4  0000          nop
0x01E00ED6  app+0x000DD6  9000          callns r0
0x01E00ED8  app+0x000DD8  abaa          r3 = r2 >>> 10
0x01E00EDA  app+0x000DDA  9a00          callns r10
0x01E00EDC  app+0x000DDC  5555          if (r5 == 0) goto -150
0x01E00EDE  app+0x000DDE  a500          swi 5
0x01E00EE0  app+0x000DE0  0000          nop
0x01E00EE2  app+0x000DE2  b000          testset b[r0]
0x01E00EE4  app+0x000DE4  abaa          r3 = r2 >>> 10
0x01E00EE6  app+0x000DE6  ba00          testset b[r10]
0x01E00EE8  app+0x000DE8  5555          if (r5 == 0) goto -150
0x01E00EEA  app+0x000DEA  c500          call r5
0x01E00EEC  app+0x000DEC  0000          nop
0x01E00EEE  app+0x000DEE  d000          goto r0
0x01E00EF0  app+0x000DF0  abaa          r3 = r2 >>> 10
0x01E00EF2  app+0x000DF2  da00          goto r10
0x01E00EF4  app+0x000DF4  5555          if (r5 == 0) goto -150
0x01E00EF6  app+0x000DF6  e500          cli r5
0x01E00EF8  app+0x000DF8  0000          nop
0x01E00EFA  app+0x000DFA  f000          sti r0
0x01E00EFC  app+0x000DFC  abaa          r3 = r2 >>> 10
0x01E00EFE  app+0x000DFE  fa00          sti r10
0x01E00F00  app+0x000E00  5555          if (r5 == 0) goto -150
0x01E00F02  app+0x000E02  0500          .byte 0x05, 0x00
0x01E00F04  app+0x000E04  0000          nop
0x01E00F06  app+0x000E06  1000          .byte 0x10, 0x00
0x01E00F08  app+0x000E08  abaa          r3 = r2 >>> 10
0x01E00F0A  app+0x000E0A  1a00          .byte 0x1A, 0x00
0x01E00F0C  app+0x000E0C  5555          if (r5 == 0) goto -150
0x01E00F0E  app+0x000E0E  2500          .byte 0x25, 0x00
0x01E00F10  app+0x000E10  0000          nop
0x01E00F12  app+0x000E12  3000          .byte 0x30, 0x00
0x01E00F14  app+0x000E14  abaa          r3 = r2 >>> 10
0x01E00F16  app+0x000E16  3a00          ssync
0x01E00F18  app+0x000E18  5555          if (r5 == 0) goto -150
0x01E00F1A  app+0x000E1A  4500          .byte 0x45, 0x00
0x01E00F1C  app+0x000E1C  0000          nop
0x01E00F1E  app+0x000E1E  5000          .byte 0x50, 0x00
0x01E00F20  app+0x000E20  abaa          r3 = r2 >>> 10
0x01E00F22  app+0x000E22  5a00          .byte 0x5A, 0x00
0x01E00F24  app+0x000E24  0000          nop
0x01E00F26  app+0x000E26  0000          nop
0x01E00F28  app+0x000E28  0000          nop
0x01E00F2A  app+0x000E2A  0000          nop
0x01E00F2C  app+0x000E2C  6464          r4 = [r6+16]
0x01E00F2E  app+0x000E2E  6464          r4 = [r6+16]
0x01E00F30  app+0x000E30  6464          r4 = [r6+16]
0x01E00F32  app+0x000E32  6464          r4 = [r6+16]
0x01E00F34  app+0x000E34  6464          r4 = [r6+16]
0x01E00F36  app+0x000E36  6464          r4 = [r6+16]
0x01E00F38  app+0x000E38  6464          r4 = [r6+16]
0x01E00F3A  app+0x000E3A  6464          r4 = [r6+16]
0x01E00F3C  app+0x000E3C  6464          r4 = [r6+16]
0x01E00F3E  app+0x000E3E  6464          r4 = [r6+16]
0x01E00F40  app+0x000E40  6464          r4 = [r6+16]
0x01E00F42  app+0x000E42  6464          r4 = [r6+16]
0x01E00F44  app+0x000E44  6565          r5 = [r6+20]
0x01E00F46  app+0x000E46  6565          r5 = [r6+20]
0x01E00F48  app+0x000E48  6565          r5 = [r6+20]
0x01E00F4A  app+0x000E4A  6565          r5 = [r6+20]
0x01E00F4C  app+0x000E4C  6565          r5 = [r6+20]
0x01E00F4E  app+0x000E4E  6565          r5 = [r6+20]
0x01E00F50  app+0x000E50  6565          r5 = [r6+20]
0x01E00F52  app+0x000E52  6565          r5 = [r6+20]
0x01E00F54  app+0x000E54  6565          r5 = [r6+20]
0x01E00F56  app+0x000E56  6565          r5 = [r6+20]
0x01E00F58  app+0x000E58  6565          r5 = [r6+20]
0x01E00F5A  app+0x000E5A  6565          r5 = [r6+20]
0x01E00F5C  app+0x000E5C  6666          r6 = [r6+24]
0x01E00F5E  app+0x000E5E  6666          r6 = [r6+24]
0x01E00F60  app+0x000E60  6666          r6 = [r6+24]
0x01E00F62  app+0x000E62  6666          r6 = [r6+24]
0x01E00F64  app+0x000E64  6666          r6 = [r6+24]
0x01E00F66  app+0x000E66  6666          r6 = [r6+24]
0x01E00F68  app+0x000E68  6666          r6 = [r6+24]
0x01E00F6A  app+0x000E6A  6666          r6 = [r6+24]
0x01E00F6C  app+0x000E6C  6666          r6 = [r6+24]
0x01E00F6E  app+0x000E6E  6666          r6 = [r6+24]
0x01E00F70  app+0x000E70  6666          r6 = [r6+24]
0x01E00F72  app+0x000E72  6666          r6 = [r6+24]
0x01E00F74  app+0x000E74  6767          r7 = [r6+28]
0x01E00F76  app+0x000E76  6767          r7 = [r6+28]
0x01E00F78  app+0x000E78  6767          r7 = [r6+28]
0x01E00F7A  app+0x000E7A  6767          r7 = [r6+28]
0x01E00F7C  app+0x000E7C  6700          .byte 0x67, 0x00
0x01E00F7E  app+0x000E7E  0000          nop
0x01E00F80  app+0x000E80  d922          r1 += 98
0x01E00F82  app+0x000E82  7000          .byte 0x70, 0x00
0x01E00F84  app+0x000E84  d922          r1 += 98
0x01E00F86  app+0x000E86  7000          .byte 0x70, 0x00
0x01E00F88  app+0x000E88  d922          r1 += 98
0x01E00F8A  app+0x000E8A  7000          .byte 0x70, 0x00
0x01E00F8C  app+0x000E8C  d922          r1 += 98
0x01E00F8E  app+0x000E8E  7000          .byte 0x70, 0x00
0x01E00F90  app+0x000E90  d922          r1 += 98
0x01E00F92  app+0x000E92  7000          .byte 0x70, 0x00
0x01E00F94  app+0x000E94  7f20          r7 = 224
0x01E00F96  app+0x000E96  7000          .byte 0x70, 0x00
0x01E00F98  app+0x000E98  7f20          r7 = 224
0x01E00F9A  app+0x000E9A  7000          .byte 0x70, 0x00
0x01E00F9C  app+0x000E9C  7f20          r7 = 224
0x01E00F9E  app+0x000E9E  7000          .byte 0x70, 0x00
0x01E00FA0  app+0x000EA0  7f20          r7 = 224
0x01E00FA2  app+0x000EA2  7000          .byte 0x70, 0x00
0x01E00FA4  app+0x000EA4  7f22          r7 = 226
0x01E00FA6  app+0x000EA6  7000          .byte 0x70, 0x00
0x01E00FA8  app+0x000EA8  c20f          r2 = h[r4++=r15] (u)
0x01E00FAA  app+0x000EAA  f000          sti r0
0x01E00FAC  app+0x000EAC  c23f          r2 += 31
0x01E00FAE  app+0x000EAE  f000          sti r0
0x01E00FB0  app+0x000EB0  c2fff000c2ff  r2 = 4290904304
0x01E00FB6  app+0x000EB6  f300          sti r3
0x01E00FB8  app+0x000EB8  c2ffff00f20f  r2 = 267518207
0x01E00FBE  app+0x000EBE  f000          sti r0
0x01E00FC0  app+0x000EC0  f23f          r2 += -33
0x01E00FC2  app+0x000EC2  f000          sti r0
0x01E00FC4  app+0x000EC4  f2ff          .byte 0xF2, 0xFF
0x01E00FC6  app+0x000EC6  f000          sti r0
0x01E00FC8  app+0x000EC8  f2ff          .byte 0xF2, 0xFF
0x01E00FCA  app+0x000ECA  f300          sti r3
0x01E00FCC  app+0x000ECC  f2ff          .byte 0xF2, 0xFF
0x01E00FCE  app+0x000ECE  ff00          sti r15
0x01E00FD0  app+0x000ED0  1f00          .byte 0x1F, 0x00
0x01E00FD2  app+0x000ED2  0000          nop
0x01E00FD4  app+0x000ED4  0000          nop
0x01E00FD6  app+0x000ED6  0000          nop
0x01E00FD8  app+0x000ED8  e007          b[r6++=1] = r0
0x01E00FDA  app+0x000EDA  0000          nop
0x01E00FDC  app+0x000EDC  0000          nop
0x01E00FDE  app+0x000EDE  0000          nop
0x01E00FE0  app+0x000EE0  00f8ffff      if (r0 == 127) goto -2
0x01E00FE4  app+0x000EE4  ffff          .byte 0xFF, 0xFF
0x01E00FE6  app+0x000EE6  0000          nop
0x01E00FE8  app+0x000EE8  0782          goto -1020
0x01E00FEA  app+0x000EEA  2489          goto 146
0x01E00FEC  app+0x000EEC  a22c          [sp+176] = r2
0x01E00FEE  app+0x000EEE  0000          nop
0x01E00FF0  app+0x000EF0  0341          if (r3 == 0) goto 2
0x01E00FF2  app+0x000EF2  1485          goto 74
0x01E00FF4  app+0x000EF4  611c          r1 = r6 + r0
0x01E00FF6  app+0x000EF6  0000          nop
0x01E00FF8  app+0x000EF8  4010          r0 = b[r4++=r8] (u)
0x01E00FFA  app+0x000EFA  0482          goto 4
0x01E00FFC  app+0x000EFC  200c          r0 = h[r2++=r8] (u)
0x01E00FFE  app+0x000EFE  0000          nop
0x01E01000  app+0x000F00  beff          .byte 0xBE, 0xFF
0x01E01002  app+0x000F02  0300          hbkpt
0x01E01004  app+0x000F04  0000          nop
0x01E01006  app+0x000F06  0000          nop
0x01E01008  app+0x000F08  b9ae          r1 = r3 >>> 14
0x01E0100A  app+0x000F0A  ef3b          r7 += -69
0x01E0100C  app+0x000F0C  cff7          .byte 0xCF, 0xF7
0x01E0100E  app+0x000F0E  0000          nop
0x01E01010  app+0x000F10  b56d          [r3+52] = r5
0x01E01012  app+0x000F12  df37          r7 += 119
0x01E01014  app+0x000F14  8ee7          .byte 0x8E, 0xE7
0x01E01016  app+0x000F16  0000          nop
0x01E01018  app+0x000F18  b12c          .byte 0xB1, 0x2C
0x01E0101A  app+0x000F1A  cf33          r7 += 51
0x01E0101C  app+0x000F1C  4dd7          r5 = r4.b0 (s)  #
0x01E0101E  app+0x000F1E  0000          nop
0x01E01020  app+0x000F20  0882          r0 = r0 + 2
0x01E01022  app+0x000F22  2008          r0 = [r2++=r8]
0x01E01024  app+0x000F24  8220          [sp] = r2
0x01E01026  app+0x000F26  0000          nop
0x01E01028  app+0x000F28  0882          r0 = r0 + 2
0x01E0102A  app+0x000F2A  2008          r0 = [r2++=r8]
0x01E0102C  app+0x000F2C  8220          [sp] = r2
0x01E0102E  app+0x000F2E  0000          nop
0x01E01030  app+0x000F30  0882          r0 = r0 + 2
0x01E01032  app+0x000F32  2008          r0 = [r2++=r8]
0x01E01034  app+0x000F34  8220          [sp] = r2
0x01E01036  app+0x000F36  0000          nop
0x01E01038  app+0x000F38  0882          r0 = r0 + 2
0x01E0103A  app+0x000F3A  2008          r0 = [r2++=r8]
0x01E0103C  app+0x000F3C  8220          [sp] = r2
0x01E0103E  app+0x000F3E  0000          nop
0x01E01040  app+0x000F40  0882          r0 = r0 + 2
0x01E01042  app+0x000F42  2008          r0 = [r2++=r8]
0x01E01044  app+0x000F44  8220          [sp] = r2
0x01E01046  app+0x000F46  0000          nop
0x01E01048  app+0x000F48  4551          if (r5 == 0) goto -222
0x01E0104A  app+0x000F4A  1408          r4 = [r1++=r8]
0x01E0104C  app+0x000F4C  8220          [sp] = r2
0x01E0104E  app+0x000F4E  0000          nop
0x01E01050  app+0x000F50  8220          [sp] = r2
0x01E01052  app+0x000F52  0845          r0 = b[r0+5] (u)
0x01E01054  app+0x000F54  5114          .byte 0x51, 0x14
0x01E01056  app+0x000F56  0000          nop
0x01E01058  app+0x000F58  8220          [sp] = r2
0x01E0105A  app+0x000F5A  0882          r0 = r0 + 2
0x01E0105C  app+0x000F5C  2008          r0 = [r2++=r8]
0x01E0105E  app+0x000F5E  0000          nop
0x01E01060  app+0x000F60  c771          [r4+-60] = r7
0x01E01062  app+0x000F62  1cc7          .byte 0x1C, 0xC7
0x01E01064  app+0x000F64  711c          r1 = r7 + r0
0x01E01066  app+0x000F66  0000          nop
0x01E01068  app+0x000F68  c771          [r4+-60] = r7
0x01E0106A  app+0x000F6A  1cc7          .byte 0x1C, 0xC7
0x01E0106C  app+0x000F6C  711c          r1 = r7 + r0
0x01E0106E  app+0x000F6E  0000          nop
0x01E01070  app+0x000F70  c771          [r4+-60] = r7
0x01E01072  app+0x000F72  1cc7          .byte 0x1C, 0xC7
0x01E01074  app+0x000F74  711c          r1 = r7 + r0
0x01E01076  app+0x000F76  0000          nop
0x01E01078  app+0x000F78  c771          [r4+-60] = r7
0x01E0107A  app+0x000F7A  1cc7          .byte 0x1C, 0xC7
0x01E0107C  app+0x000F7C  711c          r1 = r7 + r0
0x01E0107E  app+0x000F7E  0000          nop
0x01E01080  app+0x000F80  4461          r4 = [r4+4]
0x01E01082  app+0x000F82  1cc7          .byte 0x1C, 0xC7
0x01E01084  app+0x000F84  711c          r1 = r7 + r0
0x01E01086  app+0x000F86  0000          nop
0x01E01088  app+0x000F88  4461          r4 = [r4+4]
0x01E0108A  app+0x000F8A  1c40          r4 = b[r1+0] (u)
0x01E0108C  app+0x000F8C  200c          r0 = h[r2++=r8] (u)
0x01E0108E  app+0x000F8E  0000          nop
0x01E01090  app+0x000F90  4461          r4 = [r4+4]
0x01E01092  app+0x000F92  1c40          r4 = b[r1+0] (u)
0x01E01094  app+0x000F94  200c          r0 = h[r2++=r8] (u)
0x01E01096  app+0x000F96  0000          nop
0x01E01098  app+0x000F98  0000          nop
0x01E0109A  app+0x000F9A  0040          if (r0 == 0) goto 0
0x01E0109C  app+0x000F9C  200c          r0 = h[r2++=r8] (u)
0x01E0109E  app+0x000F9E  0000          nop
0x01E010A0  app+0x000FA0  c771          [r4+-60] = r7
0x01E010A2  app+0x000FA2  1cc7          .byte 0x1C, 0xC7
0x01E010A4  app+0x000FA4  711c          r1 = r7 + r0
0x01E010A6  app+0x000FA6  0000          nop
0x01E010A8  app+0x000FA8  c771          [r4+-60] = r7
0x01E010AA  app+0x000FAA  1cc7          .byte 0x1C, 0xC7
0x01E010AC  app+0x000FAC  711c          r1 = r7 + r0
0x01E010AE  app+0x000FAE  0000          nop
0x01E010B0  app+0x000FB0  c771          [r4+-60] = r7
0x01E010B2  app+0x000FB2  1cc7          .byte 0x1C, 0xC7
0x01E010B4  app+0x000FB4  711c          r1 = r7 + r0
0x01E010B6  app+0x000FB6  0000          nop
0x01E010B8  app+0x000FB8  c771          [r4+-60] = r7
0x01E010BA  app+0x000FBA  1cc7          .byte 0x1C, 0xC7
0x01E010BC  app+0x000FBC  711c          r1 = r7 + r0
0x01E010BE  app+0x000FBE  0000          nop
0x01E010C0  app+0x000FC0  c771          [r4+-60] = r7
0x01E010C2  app+0x000FC2  1cc7          .byte 0x1C, 0xC7
0x01E010C4  app+0x000FC4  711c          r1 = r7 + r0
0x01E010C6  app+0x000FC6  0000          nop
0x01E010C8  app+0x000FC8  4551          if (r5 == 0) goto -222
0x01E010CA  app+0x000FCA  14c7          .byte 0x14, 0xC7
0x01E010CC  app+0x000FCC  711c          r1 = r7 + r0
0x01E010CE  app+0x000FCE  0000          nop
0x01E010D0  app+0x000FD0  0441          if (r4 == 0) goto 2
0x01E010D2  app+0x000FD2  1045          if (r0 == 0) goto 74
0x01E010D4  app+0x000FD4  5114          .byte 0x51, 0x14
0x01E010D6  app+0x000FD6  0000          nop
0x01E010D8  app+0x000FD8  4020          r0 = 0
0x01E010DA  app+0x000FDA  0c04          .byte 0x0C, 0x04
0x01E010DC  app+0x000FDC  4110          r1 = b[r4++=r8] (u)
0x01E010DE  app+0x000FDE  0000          nop
0x01E010E0  app+0x000FE0  a04f          if (r0 != 0) goto 158
0x01E010E2  app+0x000FE2  2b1c          r3 = r2 + r1
0x01E010E4  app+0x000FE4  0000          nop
0x01E010E6  app+0x000FE6  0000          nop
0x01E010E8  app+0x000FE8  a04f          if (r0 != 0) goto 158
0x01E010EA  app+0x000FEA  2b1c          r3 = r2 + r1
0x01E010EC  app+0x000FEC  0000          nop
0x01E010EE  app+0x000FEE  0000          nop
0x01E010F0  app+0x000FF0  a04f          if (r0 != 0) goto 158
0x01E010F2  app+0x000FF2  2b1c          r3 = r2 + r1
0x01E010F4  app+0x000FF4  0000          nop
0x01E010F6  app+0x000FF6  0000          nop
0x01E010F8  app+0x000FF8  a04f          if (r0 != 0) goto 158
0x01E010FA  app+0x000FFA  2b1c          r3 = r2 + r1
0x01E010FC  app+0x000FFC  0000          nop
0x01E010FE  app+0x000FFE  0000          nop
