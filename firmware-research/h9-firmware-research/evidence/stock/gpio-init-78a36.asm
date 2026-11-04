; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 35; undecoded chunks: 0
0x01E78B36  app+0x078A36  bcea513c      call -493406
0x01E78B3A  app+0x078A3A  70e17054      r0 = r5 & 0xFFFFFFF
0x01E78B3E  app+0x078A3E  00f80504      if (r0 == 2) goto 10
0x01E78B42  app+0x078A42  00f80902      if (r0 == 1) goto 18
0x01E78B46  app+0x078A46  8050          if (r0 != 0) goto 32
0x01E78B48  app+0x078A48  4120          r1 = 0
0x01E78B4A  app+0x078A4A  0481          goto 2
0x01E78B4C  app+0x078A4C  4121          r1 = 1
0x01E78B4E  app+0x078A4E  4016          r0 = r4
0x01E78B50  app+0x078A50  bcea613c      call -493374
0x01E78B54  app+0x078A54  4120          r1 = 0
0x01E78B56  app+0x078A56  0485          goto 10
0x01E78B58  app+0x078A58  4120          r1 = 0
0x01E78B5A  app+0x078A5A  4016          r0 = r4
0x01E78B5C  app+0x078A5C  bcea5b3c      call -493386
0x01E78B60  app+0x078A60  4121          r1 = 1
0x01E78B62  app+0x078A62  4016          r0 = r4
0x01E78B64  app+0x078A64  bcea873b      call -493810
0x01E78B68  app+0x078A68  5504          {pc, r5, r4} = [sp++]
0x01E78B6A  app+0x078A6A  7504          [--sp] = {rets, r5, r4}
0x01E78B6C  app+0x078A6C  e293          sp += -52
0x01E78B6E  app+0x078A6E  1416          r4 = r1
0x01E78B70  app+0x078A70  0516          r5 = r0
0x01E78B72  app+0x078A72  dee93320      b[sp+51] = r2
0x01E78B76  app+0x078A76  a88b          r0 = sp + 43
0x01E78B78  app+0x078A78  bfeafe21      call -113668
0x01E78B7C  app+0x078A7C  8980          r1 = sp + 0
0x01E78B7E  app+0x078A7E  4220          r2 = 0
0x01E78B80  app+0x078A80  1316          r3 = r1
0x01E78B82  app+0x078A82  0088          rep 2 9 {
0x01E78B84  app+0x078A84  b205          [r3++=4] = r2
0x01E78B86  app+0x078A86  c2ff38200100  r2 = 73784
0x01E78B8C  app+0x078A8C  8220          [sp] = r2
0x01E78B8E  app+0x078A8E  c2ffda82e701  r2 = 31949530
0x01E78B94  app+0x078A94  8221          [sp+4] = r2
