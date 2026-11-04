; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 128; undecoded chunks: 0
0x01E3C980  app+0x03C880  5840          r0 = b[r5+0] (u)
0x01E3C982  app+0x03C882  0052          if (r0 == 0) goto 36
0x01E3C984  app+0x03C884  5162          r1 = [r5+8]
0x01E3C986  app+0x03C886  4420          r4 = 0
0x01E3C988  app+0x03C888  0154          if (r1 == 0) goto 40
0x01E3C98A  app+0x03C88A  06e12c50      r6 = r5 + 44
0x01E3C98E  app+0x03C88E  6a60          r2 = h[r6+0] (u)
0x01E3C990  app+0x03C890  024d          if (r2 == 0) goto 26
0x01E3C992  app+0x03C892  516a          r1 = [r5+40]
0x01E3C994  app+0x03C894  5061          r0 = [r5+4]
0x01E3C996  app+0x03C896  bfea60fe      call -832
0x01E3C99A  app+0x03C89A  6960          r1 = h[r6+0] (u)
0x01E3C99C  app+0x03C89C  111e          r1 = r1 - r0
0x01E3C99E  app+0x03C89E  80d7          r0 = r0.l (u)  #
0x01E3C9A0  app+0x03C8A0  e960          h[r6+0] = r1
0x01E3C9A2  app+0x03C8A2  68e82850      [r5+40] += r0
0x01E3C9A6  app+0x03C8A6  0485          goto 10
0x01E3C9A8  app+0x03C8A8  4c21          r4 = 33
0x01E3C9AA  app+0x03C8AA  0483          goto 6
0x01E3C9AC  app+0x03C8AC  5063          r0 = [r5+12]
0x01E3C9AE  app+0x03C8AE  1164          r1 = [r1+16]
0x01E3C9B0  app+0x03C8B0  c100          call r1
0x01E3C9B2  app+0x03C8B2  4016          r0 = r4
0x01E3C9B4  app+0x03C8B4  5604          {pc, r6-r4} = [sp++]
0x01E3C9B6  app+0x03C8B6  4020          r0 = 0
0x01E3C9B8  app+0x03C8B8  8000          rts
0x01E3C9BA  app+0x03C8BA  7404          [--sp] = {rets, r4}
0x01E3C9BC  app+0x03C8BC  0416          r4 = r0
0x01E3C9BE  app+0x03C8BE  4020          r0 = 0
0x01E3C9C0  app+0x03C8C0  c840          b[r4+0] = r0
0x01E3C9C2  app+0x03C8C2  0447          if (r4 == 0) goto 14
0x01E3C9C4  app+0x03C8C4  4063          r0 = [r4+12]
0x01E3C9C6  app+0x03C8C6  0042          if (r0 == 0) goto 4
0x01E3C9C8  app+0x03C8C8  bfea4da3      call -47462
0x01E3C9CC  app+0x03C8CC  4016          r0 = r4
0x01E3C9CE  app+0x03C8CE  bfea4aa3      call -47468
0x01E3C9D2  app+0x03C8D2  4020          r0 = 0
0x01E3C9D4  app+0x03C8D4  5404          {pc, r4} = [sp++]
0x01E3C9D6  app+0x03C8D6  1004          [--sp] = rets
0x01E3C9D8  app+0x03C8D8  0116          r1 = r0
0x01E3C9DA  app+0x03C8DA  4020          r0 = 0
0x01E3C9DC  app+0x03C8DC  0147          if (r1 == 0) goto 14
0x01E3C9DE  app+0x03C8DE  2260          r2 = [r2+0]
0x01E3C9E0  app+0x03C8E0  32e801c0      if (r2 == 1) {
0x01E3C9E4  app+0x03C8E4  1262          r2 = [r1+8]
0x01E3C9E6  app+0x03C8E6  1063          r0 = [r1+12]
0x01E3C9E8  app+0x03C8E8  2166          r1 = [r2+24]
0x01E3C9EA  app+0x03C8EA  c100          call r1
0x01E3C9EC  app+0x03C8EC  0004          pc = [sp++]
0x01E3C9EE  app+0x03C8EE  1004          [--sp] = rets
0x01E3C9F0  app+0x03C8F0  0116          r1 = r0
0x01E3C9F2  app+0x03C8F2  106b          r0 = [r1+44]
0x01E3C9F4  app+0x03C8F4  d0ec0a28      r2 = [++r0=136]
0x01E3C9F8  app+0x03C8F8  12e80081      if (r2 == r1) {
0x01E3C9FC  app+0x03C8FC  4120          r1 = 0
0x01E3C9FE  app+0x03C8FE  d8ec0112      [r0++=32] = r1
0x01E3CA02  app+0x03C902  80ea2e04      call 2140
0x01E3CA06  app+0x03C906  0004          pc = [sp++]
0x01E3CA08  app+0x03C908  1004          [--sp] = rets
0x01E3CA0A  app+0x03C90A  0216          r2 = r0
0x01E3CA0C  app+0x03C90C  8eea1226      call 1854500
0x01E3CA10  app+0x03C910  2016          r0 = r2
0x01E3CA12  app+0x03C912  0060          r0 = [r0+0]
0x01E3CA14  app+0x03C914  02e80900      if (r0 == r2) goto 18
0x01E3CA18  app+0x03C918  0177          r1 = [r0+-36]
0x01E3CA1A  app+0x03C91A  1b17          r3 = r1.b0 (s)
0x01E3CA1C  app+0x03C91C  83fd0200      ifs (r3 < 0) goto 4
0x01E3CA20  app+0x03C920  51e8f741      if ((r1 & 256) == 0) goto -18
0x01E3CA24  app+0x03C924  8804          {rets} = [sp++]
0x01E3CA26  app+0x03C926  ceea1326      goto 1854502
0x01E3CA2A  app+0x03C92A  8eea1126      call 1854498
0x01E3CA2E  app+0x03C92E  50ed2008      r0 = h[r2+128] (u)
0x01E3CA32  app+0x03C932  b828          r0 &= 4294967039
0x01E3CA34  app+0x03C934  50ed2108      h[r2+128] = r0
0x01E3CA38  app+0x03C938  0004          pc = [sp++]
0x01E3CA3A  app+0x03C93A  7404          [--sp] = {rets, r4}
0x01E3CA3C  app+0x03C93C  0116          r1 = r0
0x01E3CA3E  app+0x03C93E  8eeaf925      call 1854450
0x01E3CA42  app+0x03C942  4220          r2 = 0
0x01E3CA44  app+0x03C944  1016          r0 = r1
0x01E3CA46  app+0x03C946  048b          goto 22
0x01E3CA48  app+0x03C948  0377          r3 = [r0+-36]
0x01E3CA4A  app+0x03C94A  63e10630      r3 = r3 & 0x6
0x01E3CA4E  app+0x03C94E  83f80604      if (r3 != 2) goto 12
0x01E3CA52  app+0x03C952  51ee044e      r4 = b[r0+-28] (u)
0x01E3CA56  app+0x03C956  4321          r3 = 1
0x01E3CA58  app+0x03C958  0441          if (r4 == 0) goto 2
0x01E3CA5A  app+0x03C95A  4320          r3 = 0
0x01E3CA5C  app+0x03C95C  3218          r2 += r3
0x01E3CA5E  app+0x03C95E  0060          r0 = [r0+0]
0x01E3CA60  app+0x03C960  81e8f201      if (r0 != r1) goto -28
0x01E3CA64  app+0x03C964  8eeaf425      call 1854440
0x01E3CA68  app+0x03C968  2016          r0 = r2
0x01E3CA6A  app+0x03C96A  5404          {pc, r4} = [sp++]
0x01E3CA6C  app+0x03C96C  7604          [--sp] = {rets, r6-r4}
0x01E3CA6E  app+0x03C96E  02e19000      r2 = r0 + 144
0x01E3CA72  app+0x03C972  2362          r3 = [r2+8]
0x01E3CA74  app+0x03C974  83f80508      if (r3 != 4) goto 10
0x01E3CA78  app+0x03C978  2161          r1 = [r2+4]
0x01E3CA7A  app+0x03C97A  31e8ff0f      if (r1 == -1) {
0x01E3CA7E  app+0x03C97E  2160          r1 = [r2+0]
0x01E3CA80  app+0x03C980  049a          goto 52
0x01E3CA82  app+0x03C982  4520          r5 = 0
0x01E3CA84  app+0x03C984  0416          r4 = r0
0x01E3CA86  app+0x03C986  0493          goto 38
0x01E3CA88  app+0x03C988  417e          r1 = [r4+-8]
0x01E3CA8A  app+0x03C98A  0149          if (r1 == 0) goto 18
0x01E3CA8C  app+0x03C98C  1616          r6 = r1
0x01E3CA8E  app+0x03C98E  0541          if (r5 == 0) goto 2
0x01E3CA90  app+0x03C990  5616          r6 = r5
0x01E3CA92  app+0x03C992  0347          if (r3 == 0) goto 14
0x01E3CA94  app+0x03C994  03f80802      if (r3 == 1) goto 16
0x01E3CA98  app+0x03C998  83f80804      if (r3 != 2) goto 16
0x01E3CA9C  app+0x03C99C  048c          goto 24
0x01E3CA9E  app+0x03C99E  5116          r1 = r5
0x01E3CAA0  app+0x03C9A0  0485          goto 10
0x01E3CAA2  app+0x03C9A2  34e46011      r1 = umax(r6, r1)
0x01E3CAA6  app+0x03C9A6  0482          goto 4
0x01E3CAA8  app+0x03C9A8  35e46011      r1 = umin(r6, r1)
0x01E3CAAC  app+0x03C9AC  1516          r5 = r1
0x01E3CAAE  app+0x03C9AE  4460          r4 = [r4+0]
0x01E3CAB0  app+0x03C9B0  80e8ea41      if (r4 != r0) goto -44
0x01E3CAB4  app+0x03C9B4  5116          r1 = r5
0x01E3CAB6  app+0x03C9B6  2263          r2 = [r2+12]
0x01E3CAB8  app+0x03C9B8  0242          if (r2 == 0) goto 4
0x01E3CABA  app+0x03C9BA  c200          call r2
0x01E3CABC  app+0x03C9BC  0116          r1 = r0
0x01E3CABE  app+0x03C9BE  8143          if (r1 != 0) goto 6
