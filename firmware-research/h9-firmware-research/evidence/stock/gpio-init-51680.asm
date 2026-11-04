; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 33; undecoded chunks: 1
0x01E51780  app+0x051680  4124          r1 = 4
0x01E51782  app+0x051682  8deabc78      call 1765752
0x01E51786  app+0x051686  c5ffc0de0100  r5 = 122560
0x01E5178C  app+0x05168C  50ee5801      r0 = b[r5+24] (u)
0x01E51790  app+0x051690  80f81b06      if (r0 != 3) goto 54
0x01E51794  app+0x051694  4026          r0 = 6
0x01E51796  app+0x051696  4120          r1 = 0
0x01E51798  app+0x051698  4223          r2 = 3
0x01E5179A  app+0x05169A  4324          r3 = 4
0x01E5179C  app+0x05169C  8deaa174      call 1763650
0x01E517A0  app+0x0516A0  50ee5a05      r0 = b[r5+90] (u)
0x01E517A4  app+0x0516A4  10f808fe      if (r0 == 255) goto 16
0x01E517A8  app+0x0516A8  4121          r1 = 1
0x01E517AA  app+0x0516AA  bdea6475      call -333112
0x01E517AE  app+0x0516AE  50ee5a05      r0 = b[r5+90] (u)
0x01E517B2  app+0x0516B2  4120          r1 = 0
0x01E517B4  app+0x0516B4  bdea2f76      call -332706
0x01E517B8  app+0x0516B8  4021          r0 = 1
0x01E517BA  app+0x0516BA  8dea2479      call 1765960
0x01E517BE  app+0x0516BE  4026          r0 = 6
0x01E517C0  app+0x0516C0  4120          r1 = 0
0x01E517C2  app+0x0516C2  4223          r2 = 3
0x01E517C4  app+0x0516C4  4323          r3 = 3
0x01E517C6  app+0x0516C6  8dea8c74      call 1763608
0x01E517CA  app+0x0516CA  4940          r1 = b[r4+0] (u)
0x01E517CC  app+0x0516CC  4022          r0 = 2
0x01E517CE  app+0x0516CE  8141          if (r1 != 0) goto 2
0x01E517D0  app+0x0516D0  103f          r0 = -1
0x01E517D2  app+0x0516D2  52ee5801      b[r5+24] = r0
0x01E517D6  app+0x0516D6  5504          {pc, r5, r4} = [sp++]
0x01E517D8  app+0x0516D8  7704          [--sp] = {rets, r7-r4}
0x01E517DA  app+0x0516DA  c0ffc0de0100  r0 = 122560
0x01E517E0  app+0x0516E0  d0ec0414      r1 = [r0+68]
0x01E517E4  app+0x0516E4  8d            .byte 0x8D
