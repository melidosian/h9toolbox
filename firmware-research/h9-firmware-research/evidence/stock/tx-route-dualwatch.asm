; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 27; undecoded chunks: 0
0x01E58E3E  app+0x058D3E  7404          [--sp] = {rets, r4}
0x01E58E40  app+0x058D40  0416          r4 = r0
0x01E58E42  app+0x058D42  bfeafceb      call -10248
0x01E58E46  app+0x058D46  4016          r0 = r4
0x01E58E48  app+0x058D48  bfea2eec      call -10148
0x01E58E4C  app+0x058D4C  41e0d710      r1 = 4311
0x01E58E50  app+0x058D50  c0ffd01a0100  r0 = 72400
0x01E58E56  app+0x058D56  d8ee0011      r1 = b[r0+r1] (u)
0x01E58E5A  app+0x058D5A  0144          if (r1 == 0) goto 8
0x01E58E5C  app+0x058D5C  50ee0c01      r0 = b[r0+28] (u)
0x01E58E60  app+0x058D60  8048          if (r0 != 0) goto 16
0x01E58E62  app+0x058D62  0484          goto 8
0x01E58E64  app+0x058D64  54ee0008      r0 = b[r0+128] (s)
0x01E58E68  app+0x058D68  80fd0300      ifs (r0 < 0) goto 6
0x01E58E6C  app+0x058D6C  bfea29ed      call -9646
0x01E58E70  app+0x058D70  0482          goto 4
0x01E58E72  app+0x058D72  bfeaaeed      call -9380
0x01E58E76  app+0x058D76  bfea47ee      call -9074
0x01E58E7A  app+0x058D7A  bfea79ee      call -8974
0x01E58E7E  app+0x058D7E  3404          {rets, r4} = [sp++]
0x01E58E80  app+0x058D80  ffea86f1      goto -7412
0x01E58E84  app+0x058D84  7404          [--sp] = {rets, r4}
0x01E58E86  app+0x058D86  e29f          sp += -4
0x01E58E88  app+0x058D88  c0fff0c00000  r0 = 49392
0x01E58E8E  app+0x058D8E  4420          r4 = 0
0x01E58E90  app+0x058D90  52ee0a44      b[r0+74] = r4
0x01E58E94  app+0x058D94  52ee0c44      b[r0+76] = r4
