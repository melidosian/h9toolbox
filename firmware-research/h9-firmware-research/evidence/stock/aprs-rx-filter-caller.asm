; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 19; undecoded chunks: 0
0x01E871C0  app+0x0870C0  c4fff0590100  r4 = 88560
0x01E871C6  app+0x0870C6  00f81da2      if (r0 == 81) goto 58
0x01E871CA  app+0x0870CA  00f84b20      if (r0 == 16) goto 150
0x01E871CE  app+0x0870CE  00f84e22      if (r0 == 17) goto 156
0x01E871D2  app+0x0870D2  00f85642      if (r0 == 33) goto 172
0x01E871D6  app+0x0870D6  80f88200      if (r0 != 0) goto 260
0x01E871DA  app+0x0870DA  00e10544      r0 = r4 + 1029
0x01E871DE  app+0x0870DE  8a80          r2 = sp + 0
0x01E871E0  app+0x0870E0  beea89b4      call -169710
0x01E871E4  app+0x0870E4  305c          if (r0 == 0) goto 248
0x01E871E6  app+0x0870E6  d8e93c01      r0 = h[sp+316] (u)
0x01E871EA  app+0x0870EA  3059          if (r0 == 0) goto 242
0x01E871EC  app+0x0870EC  dce93a00      r0 = b[sp+58] (u)
0x01E871F0  app+0x0870F0  10f804e0      if (r0 == 240) goto 8
0x01E871F4  app+0x0870F4  70e10f00      r0 = r0 & 0xFFFFFFF0
0x01E871F8  app+0x0870F8  80f87160      if (r0 != 48) goto 226
0x01E871FC  app+0x0870FC  8880          r0 = sp + 0
0x01E871FE  app+0x0870FE  bfea1bf8      call -4042
0x01E87202  app+0x087102  348d          goto 218
