; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 49; undecoded chunks: 1
0x01E5AB10  app+0x05AA10  1f04          .byte 0x1F, 0x04
0x01E5AB12  app+0x05AA12  8000          rts
0x01E5AB14  app+0x05AA14  7504          [--sp] = {rets, r5, r4}
0x01E5AB16  app+0x05AA16  41e0e410      r1 = 4324
0x01E5AB1A  app+0x05AA1A  c5ffd01a0100  r5 = 72400
0x01E5AB20  app+0x05AA20  d8ee5011      r1 = b[r5+r1] (u)
0x01E5AB24  app+0x05AA24  0416          r4 = r0
0x01E5AB26  app+0x05AA26  31e80180      if (r1 == 1) {
0x01E5AB2A  app+0x05AA2A  4028          r0 = 8
0x01E5AB2C  app+0x05AA2C  4120          r1 = 0
0x01E5AB2E  app+0x05AA2E  bfea0dbc      call -34790
0x01E5AB32  app+0x05AA32  4832          r0 = 50
0x01E5AB34  app+0x05AA34  8dea642f      call 1728200
0x01E5AB38  app+0x05AA38  4021          r0 = 1
0x01E5AB3A  app+0x05AA3A  bfea53f4      call -5978
0x01E5AB3E  app+0x05AA3E  bfea49f2      call -7022
0x01E5AB42  app+0x05AA42  5830          r0 = 112
0x01E5AB44  app+0x05AA44  4120          r1 = 0
0x01E5AB46  app+0x05AA46  bfeae0ad      call -42048
0x01E5AB4A  app+0x05AA4A  5028          r0 = 72
0x01E5AB4C  app+0x05AA4C  4116          r1 = r4
0x01E5AB4E  app+0x05AA4E  bfeadcad      call -42056
0x01E5AB52  app+0x05AA52  403e          r0 = 30
0x01E5AB54  app+0x05AA54  52ee5604      b[r5+70] = r0
0x01E5AB58  app+0x05AA58  5504          {pc, r5, r4} = [sp++]
0x01E5AB5A  app+0x05AA5A  7c04          [--sp] = {rets, r12-r4}
0x01E5AB5C  app+0x05AA5C  c4ffd01a0100  r4 = 72400
0x01E5AB62  app+0x05AA62  13e1fa40      r3 = r4 + 4346
0x01E5AB66  app+0x05AA66  12e10241      r2 = r4 + 4354
0x01E5AB6A  app+0x05AA6A  0a16          r10 = r0
0x01E5AB6C  app+0x05AA6C  0141          if (r1 == 0) goto 2
0x01E5AB6E  app+0x05AA6E  3216          r2 = r3
0x01E5AB70  app+0x05AA70  2d40          r5 = b[r2+0] (u)
0x01E5AB72  app+0x05AA72  bfea79ff      call -270
0x01E5AB76  app+0x05AA76  41e0f401      r1 = 500
0x01E5AB7A  app+0x05AA7A  d8ee4011      r1 = b[r4+r1] (u)
0x01E5AB7E  app+0x05AA7E  0816          r8 = r0
0x01E5AB80  app+0x05AA80  014a          if (r1 == 0) goto 20
0x01E5AB82  app+0x05AA82  4021          r0 = 1
0x01E5AB84  app+0x05AA84  01e1d145      r1 = r4 + 1489
0x01E5AB88  app+0x05AA88  c4d4          r12 = 0  #
0x01E5AB8A  app+0x05AA8A  9840          b[r1+0] = r0
0x01E5AB8C  app+0x05AA8C  9841          b[r1+1] = r0
0x01E5AB8E  app+0x05AA8E  4029          r0 = 9
0x01E5AB90  app+0x05AA90  9842          b[r1+2] = r0
0x01E5AB92  app+0x05AA92  4023          r0 = 3
0x01E5AB94  app+0x05AA94  0483          goto 6
0x01E5AB96  app+0x05AA96  5883          r0 = r5 + 3
0x01E5AB98  app+0x05AA98  4ce00300      r12 = 3
0x01E5AB9C  app+0x05AA9C  41e00711      r1 = 4359
