; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 123; undecoded chunks: 0
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
0x01E89000  app+0x088F00  d8ee6170      b[r6+r0] = r7
0x01E89004  app+0x088F04  beeace9c      call -181860
0x01E89008  app+0x088F08  d1ec6c0b      r0 = [r6+444]
0x01E8900C  app+0x088F0C  beeaaa8b      call -190636
0x01E89010  app+0x088F10  4021          r0 = 1
0x01E89012  app+0x088F12  4120          r1 = 0
0x01E89014  app+0x088F14  beea9a49      call -224460
0x01E89018  app+0x088F18  402a          r0 = 10
0x01E8901A  app+0x088F1A  beea5443      call -227672
0x01E8901E  app+0x088F1E  4020          r0 = 0
0x01E89020  app+0x088F20  beeae081      call -195648
0x01E89024  app+0x088F24  4830          r0 = 48
0x01E89026  app+0x088F26  4120          r1 = 0
0x01E89028  app+0x088F28  beea6f3b      call -231714
0x01E8902C  app+0x088F2C  5830          r0 = 112
0x01E8902E  app+0x088F2E  4120          r1 = 0
0x01E89030  app+0x088F30  beea6b3b      call -231722
0x01E89034  app+0x088F34  0489          goto 18
0x01E89036  app+0x088F36  4021          r0 = 1
0x01E89038  app+0x088F38  4120          r1 = 0
0x01E8903A  app+0x088F3A  beea8749      call -224498
0x01E8903E  app+0x088F3E  4020          r0 = 0
0x01E89040  app+0x088F40  beead081      call -195680
0x01E89044  app+0x088F44  bfeafff8      call -3586
0x01E89048  app+0x088F48  50ed4202      r0 = h[r4+34] (u)
0x01E8904C  app+0x088F4C  80ea6e53      call 42716
0x01E89050  app+0x088F50  f216          r2 = r15
0x01E89052  app+0x088F52  8316          r3 = r8
0x01E89054  app+0x088F54  80ea8f50      call 41246
0x01E89058  app+0x088F58  80ea2250      call 41028
0x01E8905C  app+0x088F5C  d8ee605b      r5 = b[r6+r11] (u)
0x01E89060  app+0x088F60  0116          r1 = r0
0x01E89062  app+0x088F62  5831          r0 = 113
0x01E89064  app+0x088F64  beea513b      call -231774
0x01E89068  app+0x088F68  4022          r0 = 2
0x01E8906A  app+0x088F6A  8beac9bc      call 1538450
0x01E8906E  app+0x088F6E  50ed4202      r0 = h[r4+34] (u)
0x01E89072  app+0x088F72  054d          if (r5 == 0) goto 26
0x01E89074  app+0x088F74  c021          r0 += 1
0x01E89076  app+0x088F76  8117          r1 = r0.l (u)
0x01E89078  app+0x088F78  50ed4302      h[r4+34] = r0
0x01E8907C  app+0x088F7C  31e9e983      if (r1 >= 1001) {
