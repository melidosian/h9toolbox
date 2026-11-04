; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 438; undecoded chunks: 2
0x01E590C0  app+0x058FC0  d8edaa00      r0 = h[r10+r0<<1] (s)
0x01E590C4  app+0x058FC4  412a          r1 = 10
0x01E590C6  app+0x058FC6  f4e10101      r0 = r0 / r1 (s)
0x01E590CA  app+0x058FCA  8817          r0 = r0.l (s)
0x01E590CC  app+0x058FCC  0518          r5 += r0
0x01E590CE  app+0x058FCE  d117          r1 = r5.l (u)
0x01E590D0  app+0x058FD0  4838          r0 = 56
0x01E590D2  app+0x058FD2  bfea1abb      call -35276
0x01E590D6  app+0x058FD6  d1b0          r1 = r5 >> 16
0x01E590D8  app+0x058FD8  4839          r0 = 57
0x01E590DA  app+0x058FDA  bfea16bb      call -35284
0x01E590DE  app+0x058FDE  b9e80010      if (r9 != 0) {
0x01E590E2  app+0x058FE2  4021          r0 = 1
0x01E590E4  app+0x058FE4  4022          r0 = 2
0x01E590E6  app+0x058FE6  bfea5cff      call -328
0x01E590EA  app+0x058FEA  bfea73ff      call -282
0x01E590EE  app+0x058FEE  40e10170      r0 = r7 | 0x1
0x01E590F2  app+0x058FF2  00f80c06      if (r0 == 3) goto 24
0x01E590F6  app+0x058FF6  40e08c07      r0 = 1932
0x01E590FA  app+0x058FFA  d8eea000      r0 = b[r10+r0] (u)
0x01E590FE  app+0x058FFE  0047          if (r0 == 0) goto 14
0x01E59100  app+0x059000  b7e80100      if (r7 != 1) {
0x01E59104  app+0x059004  46e0ff0f      r6 = 4095
0x01E59108  app+0x059008  4521          r5 = 1
0x01E5910A  app+0x05900A  4121          r1 = 1
0x01E5910C  app+0x05900C  0489          goto 18
0x01E5910E  app+0x05900E  4120          r1 = 0
0x01E59110  app+0x059010  4521          r5 = 1
0x01E59112  app+0x059012  b7e80200      if (r7 != 2) {
0x01E59116  app+0x059016  4520          r5 = 0
0x01E59118  app+0x059018  30e1fe7f      r0 = r7 + -2
0x01E5911C  app+0x05901C  00f93104      if (r0 >= 2) goto 98
0x01E59120  app+0x059020  4022          r0 = 2
0x01E59122  app+0x059022  52eeaa04      b[r10+74] = r0
0x01E59126  app+0x059026  70e1706c      r0 = r6 & 0xFFFF0FFF
0x01E5912A  app+0x05902A  88f81302      if (r8 != 1) goto 38
0x01E5912E  app+0x05902E  b1e80050      if (r1 != 0) {
0x01E59132  app+0x059032  70e17e6c      r0 = r6 & 0xFFFF01FF
0x01E59136  app+0x059036  302b          r0 |= 2048
0x01E59138  app+0x059038  bfea51ff      call -350
0x01E5913C  app+0x05903C  50eda50b      h[r10+180] = r0
0x01E59140  app+0x059040  bfea59ff      call -334
0x01E59144  app+0x059044  40e0ff0f      r0 = 4095
0x01E59148  app+0x059048  80e84e60      if (r6 != r0) goto 156
0x01E5914C  app+0x05904C  5127          r1 = 71
0x01E5914E  app+0x05904E  50eda51b      h[r10+180] = r1
0x01E59152  app+0x059052  0492          goto 36
0x01E59154  app+0x059054  b1e80050      if (r1 != 0) {
0x01E59158  app+0x059058  70e17e6c      r0 = r6 & 0xFFFF01FF
0x01E5915C  app+0x05905C  302b          r0 |= 2048
0x01E5915E  app+0x05905E  bfea3eff      call -388
0x01E59162  app+0x059062  50eda70b      h[r10+182] = r0
0x01E59166  app+0x059066  bfea46ff      call -372
0x01E5916A  app+0x05906A  40e0ff0f      r0 = 4095
0x01E5916E  app+0x05906E  80e83e60      if (r6 != r0) goto 124
0x01E59172  app+0x059072  5127          r1 = 71
0x01E59174  app+0x059074  50eda71b      h[r10+182] = r1
0x01E59178  app+0x059078  40e06c03      r0 = 876
0x01E5917C  app+0x05907C  50eda30b      h[r10+178] = r0
0x01E59180  app+0x059080  149a          goto 116
0x01E59182  app+0x059082  87f85002      if (r7 != 1) goto 160
0x01E59186  app+0x059086  4021          r0 = 1
0x01E59188  app+0x059088  52eeaa04      b[r10+74] = r0
0x01E5918C  app+0x05908C  4020          r0 = 0
0x01E5918E  app+0x05908E  d8e90300      h[sp+2] = r0
0x01E59192  app+0x059092  70e1706c      r0 = r6 & 0xFFFF0FFF
0x01E59196  app+0x059096  3fe59f00      r0 = utof(r0)
0x01E5919A  app+0x05909A  c1ff00002041  r1 = 1092616192
0x01E591A0  app+0x0590A0  3fe50301      r0 = r0 / r1 (f)
0x01E591A4  app+0x0590A4  81ea67d2      call 238798
0x01E591A8  app+0x0590A8  c2ff4a07ebff  r2 = 4293592906
0x01E591AE  app+0x0590AE  c3ff1ca63440  r3 = 1077192220
0x01E591B4  app+0x0590B4  81eadfcf      call 237502
0x01E591B8  app+0x0590B8  81ea72cf      call 237284
0x01E591BC  app+0x0590BC  0516          r5 = r0
0x01E591BE  app+0x0590BE  8a82          r2 = sp + 2
0x01E591C0  app+0x0590C0  4024          r0 = 4
0x01E591C2  app+0x0590C2  b9e80010      if (r9 != 0) {
0x01E591C6  app+0x0590C6  41e0d752      r1 = 21207
0x01E591CA  app+0x0590CA  41e0d652      r1 = 21206
0x01E591CE  app+0x0590CE  4321          r3 = 1
0x01E591D0  app+0x0590D0  bfea6fcd      call -25890
0x01E591D4  app+0x0590D4  dce90200      r0 = b[sp+2] (u)
0x01E591D8  app+0x0590D8  41e1100c      r1 = r0 | 0x9000
0x01E591DC  app+0x0590DC  5031          r0 = 81
0x01E591DE  app+0x0590DE  bfea94ba      call -35544
0x01E591E2  app+0x0590E2  4027          r0 = 7
0x01E591E4  app+0x0590E4  5116          r1 = r5
0x01E591E6  app+0x0590E6  1483          goto 70
0x01E591E8  app+0x0590E8  50eda41b      r1 = h[r10+180] (u)
0x01E591EC  app+0x0590EC  0482          goto 4
0x01E591EE  app+0x0590EE  50eda61b      r1 = h[r10+182] (u)
0x01E591F2  app+0x0590F2  50eda20b      r0 = h[r10+178] (u)
0x01E591F6  app+0x0590F6  9216          r2 = r9
0x01E591F8  app+0x0590F8  bfea13ff      call -474
0x01E591FC  app+0x0590FC  5031          r0 = 81
0x01E591FE  app+0x0590FE  bfeafeb9      call -35844
0x01E59202  app+0x059102  0548          if (r5 == 0) goto 16
0x01E59204  app+0x059104  4120          r1 = 0
0x01E59206  app+0x059106  52eeab14      b[r10+75] = r1
0x01E5920A  app+0x05910A  88f81d02      if (r8 != 1) goto 58
0x01E5920E  app+0x05910E  71e1000d      r1 = r0 & 0xFFFFDFFF
0x01E59212  app+0x059112  0487          goto 14
0x01E59214  app+0x059114  4121          r1 = 1
0x01E59216  app+0x059116  52eeab14      b[r10+75] = r1
0x01E5921A  app+0x05911A  88f81502      if (r8 != 1) goto 42
0x01E5921E  app+0x05911E  41e1000d      r1 = r0 | 0x2000
0x01E59222  app+0x059122  5031          r0 = 81
0x01E59224  app+0x059124  048f          goto 30
0x01E59226  app+0x059126  4020          r0 = 0
0x01E59228  app+0x059128  52eeaa04      b[r10+74] = r0
0x01E5922C  app+0x05912C  5031          r0 = 81
0x01E5922E  app+0x05912E  bfea6cba      call -35624
0x01E59232  app+0x059132  40e0cd01      r0 = 461
0x01E59236  app+0x059136  50eda90b      h[r10+184] = r0
0x01E5923A  app+0x05913A  50eda80b      r0 = h[r10+184] (u)
0x01E5923E  app+0x05913E  41e1000d      r1 = r0 | 0x2000
0x01E59242  app+0x059142  4027          r0 = 7
0x01E59244  app+0x059144  bfea61ba      call -35646
0x01E59248  app+0x059148  0281          sp += 4
0x01E5924A  app+0x05914A  5a04          {pc, r10-r4} = [sp++]
0x01E5924C  app+0x05914C  1004          [--sp] = rets
0x01E5924E  app+0x05914E  71e1f010      r1 = r1 & 0xFFFFFF0F
0x01E59252  app+0x059152  32e1ff1f      r2 = r1 + -1
0x01E59256  app+0x059156  c1ff0bf50000  r1 = 62731
0x01E5925C  app+0x05915C  02fc1f1c      if (r2 > 14) goto 62
0x01E59260  app+0x059160  0201          tbb [r2]
0x01E59262  app+0x059162  080b          [r0++=r14] = r0
0x01E59264  app+0x059164  0e12          b[r0++=r12] = r6
0x01E59266  app+0x059166  1e1e          r6 = r1 - r1
0x01E59268  app+0x059168  1e1e          r6 = r1 - r1
0x01E5926A  app+0x05916A  1e1e          r6 = r1 - r1
0x01E5926C  app+0x05916C  1e1e          r6 = r1 - r1
0x01E5926E  app+0x05916E  1c16          r12 = r1
0x01E59270  app+0x059170  1900          .byte 0x19, 0x00
0x01E59272  app+0x059172  31e11111      r1 = r1 + -3823
0x01E59276  app+0x059176  0493          goto 38
0x01E59278  app+0x059178  21e1171a      r1 = r1 + -5609
0x01E5927C  app+0x05917C  0490          goto 32
0x01E5927E  app+0x05917E  c1ff2dd40000  r1 = 54317
0x01E59284  app+0x059184  048c          goto 24
0x01E59286  app+0x059186  c1ff35cc0000  r1 = 52277
0x01E5928C  app+0x05918C  0488          goto 16
0x01E5928E  app+0x05918E  01e1ef17      r1 = r1 + 2031
0x01E59292  app+0x059192  0485          goto 10
0x01E59294  app+0x059194  01e1f714      r1 = r1 + 1271
0x01E59298  app+0x059198  0482          goto 4
0x01E5929A  app+0x05919A  01e1e519      r1 = r1 + 2533
0x01E5929E  app+0x05919E  0044          if (r0 == 0) goto 8
0x01E592A0  app+0x0591A0  80f80502      if (r0 != 1) goto 10
0x01E592A4  app+0x0591A4  5834          r0 = 116
0x01E592A6  app+0x0591A6  0481          goto 2
0x01E592A8  app+0x0591A8  5835          r0 = 117
0x01E592AA  app+0x0591AA  bfea2eba      call -35748
0x01E592AE  app+0x0591AE  0004          pc = [sp++]
0x01E592B0  app+0x0591B0  1004          [--sp] = rets
0x01E592B2  app+0x0591B2  4029          r0 = 9
0x01E592B4  app+0x0591B4  592f          r1 = 111
0x01E592B6  app+0x0591B6  bfea28ba      call -35760
0x01E592BA  app+0x0591BA  4029          r0 = 9
0x01E592BC  app+0x0591BC  41e06b10      r1 = 4203
0x01E592C0  app+0x0591C0  bfea23ba      call -35770
0x01E592C4  app+0x0591C4  4029          r0 = 9
0x01E592C6  app+0x0591C6  41e06720      r1 = 8295
0x01E592CA  app+0x0591CA  bfea1eba      call -35780
0x01E592CE  app+0x0591CE  4029          r0 = 9
0x01E592D0  app+0x0591D0  41e06230      r1 = 12386
0x01E592D4  app+0x0591D4  bfea19ba      call -35790
0x01E592D8  app+0x0591D8  4029          r0 = 9
0x01E592DA  app+0x0591DA  41e05040      r1 = 16464
0x01E592DE  app+0x0591DE  bfea14ba      call -35800
0x01E592E2  app+0x0591E2  4029          r0 = 9
0x01E592E4  app+0x0591E4  41e04750      r1 = 20551
0x01E592E8  app+0x0591E8  bfea0fba      call -35810
0x01E592EC  app+0x0591EC  4029          r0 = 9
0x01E592EE  app+0x0591EE  41e03a60      r1 = 24634
0x01E592F2  app+0x0591F2  bfea0aba      call -35820
0x01E592F6  app+0x0591F6  4029          r0 = 9
0x01E592F8  app+0x0591F8  41e02c70      r1 = 28716
0x01E592FC  app+0x0591FC  bfea05ba      call -35830
0x01E59300  app+0x059200  c1ff41800000  r1 = 32833
0x01E59306  app+0x059206  4029          r0 = 9
0x01E59308  app+0x059208  bfeaffb9      call -35842
0x01E5930C  app+0x05920C  c1ff37900000  r1 = 36919
0x01E59312  app+0x059212  4029          r0 = 9
0x01E59314  app+0x059214  bfeaf9b9      call -35854
0x01E59318  app+0x059218  c1ff25a00000  r1 = 40997
0x01E5931E  app+0x05921E  4029          r0 = 9
0x01E59320  app+0x059220  bfeaf3b9      call -35866
0x01E59324  app+0x059224  c1ff17b00000  r1 = 45079
0x01E5932A  app+0x05922A  4029          r0 = 9
0x01E5932C  app+0x05922C  bfeaedb9      call -35878
0x01E59330  app+0x059230  c1ffe4c00000  r1 = 49380
0x01E59336  app+0x059236  4029          r0 = 9
0x01E59338  app+0x059238  bfeae7b9      call -35890
0x01E5933C  app+0x05923C  c1ffcbd00000  r1 = 53451
0x01E59342  app+0x059242  4029          r0 = 9
0x01E59344  app+0x059244  bfeae1b9      call -35902
0x01E59348  app+0x059248  c1ffb5e00000  r1 = 57525
0x01E5934E  app+0x05924E  4029          r0 = 9
0x01E59350  app+0x059250  bfeadbb9      call -35914
0x01E59354  app+0x059254  c1ff9ff00000  r1 = 61599
0x01E5935A  app+0x05925A  4029          r0 = 9
0x01E5935C  app+0x05925C  bfead5b9      call -35926
0x01E59360  app+0x059260  4821          r0 = 33
0x01E59362  app+0x059262  41e0d806      r1 = 1752
0x01E59366  app+0x059266  bfead0b9      call -35936
0x01E5936A  app+0x05926A  c1ff7e8c0000  r1 = 35966
0x01E59370  app+0x059270  4824          r0 = 36
0x01E59372  app+0x059272  8804          {rets} = [sp++]
0x01E59374  app+0x059274  ffeac9b9      goto -35950
0x01E59378  app+0x059278  4824          r0 = 36
0x01E5937A  app+0x05927A  4120          r1 = 0
0x01E5937C  app+0x05927C  ffeac5b9      goto -35958
0x01E59380  app+0x059280  7404          [--sp] = {rets, r4}
0x01E59382  app+0x059282  40e08207      r0 = 1922
0x01E59386  app+0x059286  c1ffd01a0100  r1 = 72400
0x01E5938C  app+0x05928C  d8ee1040      r4 = b[r1+r0] (u)
0x01E59390  app+0x059290  30e1ff4f      r0 = r4 + -1
0x01E59394  app+0x059294  0017          r0 = r0.b0 (u)
0x01E59396  app+0x059296  00fc1c1e      if (r0 > 15) goto 56
0x01E5939A  app+0x05929A  4831          r0 = 49
0x01E5939C  app+0x05929C  bfea2fb9      call -36258
0x01E593A0  app+0x0592A0  41e10200      r1 = r0 | 0x2
0x01E593A4  app+0x0592A4  4831          r0 = 49
0x01E593A6  app+0x0592A6  bfeab0b9      call -36000
0x01E593AA  app+0x0592AA  c0fff408eb01  r0 = 32180468
0x01E593B0  app+0x0592B0  d8ed0804      r0 = h[r0+r4<<1] (u)
0x01E593B4  app+0x0592B4  81eabad1      call 238452
0x01E593B8  app+0x0592B8  c2ff4a07ebff  r2 = 4293592906
0x01E593BE  app+0x0592BE  c3ff1ca62440  r3 = 1076143644
0x01E593C4  app+0x0592C4  81ead7ce      call 236974
0x01E593C8  app+0x0592C8  81ea6ace      call 236756
0x01E593CC  app+0x0592CC  0116          r1 = r0
0x01E593CE  app+0x0592CE  5831          r0 = 113
0x01E593D0  app+0x0592D0  0486          goto 12
0x01E593D2  app+0x0592D2  4831          r0 = 49
0x01E593D4  app+0x0592D4  bfea13b9      call -36314
0x01E593D8  app+0x0592D8  71e10200      r1 = r0 & 0xFFFFFFFD
0x01E593DC  app+0x0592DC  4831          r0 = 49
0x01E593DE  app+0x0592DE  3404          {rets, r4} = [sp++]
0x01E593E0  app+0x0592E0  ffea93b9      goto -36058
0x01E593E4  app+0x0592E4  00a8          r0 = r0 << 8
0x01E593E6  app+0x0592E6  41e04060      r1 = 24640
0x01E593EA  app+0x0592EA  0119          r1 |= r0
0x01E593EC  app+0x0592EC  5027          r0 = 71
0x01E593EE  app+0x0592EE  ffea8cb9      goto -36072
0x01E593F2  app+0x0592F2  1004          [--sp] = rets
0x01E593F4  app+0x0592F4  c1ff20d77501  r1 = 24500000
0x01E593FA  app+0x0592FA  81e90700      if (r0 < r1) goto 14
0x01E593FE  app+0x0592FE  4022          r0 = 2
0x01E59400  app+0x059300  4120          r1 = 0
0x01E59402  app+0x059302  bfeab9b9      call -35982
0x01E59406  app+0x059306  4024          r0 = 4
0x01E59408  app+0x059308  4121          r1 = 1
0x01E5940A  app+0x05930A  0486          goto 12
0x01E5940C  app+0x05930C  4022          r0 = 2
0x01E5940E  app+0x05930E  4121          r1 = 1
0x01E59410  app+0x059310  bfeab2b9      call -35996
0x01E59414  app+0x059314  4024          r0 = 4
0x01E59416  app+0x059316  4120          r1 = 0
0x01E59418  app+0x059318  bfeaaeb9      call -36004
0x01E5941C  app+0x05931C  4023          r0 = 3
0x01E5941E  app+0x05931E  4121          r1 = 1
0x01E59420  app+0x059320  bfeaaab9      call -36012
0x01E59424  app+0x059324  4025          r0 = 5
0x01E59426  app+0x059326  4121          r1 = 1
0x01E59428  app+0x059328  8804          {rets} = [sp++]
0x01E5942A  app+0x05932A  ffeaa5b9      goto -36022
0x01E5942E  app+0x05932E  7b04          [--sp] = {rets, r11-r4}
0x01E59430  app+0x059330  e29f          sp += -4
0x01E59432  app+0x059332  c8ffd01a0100  r8 = 72400
0x01E59438  app+0x059338  0047          if (r0 == 0) goto 14
0x01E5943A  app+0x05933A  0990          r1 = r0 + 16
0x01E5943C  app+0x05933C  d0ec0ca0      r10 = [r0+12]
0x01E59440  app+0x059340  0a88          r2 = r0 + 8
0x01E59442  app+0x059342  0b86          r3 = r0 + 6
0x01E59444  app+0x059344  0c84          r4 = r0 + 4
0x01E59446  app+0x059346  048b          goto 22
0x01E59448  app+0x059348  01e17a87      r1 = r8 + 1914
0x01E5944C  app+0x05934C  02e11880      r2 = r8 + 24
0x01E59450  app+0x059350  03e19880      r3 = r8 + 152
0x01E59454  app+0x059354  04e11780      r4 = r8 + 23
0x01E59458  app+0x059358  00e1bc81      r0 = r8 + 444
0x01E5945C  app+0x05935C  c214          r10 = 0
0x01E5945E  app+0x05935E  d0ec0090      r9 = [r0+0]
0x01E59462  app+0x059362  4f40          r7 = b[r4+0] (u)
0x01E59464  app+0x059364  3e60          r6 = h[r3+0] (u)
0x01E59466  app+0x059366  50ee20b0      r11 = b[r2+0] (u)
0x01E5946A  app+0x05936A  1c40          r4 = b[r1+0] (u)
0x01E5946C  app+0x05936C  4020          r0 = 0
0x01E5946E  app+0x05936E  4520          r5 = 0
0x01E59470  app+0x059370  bfeafbb7      call -36874
0x01E59474  app+0x059374  4020          r0 = 0
0x01E59476  app+0x059376  bfea6ffd      call -1314
0x01E5947A  app+0x05937A  8014          r1_r0 = 0
0x01E5947C  app+0x05937C  bfea80fd      call -1280
0x01E59480  app+0x059380  8af80800      if (r10 != 0) goto 16
0x01E59484  app+0x059384  50ee8502      r0 = b[r8+37] (u)
0x01E59488  app+0x059388  4ae00200      r10 = 2
0x01E5948C  app+0x05938C  b0e80100      if (r0 != 1) {
0x01E59490  app+0x059390  4ae00100      r10 = 1
0x01E59494  app+0x059394  4830          r0 = 48
0x01E59496  app+0x059396  4120          r1 = 0
0x01E59498  app+0x059398  bfea37b9      call -36242
0x01E5949C  app+0x05939C  4320          r3 = 0
0x01E5949E  app+0x05939E  bae80240      if (r10 != 2) {
0x01E594A2  app+0x0593A2  7316          r3 = r7
0x01E594A4  app+0x0593A4  6516          r5 = r6
0x01E594A6  app+0x0593A6  4020          r0 = 0
0x01E594A8  app+0x0593A8  91d6          r1 = r9  #
0x01E594AA  app+0x0593AA  8420          [sp] = r4
0x01E594AC  app+0x0593AC  5216          r2 = r5
0x01E594AE  app+0x0593AE  bfeaf1fd      call -1054
0x01E594B2  app+0x0593B2  4837          r0 = 55
0x01E594B4  app+0x0593B4  41e00f1f      r1 = 7951
0x01E594B8  app+0x0593B8  bfea27b9      call -36274
0x01E594BC  app+0x0593BC  50ee8d02      r0 = b[r8+45] (u)
0x01E594C0  app+0x0593C0  8044          if (r0 != 0) goto 8
0x01E594C2  app+0x0593C2  4020          r0 = 0
0x01E594C4  app+0x0593C4  4122          r1 = 2
0x01E594C6  app+0x0593C6  bfeac1fe      call -638
0x01E594CA  app+0x0593CA  3be80010      if (r11 == 0) {
0x01E594CE  app+0x0593CE  c1fff1bd0000  r1 = 48625
0x01E594D4  app+0x0593D4  c1fff7bf0000  r1 = 49143
0x01E594DA  app+0x0593DA  4830          r0 = 48
0x01E594DC  app+0x0593DC  bfea15b9      call -36310
0x01E594E0  app+0x0593E0  8014          r1_r0 = 0
0x01E594E2  app+0x0593E2  bfea4dfd      call -1382
0x01E594E6  app+0x0593E6  40e0d010      r0 = 4304
0x01E594EA  app+0x0593EA  d8ee8040      r4 = b[r8+r0] (u)
0x01E594EE  app+0x0593EE  4831          r0 = 49
0x01E594F0  app+0x0593F0  bfea85b8      call -36598
0x01E594F4  app+0x0593F4  34e80010      if (r4 == 0) {
0x01E594F8  app+0x0593F8  71e10400      r1 = r0 & 0xFFFFFFFB
0x01E594FC  app+0x0593FC  41e10400      r1 = r0 | 0x4
0x01E59500  app+0x059400  4831          r0 = 49
0x01E59502  app+0x059402  bfea02b9      call -36348
0x01E59506  app+0x059406  40e07e07      r0 = 1918
0x01E5950A  app+0x05940A  d8ee8000      r0 = b[r8+r0] (u)
0x01E5950E  app+0x05940E  8047          if (r0 != 0) goto 14
0x01E59510  app+0x059410  c0fff0c00000  r0 = 49392
0x01E59516  app+0x059416  50ee0015      r1 = b[r0+80] (u)
0x01E5951A  app+0x05941A  81f80802      if (r1 != 1) goto 16
0x01E5951E  app+0x05941E  c0ff0085cf00  r0 = 13600000
0x01E59524  app+0x059424  80e91c90      if (r9 < r0) goto 56
0x01E59528  app+0x059428  bfeac2fe      call -636
0x01E5952C  app+0x05942C  049b          goto 54
0x01E5952E  app+0x05942E  50ee0105      r0 = b[r0+81] (u)
0x01E59532  app+0x059432  00f8f403      if (r0 == 1) goto -24
0x01E59536  app+0x059436  40e00811      r0 = 4360
0x01E5953A  app+0x05943A  d8ee8000      r0 = b[r8+r0] (u)
0x01E5953E  app+0x05943E  00f8ee03      if (r0 == 1) goto -36
0x01E59542  app+0x059442  40e00f11      r0 = 4367
0x01E59546  app+0x059446  d8ee8000      r0 = b[r8+r0] (u)
0x01E5954A  app+0x05944A  f049          if (r0 != 0) goto -46
0x01E5954C  app+0x05944C  c0ff0085cf00  r0 = 13600000
0x01E59552  app+0x059452  80e90590      if (r9 < r0) goto 10
0x01E59556  app+0x059456  40e01011      r0 = 4368
0x01E5955A  app+0x05945A  d8ee8000      r0 = b[r8+r0] (u)
0x01E5955E  app+0x05945E  f044          if (r0 != 0) goto -56
0x01E59560  app+0x059460  bfea0aff      call -492
0x01E59564  app+0x059464  bfea0cff      call -488
0x01E59568  app+0x059468  4033          r0 = 19
0x01E5956A  app+0x05946A  8af81304      if (r10 != 2) goto 38
0x01E5956E  app+0x05946E  41e0ff03      r1 = 1023
0x01E59572  app+0x059472  bfeacab8      call -36460
0x01E59576  app+0x059476  502a          r0 = 74
0x01E59578  app+0x059478  41e04854      r1 = 21576
0x01E5957C  app+0x05947C  bfeac5b8      call -36470
0x01E59580  app+0x059480  5023          r0 = 67
0x01E59582  app+0x059482  4928          r1 = 40
0x01E59584  app+0x059484  bfeac1b8      call -36478
0x01E59588  app+0x059488  5027          r0 = 71
0x01E5958A  app+0x05948A  41e04067      r1 = 26432
0x01E5958E  app+0x05948E  bfeabcb8      call -36488
0x01E59592  app+0x059492  048c          goto 24
0x01E59594  app+0x059494  41e0be03      r1 = 958
0x01E59598  app+0x059498  bfeab7b8      call -36498
0x01E5959C  app+0x05949C  502a          r0 = 74
0x01E5959E  app+0x05949E  41e05054      r1 = 21584
0x01E595A2  app+0x0594A2  bfeab2b8      call -36508
0x01E595A6  app+0x0594A6  4021          r0 = 1
0x01E595A8  app+0x0594A8  bfea1cff      call -456
0x01E595AC  app+0x0594AC  9016          r0 = r9
0x01E595AE  app+0x0594AE  bfea20ff      call -448
0x01E595B2  app+0x0594B2  0281          sp += 4
0x01E595B4  app+0x0594B4  5b04          {pc, r11-r4} = [sp++]
0x01E595B6  app+0x0594B6  1004          [--sp] = rets
0x01E595B8  app+0x0594B8  e29b          sp += -20
0x01E595BA  app+0x0594BA  c0ffd01a0100  r0 = 72400
0x01E595C0  app+0x0594C0  d1ec0c1b      r1 = [r0+444]
0x01E595C4  app+0x0594C4  50ee0721      r2 = b[r0+23] (u)
0x01E595C8  app+0x0594C8  50ed0839      r3 = h[r0+152] (u)
0x01E595CC  app+0x0594CC  8120          [sp] = r1
0x01E595CE  app+0x0594CE  dee90420      b[sp+4] = r2
0x01E595D2  app+0x0594D2  50ee0811      r1 = b[r0+24] (u)
0x01E595D6  app+0x0594D6  d8e90730      h[sp+6] = r3
0x01E595DA  app+0x0594DA  42e07a07      r2 = 1914
0x01E595DE  app+0x0594DE  d8ee0002      r0 = b[r0+r2] (u)
0x01E595E2  app+0x0594E2  dee90810      b[sp+8] = r1
0x01E595E6  app+0x0594E6  4120          r1 = 0
0x01E595E8  app+0x0594E8  8123          [sp+12] = r1
0x01E595EA  app+0x0594EA  dee91000      b[sp+16] = r0
0x01E595EE  app+0x0594EE  8880          r0 = sp + 0
0x01E595F0  app+0x0594F0  bfea1dff      call -454
0x01E595F4  app+0x0594F4  0285          sp += 20
0x01E595F6  app+0x0594F6  0004          pc = [sp++]
0x01E595F8  app+0x0594F8  7404          [--sp] = {rets, r4}
0x01E595FA  app+0x0594FA  e29f          sp += -4
0x01E595FC  app+0x0594FC  4020          r0 = 0
0x01E595FE  app+0x0594FE  bfea34b7      call -37272
0x01E59602  app+0x059502  4020          r0 = 0
0x01E59604  app+0x059504  bfeaa8fc      call -1712
0x01E59608  app+0x059508  8014          r1_r0 = 0
0x01E5960A  app+0x05950A  bfeab9fc      call -1678
0x01E5960E  app+0x05950E  4830          r0 = 48
0x01E59610  app+0x059510  4120          r1 = 0
0x01E59612  app+0x059512  bfea7ab8      call -36620
0x01E59616  app+0x059516  c0ffd01a0100  r0 = 72400
0x01E5961C  app+0x05951C  50ee0a12      r1 = b[r0+42] (u)
0x01E59620  app+0x059520  42e07a07      r2 = 1914
0x01E59624  app+0x059524  d8ee0042      r4 = b[r0+r2] (u)
0x01E59628  app+0x059528  c0ff1432eb01  r0 = 32190996
0x01E5962E  app+0x05952E  d8ec0a11      r1 = [r0+r1<<2]
0x01E59632  app+0x059532  4020          r0 = 0
0x01E59634  app+0x059534  82d4          r3_r2 = 0  #
0x01E59636  app+0x059536  8420          [sp] = r4
0x01E59638  app+0x059538  bfea2cfd      call -1448
0x01E5963C  app+0x05953C  4837          r0 = 55
0x01E5963E  app+0x05953E  41e00f1f      r1 = 7951
0x01E59642  app+0x059542  bfea62b8      call -36668
0x01E59646  app+0x059546  8014          r1_r0 = 0
0x01E59648  app+0x059548  bfea00fe      call -1024
0x01E5964C  app+0x05954C  c1fff5bd0000  r1 = 48629
0x01E59652  app+0x059552  4830          r0 = 48
0x01E59654  app+0x059554  bfea59b8      call -36686
0x01E59658  app+0x059558  4033          r0 = 19
0x01E5965A  app+0x05955A  41e0be03      r1 = 958
0x01E5965E  app+0x05955E  bfea          .byte 0xBF, 0xEA
