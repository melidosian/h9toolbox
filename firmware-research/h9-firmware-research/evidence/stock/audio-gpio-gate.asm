; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 21; undecoded chunks: 0
0x01E52320  app+0x052220  4420          r4 = 0
0x01E52322  app+0x052222  01f88903      if (r1 == 1) goto -238
0x01E52326  app+0x052226  5504          {pc, r5, r4} = [sp++]
0x01E52328  app+0x052228  7404          [--sp] = {rets, r4}
0x01E5232A  app+0x05222A  0416          r4 = r0
0x01E5232C  app+0x05222C  4023          r0 = 3
0x01E5232E  app+0x05222E  4116          r1 = r4
0x01E52330  app+0x052230  bdea4f6f      call -336226
0x01E52334  app+0x052234  c0ffd01a0100  r0 = 72400
0x01E5233A  app+0x05223A  50ee0d17      r1 = b[r0+125] (u)
0x01E5233E  app+0x05223E  72e1fe40      r2 = r4 & 0xFFFFFF01
0x01E52342  app+0x052242  b920          r1 &= 4294967294
0x01E52344  app+0x052244  2119          r1 |= r2
0x01E52346  app+0x052246  52ee0d17      b[r0+125] = r1
0x01E5234A  app+0x05224A  5404          {pc, r4} = [sp++]
0x01E5234C  app+0x05224C  7504          [--sp] = {rets, r5, r4}
0x01E5234E  app+0x05224E  c5ffd01a0100  r5 = 72400
0x01E52354  app+0x052254  50ee5d27      r2 = b[r5+125] (u)
0x01E52358  app+0x052258  73e1fe20      r3 = r2 & 0xFFFFFF01
0x01E5235C  app+0x05225C  1416          r4 = r1
0x01E5235E  app+0x05225E  04e85b30      if (r3 == r4) goto 182
