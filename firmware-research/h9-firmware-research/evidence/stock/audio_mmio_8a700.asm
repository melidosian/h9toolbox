; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 174; undecoded chunks: 0
0x01E8A800  app+0x08A700  580b          [r5++=r14] = r0
0x01E8A802  app+0x08A702  4120          r1 = 0
0x01E8A804  app+0x08A704  beea85bb      call -166134
0x01E8A808  app+0x08A708  30e80180      if (r0 == 1) {
0x01E8A80C  app+0x08A70C  d2ec580b      r0 = [r5+696]
0x01E8A810  app+0x08A710  4116          r1 = r4
0x01E8A812  app+0x08A712  beeaa2c1      call -163004
0x01E8A816  app+0x08A716  0287          sp += 28
0x01E8A818  app+0x08A718  5504          {pc, r5, r4} = [sp++]
0x01E8A81A  app+0x08A71A  602c          r0 = 140
0x01E8A81C  app+0x08A71C  fdea2d18      goto -380838
0x01E8A820  app+0x08A720  602d          r0 = 141
0x01E8A822  app+0x08A722  fdea2a18      goto -380844
0x01E8A826  app+0x08A726  7604          [--sp] = {rets, r6-r4}
0x01E8A828  app+0x08A728  40e0ec03      r0 = 1004
0x01E8A82C  app+0x08A72C  c4ffd01a0100  r4 = 72400
0x01E8A832  app+0x08A732  d8ec4a00      r0 = [r4+r0<<2]
0x01E8A836  app+0x08A736  f0f85bfe      if (r0 != -1) goto 182
0x01E8A83A  app+0x08A73A  c0ff00311e00  r0 = 1978624
0x01E8A840  app+0x08A740  0060          r0 = [r0+0]
0x01E8A842  app+0x08A742  50e85522      if ((r0 & 16) != 0) goto 170
0x01E8A846  app+0x08A746  c0ff48c00000  r0 = 49224
0x01E8A84C  app+0x08A74C  43f01127      r3 = 10001  #
0x01E8A850  app+0x08A750  0960          r1 = h[r0+0] (u)
0x01E8A852  app+0x08A752  c121          r1 += 1
0x01E8A854  app+0x08A754  92d7          r2 = r1.l (u)  #
0x01E8A856  app+0x08A756  8960          h[r0+0] = r1
0x01E8A858  app+0x08A758  83e92820      if (r2 < r3) goto 80
0x01E8A85C  app+0x08A75C  50ed4a1c      r1 = h[r4+202] (u)
0x01E8A860  app+0x08A760  01f80d04      if (r1 == 2) goto 26
0x01E8A864  app+0x08A764  01f84502      if (r1 == 1) goto 138
0x01E8A868  app+0x08A768  9141          if (r1 != 0) goto 66
0x01E8A86A  app+0x08A76A  4021          r0 = 1
0x01E8A86C  app+0x08A76C  01e1c840      r1 = r4 + 200
0x01E8A870  app+0x08A770  9861          h[r1+2] = r0
0x01E8A872  app+0x08A772  1860          r0 = h[r1+0] (u)
0x01E8A874  app+0x08A774  9862          h[r1+4] = r0
0x01E8A876  app+0x08A776  402f          r0 = 15
0x01E8A878  app+0x08A778  3604          {rets, r6-r4} = [sp++]
0x01E8A87A  app+0x08A77A  feea9101      goto -261342
0x01E8A87E  app+0x08A77E  4120          r1 = 0
0x01E8A880  app+0x08A780  02e1c440      r2 = r4 + 196
0x01E8A884  app+0x08A784  a963          h[r2+6] = r1
0x01E8A886  app+0x08A786  06f16848      r6 = r4 + 2152  #
0x01E8A88A  app+0x08A78A  2b62          r3 = h[r2+4] (u)
0x01E8A88C  app+0x08A78C  d2ec445c      r5 = [r4+708]
0x01E8A890  app+0x08A790  d8ed6935      h[r6+r5<<1] = r3
0x01E8A894  app+0x08A794  c521          r5 += 1
0x01E8A896  app+0x08A796  4320          r3 = 0
0x01E8A898  app+0x08A798  b5e81000      if (r5 != 16) {
0x01E8A89C  app+0x08A79C  5316          r3 = r5
0x01E8A89E  app+0x08A79E  d2ec453c      [r4+708] = r3
0x01E8A8A2  app+0x08A7A2  2b65          r3 = h[r2+10] (u)
0x01E8A8A4  app+0x08A7A4  ab61          h[r2+2] = r3
0x01E8A8A6  app+0x08A7A6  2b64          r3 = h[r2+8] (u)
0x01E8A8A8  app+0x08A7A8  ab62          h[r2+4] = r3
0x01E8A8AA  app+0x08A7AA  8960          h[r0+0] = r1
0x01E8A8AC  app+0x08A7AC  50ee4956      r5 = b[r4+105] (u)
0x01E8A8B0  app+0x08A7B0  50a3          r0 = r5 << 3
0x01E8A8B2  app+0x08A7B2  4018          r0 += r4
0x01E8A8B4  app+0x08A7B4  00e1680f      r0 = r0 + 3944
0x01E8A8B8  app+0x08A7B8  c521          r5 += 1
0x01E8A8BA  app+0x08A7BA  05fe0512      ifs (r5 > 9) goto 10
0x01E8A8BE  app+0x08A7BE  d8ec0810      r1 = [r0++=8]
0x01E8A8C2  app+0x08A7C2  71f8f9ff      if (r1 == -1) goto -14
0x01E8A8C6  app+0x08A7C6  0481          goto 2
0x01E8A8C8  app+0x08A7C8  4520          r5 = 0
0x01E8A8CA  app+0x08A7CA  5017          r0 = r5.b0 (u)
0x01E8A8CC  app+0x08A7CC  00a3          r0 = r0 << 3
0x01E8A8CE  app+0x08A7CE  4018          r0 += r4
0x01E8A8D0  app+0x08A7D0  41e0d803      r1 = 984
0x01E8A8D4  app+0x08A7D4  d8ec0a01      r0 = [r0+r1<<2]
0x01E8A8D8  app+0x08A7D8  beea6201      call -261436
0x01E8A8DC  app+0x08A7DC  50ee4916      r1 = b[r4+105] (u)
0x01E8A8E0  app+0x08A7E0  11a3          r1 = r1 << 3
0x01E8A8E2  app+0x08A7E2  4118          r1 += r4
0x01E8A8E4  app+0x08A7E4  42e0b207      r2 = 1970
0x01E8A8E8  app+0x08A7E8  d8ed1902      h[r1+r2<<1] = r0
0x01E8A8EC  app+0x08A7EC  52ee4956      b[r4+105] = r5
0x01E8A8F0  app+0x08A7F0  5604          {pc, r6-r4} = [sp++]
0x01E8A8F2  app+0x08A7F2  4022          r0 = 2
0x01E8A8F4  app+0x08A7F4  50ed4b0c      h[r4+202] = r0
0x01E8A8F8  app+0x08A7F8  c0ff0f005000  r0 = 5242895
0x01E8A8FE  app+0x08A7FE  beea4f01      call -261474
0x01E8A902  app+0x08A802  50ed4f0c      h[r4+206] = r0
0x01E8A906  app+0x08A806  5604          {pc, r6-r4} = [sp++]
0x01E8A908  app+0x08A808  e904          [--sp] = {psr, rets, reti}
0x01E8A90A  app+0x08A80A  d8e81f00      [--sp] = {r4-r0}
0x01E8A90E  app+0x08A80E  c4ff00311e00  r4 = 1978624
0x01E8A914  app+0x08A814  4061          r0 = [r4+4]
0x01E8A916  app+0x08A816  c1ffd01a0100  r1 = 72400
0x01E8A91C  app+0x08A81C  50ed190c      h[r1+200] = r0
0x01E8A920  app+0x08A820  4060          r0 = [r4+0]
0x01E8A922  app+0x08A822  4027          r0 = 7
0x01E8A924  app+0x08A824  beea3601      call -261524
0x01E8A928  app+0x08A828  8bea84b6      call 1535240
0x01E8A92C  app+0x08A82C  40ea4040      [r4+0] = 0x40
0x01E8A930  app+0x08A830  40ea0040      [r4+0] = 0x0
0x01E8A934  app+0x08A834  8bea8cb6      call 1535256
0x01E8A938  app+0x08A838  d4e81f00      {r4-r0} = [sp++]
0x01E8A93C  app+0x08A83C  a904          {psr, rets, reti} = [sp++]
0x01E8A93E  app+0x08A83E  8100          rti
0x01E8A940  app+0x08A840  c0ff002f1e00  r0 = 1978112
0x01E8A946  app+0x08A846  c0ef1000      [r0+0] &= 0xFFFFFFEF
0x01E8A94A  app+0x08A84A  d0ef1000      [r0+64] &= 0xFFFFFFEF
0x01E8A94E  app+0x08A84E  e0e0660c      r0 = r0 + 0xE600
0x01E8A952  app+0x08A852  c0ef0100      [r0+0] &= 0xFFFFFFFE
0x01E8A956  app+0x08A856  60e08109      r0 = 0x102000
0x01E8A95A  app+0x08A85A  40ea0200      [r0+0] = 0x2
0x01E8A95E  app+0x08A85E  8000          rts
0x01E8A960  app+0x08A860  4021          r0 = 1
0x01E8A962  app+0x08A862  8000          rts
0x01E8A964  app+0x08A864  7504          [--sp] = {rets, r5, r4}
0x01E8A966  app+0x08A866  8bea65b6      call 1535178
0x01E8A96A  app+0x08A86A  40e0880b      r0 = 2952
0x01E8A96E  app+0x08A86E  c4ffd01a0100  r4 = 72400
0x01E8A974  app+0x08A874  d8ee4010      r1 = b[r4+r0] (u)
0x01E8A978  app+0x08A878  c121          r1 += 1
0x01E8A97A  app+0x08A87A  1217          r2 = r1.b0 (u)
0x01E8A97C  app+0x08A87C  d8ee4110      b[r4+r0] = r1
0x01E8A980  app+0x08A880  82f9120a      if (r2 < 5) goto 36
0x01E8A984  app+0x08A884  05e1844b      r5 = r4 + 2948
0x01E8A988  app+0x08A888  5060          r0 = [r5+0]
0x01E8A98A  app+0x08A88A  8017          r0 = r0.l (u)
0x01E8A98C  app+0x08A88C  bcea2ad0      call -417708
0x01E8A990  app+0x08A890  4020          r0 = 0
0x01E8A992  app+0x08A892  d060          [r5+0] = r0
0x01E8A994  app+0x08A894  d844          b[r5+4] = r0
0x01E8A996  app+0x08A896  8bea5bb6      call 1535158
0x01E8A99A  app+0x08A89A  11e13441      r1 = r4 + 4404
0x01E8A99E  app+0x08A89E  5036          r0 = 86
0x01E8A9A0  app+0x08A8A0  4221          r2 = 1
0x01E8A9A2  app+0x08A8A2  3504          {rets, r5, r4} = [sp++]
0x01E8A9A4  app+0x08A8A4  fdea7a0d      goto -386316
0x01E8A9A8  app+0x08A8A8  3504          {rets, r5, r4} = [sp++]
0x01E8A9AA  app+0x08A8AA  cbea51b6      goto 1535138
0x01E8A9AE  app+0x08A8AE  4020          r0 = 0
0x01E8A9B0  app+0x08A8B0  8000          rts
0x01E8A9B2  app+0x08A8B2  40e0d002      r0 = 720
0x01E8A9B6  app+0x08A8B6  c1ffd01a0100  r1 = 72400
0x01E8A9BC  app+0x08A8BC  d8ee1000      r0 = b[r1+r0] (u)
0x01E8A9C0  app+0x08A8C0  8000          rts
0x01E8A9C2  app+0x08A8C2  7504          [--sp] = {rets, r5, r4}
0x01E8A9C4  app+0x08A8C4  0416          r4 = r0
0x01E8A9C6  app+0x08A8C6  01f80902      if (r1 == 1) goto 18
0x01E8A9CA  app+0x08A8CA  9141          if (r1 != 0) goto 66
0x01E8A9CC  app+0x08A8CC  bdea6092      call -318272
0x01E8A9D0  app+0x08A8D0  80fd1d02      ifs (r0 < 1) goto 58
0x01E8A9D4  app+0x08A8D4  483a          r0 = 58
0x01E8A9D6  app+0x08A8D6  3504          {rets, r5, r4} = [sp++]
0x01E8A9D8  app+0x08A8D8  feea1600      goto -262100
0x01E8A9DC  app+0x08A8DC  bdea5892      call -318288
0x01E8A9E0  app+0x08A8E0  8043          if (r0 != 0) goto 6
0x01E8A9E2  app+0x08A8E2  483a          r0 = 58
0x01E8A9E4  app+0x08A8E4  beeaad08      call -257702
0x01E8A9E8  app+0x08A8E8  4d88          r5 = r4 + 8
0x01E8A9EA  app+0x08A8EA  4120          r1 = 0
0x01E8A9EC  app+0x08A8EC  5016          r0 = r5
0x01E8A9EE  app+0x08A8EE  8beae9c0      call 1540562
0x01E8A9F2  app+0x08A8F2  4016          r0 = r4
0x01E8A9F4  app+0x08A8F4  bdea2190      call -319422
0x01E8A9F8  app+0x08A8F8  8047          if (r0 != 0) goto 14
0x01E8A9FA  app+0x08A8FA  4016          r0 = r4
0x01E8A9FC  app+0x08A8FC  bdea5792      call -318290
0x01E8AA00  app+0x08A900  00e1a840      r0 = r4 + 168
0x01E8AA04  app+0x08A904  bdea2d94      call -317350
0x01E8AA08  app+0x08A908  5016          r0 = r5
0x01E8AA0A  app+0x08A90A  8bea17c1      call 1540654
0x01E8AA0E  app+0x08A90E  5504          {pc, r5, r4} = [sp++]
0x01E8AA10  app+0x08A910  c0ffd01a0100  r0 = 72400
0x01E8AA16  app+0x08A916  d2ec041e      r1 = [r0+740]
0x01E8AA1A  app+0x08A91A  4021          r0 = 1
0x01E8AA1C  app+0x08A91C  8141          if (r1 != 0) goto 2
0x01E8AA1E  app+0x08A91E  1016          r0 = r1
