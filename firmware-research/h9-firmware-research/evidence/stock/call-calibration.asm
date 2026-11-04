; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 20; undecoded chunks: 0
0x01E610E0  app+0x060FE0  b400          testset b[r4]
0x01E610E2  app+0x060FE2  0043          if (r0 == 0) goto 6
0x01E610E4  app+0x060FE4  d8e9b600      r0 = h[sp+182] (u)
0x01E610E8  app+0x060FE8  804a          if (r0 != 0) goto 20
0x01E610EA  app+0x060FEA  f8e8b440      r4 = sp + 180
0x01E610EE  app+0x060FEE  4016          r0 = r4
0x01E610F0  app+0x060FF0  bfeaac10      call -122536
0x01E610F4  app+0x060FF4  503c          r0 = 92
0x01E610F6  app+0x060FF6  4228          r2 = 8
0x01E610F8  app+0x060FF8  4116          r1 = r4
0x01E610FA  app+0x060FFA  beeacf59      call -216162
0x01E610FE  app+0x060FFE  f8e8b400      r0 = sp + 180
0x01E61102  app+0x061002  bfea6411      call -122168
0x01E61106  app+0x061006  d0ecac0e      r0 = [r10+236]
0x01E6110A  app+0x06100A  8054          if (r0 != 0) goto 40
0x01E6110C  app+0x06100C  482c          r0 = 44
0x01E6110E  app+0x06100E  beeaab7f      call -196778
0x01E61112  app+0x061012  0416          r4 = r0
0x01E61114  app+0x061014  c2ffce1aeb01  r2 = 32185038
0x01E6111A  app+0x06101A  c0ffdaa6e401  r0 = 31762138
