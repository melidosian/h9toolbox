; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 231; undecoded chunks: 0
0x01E88D00  app+0x088C00  4121          r1 = 1
0x01E88D02  app+0x088C02  beea234b      call -223674
0x01E88D06  app+0x088C06  50ed4602      r0 = h[r4+38] (u)
0x01E88D0A  app+0x088C0A  beea93f8      call -134874
0x01E88D0E  app+0x088C0E  4832          r0 = 50
0x01E88D10  app+0x088C10  8bea37c9      call 1544814
0x01E88D14  app+0x088C14  beea1668      call -208852
0x01E88D18  app+0x088C18  40e04c02      r0 = 588
0x01E88D1C  app+0x088C1C  d8ee6150      b[r6+r0] = r5
0x01E88D20  app+0x088C20  40e0eb10      r0 = 4331
0x01E88D24  app+0x088C24  d8ee6000      r0 = b[r6+r0] (u)
0x01E88D28  app+0x088C28  a04a          if (r0 != 0) goto 148
0x01E88D2A  app+0x088C2A  bfeae596      call -53814
0x01E88D2E  app+0x088C2E  2487          goto 142
0x01E88D30  app+0x088C30  beead880      call -196176
0x01E88D34  app+0x088C34  beea344c      call -223128
0x01E88D38  app+0x088C38  bfea5ac6      call -29516
0x01E88D3C  app+0x088C3C  bfeaf7f3      call -6162
0x01E88D40  app+0x088C40  bfeaeff9      call -3106
0x01E88D44  app+0x088C44  bfea5e89      call -60740
0x01E88D48  app+0x088C48  40e0eb10      r0 = 4331
0x01E88D4C  app+0x088C4C  d8ee6000      r0 = b[r6+r0] (u)
0x01E88D50  app+0x088C50  80f82902      if (r0 != 1) goto 82
0x01E88D54  app+0x088C54  bfeae896      call -53808
0x01E88D58  app+0x088C58  40e08005      r0 = 1408
0x01E88D5C  app+0x088C5C  d8ee6000      r0 = b[r6+r0] (u)
0x01E88D60  app+0x088C60  80f82102      if (r0 != 1) goto 66
0x01E88D64  app+0x088C64  50ee6305      r0 = b[r6+83] (u)
0x01E88D68  app+0x088C68  80f80502      if (r0 != 1) goto 10
0x01E88D6C  app+0x088C6C  40e05f01      r0 = 351
0x01E88D70  app+0x088C70  d8ec6be0      [r6+r0<<2] = r14
0x01E88D74  app+0x088C74  0498          goto 48
0x01E88D76  app+0x088C76  41e05f01      r1 = 351
0x01E88D7A  app+0x088C7A  d8ec6a01      r0 = [r6+r1<<2]
0x01E88D7E  app+0x088C7E  c021          r0 += 1
0x01E88D80  app+0x088C80  d8ec6b01      [r6+r1<<2] = r0
0x01E88D84  app+0x088C84  b0f90fe8      if (r0 < 500) goto 30
0x01E88D88  app+0x088C88  40e08005      r0 = 1408
0x01E88D8C  app+0x088C8C  d8ee6170      b[r6+r0] = r7
0x01E88D90  app+0x088C90  4020          r0 = 0
0x01E88D92  app+0x088C92  beea1c4a      call -224200
0x01E88D96  app+0x088C96  4022          r0 = 2
0x01E88D98  app+0x088C98  4121          r1 = 1
0x01E88D9A  app+0x088C9A  beead74a      call -223826
0x01E88D9E  app+0x088C9E  40e05f01      r0 = 351
0x01E88DA2  app+0x088CA2  d8ec6b70      [r6+r0<<2] = r7
0x01E88DA6  app+0x088CA6  50ee4a03      r0 = b[r4+58] (u)
0x01E88DAA  app+0x088CAA  beeab066      call -209568
0x01E88DAE  app+0x088CAE  50ee6b02      r0 = b[r6+43] (u)
0x01E88DB2  app+0x088CB2  0042          if (r0 == 0) goto 4
0x01E88DB4  app+0x088CB4  52ee6e72      b[r6+46] = r7
0x01E88DB8  app+0x088CB8  4021          r0 = 1
0x01E88DBA  app+0x088CBA  8beae2c8      call 1544644
0x01E88DBE  app+0x088CBE  50ee6e02      r0 = b[r6+46] (u)
0x01E88DC2  app+0x088CC2  d056          if (r0 != 0) goto -148
0x01E88DC4  app+0x088CC4  40e04c02      r0 = 588
0x01E88DC8  app+0x088CC8  d8ee6170      b[r6+r0] = r7
0x01E88DCC  app+0x088CCC  beea8180      call -196350
0x01E88DD0  app+0x088CD0  40e05405      r0 = 1364
0x01E88DD4  app+0x088CD4  d8ee6000      r0 = b[r6+r0] (u)
0x01E88DD8  app+0x088CD8  0047          if (r0 == 0) goto 14
0x01E88DDA  app+0x088CDA  402a          r0 = 10
0x01E88DDC  app+0x088CDC  beeaa6c4      call -161460
0x01E88DE0  app+0x088CE0  40e05405      r0 = 1364
0x01E88DE4  app+0x088CE4  d8ee6170      b[r6+r0] = r7
0x01E88DE8  app+0x088CE8  4024          r0 = 4
0x01E88DEA  app+0x088CEA  41f0211f      r1 = 7969  #
0x01E88DEE  app+0x088CEE  0225          r2 = [sp+20]
0x01E88DF0  app+0x088CF0  4321          r3 = 1
0x01E88DF2  app+0x088CF2  beea5e4f      call -221508
0x01E88DF6  app+0x088CF6  50ee400c      r0 = b[r4+192] (u)
0x01E88DFA  app+0x088CFA  30e80100      if (r0 == 1) {
0x01E88DFE  app+0x088CFE  bfea2a41      call -97708
0x01E88E02  app+0x088D02  beea4b84      call -194410
0x01E88E06  app+0x088D06  52ee4be0      b[r4+11] = r14
0x01E88E0A  app+0x088D0A  52ee6075      b[r6+80] = r7
0x01E88E0E  app+0x088D0E  52ee6375      b[r6+83] = r7
0x01E88E12  app+0x088D12  beead083      call -194656
0x01E88E16  app+0x088D16  50ee6b02      r0 = b[r6+43] (u)
0x01E88E1A  app+0x088D1A  8054          if (r0 != 0) goto 40
0x01E88E1C  app+0x088D1C  beea9267      call -209116
0x01E88E20  app+0x088D20  752e          r5 = 206
0x01E88E22  app+0x088D22  0484          goto 8
0x01E88E24  app+0x088D24  4021          r0 = 1
0x01E88E26  app+0x088D26  8beaacc8      call 1544536
0x01E88E2A  app+0x088D2A  c521          r5 += 1
0x01E88E2C  app+0x088D2C  60ffff500900  if ((r5 & 0xFF) == 0) goto 18
0x01E88E32  app+0x088D32  beeab54b      call -223382
0x01E88E36  app+0x088D36  bfea3592      call -56214
0x01E88E3A  app+0x088D3A  40e0c402      r0 = 708
0x01E88E3E  app+0x088D3E  d8ee4000      r0 = b[r4+r0] (u)
0x01E88E42  app+0x088D42  7050          if (r0 == 0) goto -32
0x01E88E44  app+0x088D44  40e04c02      r0 = 588
0x01E88E48  app+0x088D48  d8ee61e0      b[r6+r0] = r14
0x01E88E4C  app+0x088D4C  4021          r0 = 1
0x01E88E4E  app+0x088D4E  beeac982      call -195182
0x01E88E52  app+0x088D52  50ee6801      r0 = b[r6+24] (u)
0x01E88E56  app+0x088D56  00ff00006c01  if (r0 == 0) goto 728
0x01E88E5C  app+0x088D5C  40e0f001      r0 = 496
0x01E88E60  app+0x088D60  d8ee6000      r0 = b[r6+r0] (u)
0x01E88E64  app+0x088D64  4120          r1 = 0
0x01E88E66  app+0x088D66  8127          [sp+28] = r1
0x01E88E68  app+0x088D68  9059          if (r0 != 0) goto 114
0x01E88E6A  app+0x088D6A  40e0f001      r0 = 496
0x01E88E6E  app+0x088D6E  d8ee61e0      b[r6+r0] = r14
0x01E88E72  app+0x088D72  5028          r0 = 72
0x01E88E74  app+0x088D74  beeac33b      call -231546
0x01E88E78  app+0x088D78  8027          [sp+28] = r0
0x01E88E7A  app+0x088D7A  4020          r0 = 0
0x01E88E7C  app+0x088D7C  beeab282      call -195228
0x01E88E80  app+0x088D80  5028          r0 = 72
0x01E88E82  app+0x088D82  4120          r1 = 0
0x01E88E84  app+0x088D84  beea413c      call -231294
0x01E88E88  app+0x088D88  4021          r0 = 1
0x01E88E8A  app+0x088D8A  4120          r1 = 0
0x01E88E8C  app+0x088D8C  beea5e4a      call -224068
0x01E88E90  app+0x088D90  4034          r0 = 20
0x01E88E92  app+0x088D92  beea1844      call -227280
0x01E88E96  app+0x088D96  d8ee600c      r0 = b[r6+r12] (u)
0x01E88E9A  app+0x088D9A  30e80200      if (r0 == 2) {
0x01E88E9E  app+0x088D9E  beea0782      call -195570
0x01E88EA2  app+0x088DA2  4022          r0 = 2
0x01E88EA4  app+0x088DA4  beea9e82      call -195268
0x01E88EA8  app+0x088DA8  5830          r0 = 112
0x01E88EAA  app+0x088DAA  60e0001c      r1 = 0x8000
0x01E88EAE  app+0x088DAE  beea2c3c      call -231336
0x01E88EB2  app+0x088DB2  beeabd8d      call -189574
0x01E88EB6  app+0x088DB6  4830          r0 = 48
0x01E88EB8  app+0x088DB8  4120          r1 = 0
0x01E88EBA  app+0x088DBA  beea263c      call -231348
0x01E88EBE  app+0x088DBE  4830          r0 = 48
0x01E88EC0  app+0x088DC0  41e00203      r1 = 770
0x01E88EC4  app+0x088DC4  beea213c      call -231358
0x01E88EC8  app+0x088DC8  4034          r0 = 20
0x01E88ECA  app+0x088DCA  beeafc43      call -227336
0x01E88ECE  app+0x088DCE  50ee6604      r0 = b[r6+70] (u)
0x01E88ED2  app+0x088DD2  0044          if (r0 == 0) goto 8
0x01E88ED4  app+0x088DD4  4021          r0 = 1
0x01E88ED6  app+0x088DD6  4121          r1 = 1
0x01E88ED8  app+0x088DD8  beea384a      call -224144
0x01E88EDC  app+0x088DDC  c0ffc41a0100  r0 = 72388
0x01E88EE2  app+0x088DE2  8f60          h[r0+0] = r7
0x01E88EE4  app+0x088DE4  d8ee4009      r0 = b[r4+r9] (u)
0x01E88EE8  app+0x088DE8  8042          if (r0 != 0) goto 4
0x01E88EEA  app+0x088DEA  beeaec4a      call -223784
0x01E88EEE  app+0x088DEE  d8ee600c      r0 = b[r6+r12] (u)
0x01E88EF2  app+0x088DF2  00f81f02      if (r0 == 1) goto 62
0x01E88EF6  app+0x088DF6  80f8a700      if (r0 != 0) goto 334
0x01E88EFA  app+0x088DFA  50ed6c09      r0 = h[r6+156] (u)
0x01E88EFE  app+0x088DFE  0981          r1 = r0 + 1
0x01E88F00  app+0x088E00  50ed6d19      h[r6+156] = r1
0x01E88F04  app+0x088E04  80f99eca      if (r0 < 101) goto 316
0x01E88F08  app+0x088E08  50ed6d79      h[r6+156] = r7
0x01E88F0C  app+0x088E0C  50ee6604      r0 = b[r6+70] (u)
0x01E88F10  app+0x088E10  00f89100      if (r0 == 0) goto 290
0x01E88F14  app+0x088E14  4022          r0 = 2
0x01E88F16  app+0x088E16  beea6582      call -195382
0x01E88F1A  app+0x088E1A  50ee6604      r0 = b[r6+70] (u)
0x01E88F1E  app+0x088E1E  00aa          r0 = r0 << 10
0x01E88F20  app+0x088E20  302f          r0 |= 32768
0x01E88F22  app+0x088E22  8117          r1 = r0.l (u)
0x01E88F24  app+0x088E24  5830          r0 = 112
0x01E88F26  app+0x088E26  beeaf03b      call -231456
0x01E88F2A  app+0x088E2A  4021          r0 = 1
0x01E88F2C  app+0x088E2C  4121          r1 = 1
0x01E88F2E  app+0x088E2E  beea0d4a      call -224230
0x01E88F32  app+0x088E32  4488          goto 272
0x01E88F34  app+0x088E34  50ee6712      r1 = b[r6+39] (u)
0x01E88F38  app+0x088E38  40e0ec01      r0 = 492
0x01E88F3C  app+0x088E3C  d8ee6000      r0 = b[r6+r0] (u)
0x01E88F40  app+0x088E40  2154          if (r1 == 0) goto 168
0x01E88F42  app+0x088E42  50e83f00      if ((r0 & 1) == 0) goto 126
0x01E88F46  app+0x088E46  50ed6fe9      h[r6+158] = r14
0x01E88F4A  app+0x088E4A  4021          r0 = 1
0x01E88F4C  app+0x088E4C  4120          r1 = 0
0x01E88F4E  app+0x088E4E  beeafd49      call -224262
0x01E88F52  app+0x088E52  402a          r0 = 10
0x01E88F54  app+0x088E54  beeab743      call -227474
0x01E88F58  app+0x088E58  d1ec6c0c      r0 = [r6+460]
0x01E88F5C  app+0x088E5C  d1ec6c1b      r1 = [r6+444]
0x01E88F60  app+0x088E60  42e08907      r2 = 1929
0x01E88F64  app+0x088E64  d8ee6022      r2 = b[r6+r2] (u)
0x01E88F68  app+0x088E68  803b          [sp+108] = r0
0x01E88F6A  app+0x088E6A  0028          r0 = [sp+32]
0x01E88F6C  app+0x088E6C  8161          [r0+4] = r1
0x01E88F6E  app+0x088E6E  8a48          b[r0+8] = r2
0x01E88F70  app+0x088E70  8f49          b[r0+9] = r7
0x01E88F72  app+0x088E72  c1ff2ea8e501  r1 = 31828014
0x01E88F78  app+0x088E78  8163          [r0+12] = r1
0x01E88F7A  app+0x088E7A  e88c          r0 = sp + 108
0x01E88F7C  app+0x088E7C  beeaf48e      call -188952
0x01E88F80  app+0x088E80  4022          r0 = 2
0x01E88F82  app+0x088E82  beea2f82      call -195490
0x01E88F86  app+0x088E86  5830          r0 = 112
0x01E88F88  app+0x088E88  60e0401c      r1 = 0xC000
0x01E88F8C  app+0x088E8C  beeabd3b      call -231558
0x01E88F90  app+0x088E90  beea4e8d      call -189796
0x01E88F94  app+0x088E94  4830          r0 = 48
0x01E88F96  app+0x088E96  4120          r1 = 0
0x01E88F98  app+0x088E98  beeab73b      call -231570
0x01E88F9C  app+0x088E9C  4830          r0 = 48
0x01E88F9E  app+0x088E9E  c1fffec30000  r1 = 50174
0x01E88FA4  app+0x088EA4  beeab13b      call -231582
0x01E88FA8  app+0x088EA8  40e0ec01      r0 = 492
0x01E88FAC  app+0x088EAC  d8ee6170      b[r6+r0] = r7
0x01E88FB0  app+0x088EB0  402a          r0 = 10
0x01E88FB2  app+0x088EB2  beea8843      call -227568
0x01E88FB6  app+0x088EB6  50ee6604      r0 = b[r6+70] (u)
0x01E88FBA  app+0x088EBA  0044          if (r0 == 0) goto 8
0x01E88FBC  app+0x088EBC  4021          r0 = 1
0x01E88FBE  app+0x088EBE  4121          r1 = 1
0x01E88FC0  app+0x088EC0  beeac449      call -224376
0x01E88FC4  app+0x088EC4  50ed6c09      r0 = h[r6+156] (u)
0x01E88FC8  app+0x088EC8  0981          r1 = r0 + 1
0x01E88FCA  app+0x088ECA  50ed6d19      h[r6+156] = r1
0x01E88FCE  app+0x088ECE  80f939ca      if (r0 < 101) goto 114
0x01E88FD2  app+0x088ED2  50ed6d79      h[r6+156] = r7
0x01E88FD6  app+0x088ED6  50ee6604      r0 = b[r6+70] (u)
0x01E88FDA  app+0x088EDA  104d          if (r0 == 0) goto 90
0x01E88FDC  app+0x088EDC  00aa          r0 = r0 << 10
0x01E88FDE  app+0x088EDE  302f          r0 |= 32768
0x01E88FE0  app+0x088EE0  8117          r1 = r0.l (u)
0x01E88FE2  app+0x088EE2  5830          r0 = 112
0x01E88FE4  app+0x088EE4  beea913b      call -231646
0x01E88FE8  app+0x088EE8  148d          goto 90
0x01E88FEA  app+0x088EEA  50e82d02      if ((r0 & 1) != 0) goto 90
0x01E88FEE  app+0x088EEE  40e02003      r0 = 800
0x01E88FF2  app+0x088EF2  50ed6f09      h[r6+158] = r0
0x01E88FF6  app+0x088EF6  40e0ec01      r0 = 492
0x01E88FFA  app+0x088EFA  d8ee61e0      b[r6+r0] = r14
0x01E88FFE  app+0x088EFE  d82c          r0 += 108
