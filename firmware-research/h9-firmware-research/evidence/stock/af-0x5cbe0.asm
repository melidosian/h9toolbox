; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 43; undecoded chunks: 0
0x01E5CCE0  app+0x05CBE0  50ee4a01      r0 = b[r4+26] (u)
0x01E5CCE4  app+0x05CBE4  004f          if (r0 == 0) goto 30
0x01E5CCE6  app+0x05CBE6  40e00e06      r0 = 1550
0x01E5CCEA  app+0x05CBEA  0485          goto 10
0x01E5CCEC  app+0x05CBEC  50ee4901      r0 = b[r4+25] (u)
0x01E5CCF0  app+0x05CBF0  0049          if (r0 == 0) goto 18
0x01E5CCF2  app+0x05CBF2  40e0fe05      r0 = 1534
0x01E5CCF6  app+0x05CBF6  d8ee4200      r0 = b[r4+r0] (s)
0x01E5CCFA  app+0x05CBFA  b0eeff4f      ifs (r0 <= -1) {
0x01E5CCFE  app+0x05CBFE  4020          r0 = 0
0x01E5CD00  app+0x05CC00  bfea88ef      call -8432
0x01E5CD04  app+0x05CC04  45e0d310      r5 = 4307
0x01E5CD08  app+0x05CC08  d8ee4005      r0 = b[r4+r5] (u)
0x01E5CD0C  app+0x05CC0C  105d          if (r0 == 0) goto 122
0x01E5CD0E  app+0x05CC0E  4023          r0 = 3
0x01E5CD10  app+0x05CC10  bfea68e3      call -14640
0x01E5CD14  app+0x05CC14  5830          r0 = 112
0x01E5CD16  app+0x05CC16  60e0401c      r1 = 0xC000
0x01E5CD1A  app+0x05CC1A  bfeaf69c      call -50708
0x01E5CD1E  app+0x05CC1E  bfea87ee      call -8946
0x01E5CD22  app+0x05CC22  d8ee4005      r0 = b[r4+r5] (u)
0x01E5CD26  app+0x05CC26  00f80d04      if (r0 == 2) goto 26
0x01E5CD2A  app+0x05CC2A  80f82b02      if (r0 != 1) goto 86
0x01E5CD2E  app+0x05CC2E  5831          r0 = 113
0x01E5CD30  app+0x05CC30  41e08f44      r1 = 17551
0x01E5CD34  app+0x05CC34  bfeae99c      call -50734
0x01E5CD38  app+0x05CC38  bfea4ce1      call -15720
0x01E5CD3C  app+0x05CC3C  7028          r0 = 200
0x01E5CD3E  app+0x05CC3E  bfeac2a4      call -46716
0x01E5CD42  app+0x05CC42  1480          goto 64
0x01E5CD44  app+0x05CC44  bfea46e1      call -15732
0x01E5CD48  app+0x05CC48  5831          r0 = 113
0x01E5CD4A  app+0x05CC4A  41e07049      r1 = 18800
0x01E5CD4E  app+0x05CC4E  bfeadc9c      call -50760
0x01E5CD52  app+0x05CC52  4025          r0 = 5
0x01E5CD54  app+0x05CC54  8dea541e      call 1719464
0x01E5CD58  app+0x05CC58  bfea6aee      call -9004
0x01E5CD5C  app+0x05CC5C  4034          r0 = 20
0x01E5CD5E  app+0x05CC5E  8dea4f1e      call 1719454
0x01E5CD62  app+0x05CC62  bfea37e1      call -15762
0x01E5CD66  app+0x05CC66  5831          r0 = 113
0x01E5CD68  app+0x05CC68  41e00949      r1 = 18697
0x01E5CD6C  app+0x05CC6C  bfeacd9c      call -50790
