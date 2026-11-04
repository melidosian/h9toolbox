; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 56; undecoded chunks: 0
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
