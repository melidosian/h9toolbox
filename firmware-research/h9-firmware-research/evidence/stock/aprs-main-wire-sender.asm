; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 18; undecoded chunks: 0
0x01E5B4BE  app+0x05B3BE  7404          [--sp] = {rets, r4}
0x01E5B4C0  app+0x05B3C0  f0e8fc17      sp += -2052
0x01E5B4C4  app+0x05B3C4  1216          r2 = r1
0x01E5B4C6  app+0x05B3C6  0116          r1 = r0
0x01E5B4C8  app+0x05B3C8  40e00008      r0 = 2048
0x01E5B4CC  app+0x05B3CC  8020          [sp] = r0
0x01E5B4CE  app+0x05B3CE  8b84          r3 = sp + 4
0x01E5B4D0  app+0x05B3D0  4020          r0 = 0
0x01E5B4D2  app+0x05B3D2  4420          r4 = 0
0x01E5B4D4  app+0x05B3D4  518c          call -168
0x01E5B4D6  app+0x05B3D6  0045          if (r0 == 0) goto 10
0x01E5B4D8  app+0x05B3D8  71e17f0c      r1 = r0 & 0xFFFF00FF
0x01E5B4DC  app+0x05B3DC  8884          r0 = sp + 4
0x01E5B4DE  app+0x05B3DE  7185          call -54
0x01E5B4E0  app+0x05B3E0  4421          r4 = 1
0x01E5B4E2  app+0x05B3E2  4016          r0 = r4
0x01E5B4E4  app+0x05B3E4  f0e80408      sp += 2052
0x01E5B4E8  app+0x05B3E8  5404          {pc, r4} = [sp++]
