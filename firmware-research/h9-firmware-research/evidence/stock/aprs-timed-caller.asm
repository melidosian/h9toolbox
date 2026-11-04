; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 58; undecoded chunks: 1
0x01E87520  app+0x087420  1404          .byte 0x14, 0x04
0x01E87522  app+0x087422  c021          r0 += 1
0x01E87524  app+0x087424  52ee1404      b[r1+68] = r0
0x01E87528  app+0x087428  f0e80004      sp += 1024
0x01E8752C  app+0x08742C  5f04          {pc, r15-r4} = [sp++]
0x01E8752E  app+0x08742E  7f04          [--sp] = {rets, r15-r4}
0x01E87530  app+0x087430  e28a          sp += -88
0x01E87532  app+0x087432  c4ffb01a0100  r4 = 72368
0x01E87538  app+0x087438  c5ffc61a0100  r5 = 72390
0x01E8753E  app+0x08743E  5940          r1 = b[r5+0] (u)
0x01E87540  app+0x087440  4840          r0 = b[r4+0] (u)
0x01E87542  app+0x087442  c7ffd01a0100  r7 = 72400
0x01E87548  app+0x087448  81f81810      if (r1 != 8) goto 48
0x01E8754C  app+0x08744C  00fc1602      if (r0 > 1) goto 44
0x01E87550  app+0x087450  50ee7913      r1 = b[r7+57] (u)
0x01E87554  app+0x087454  1a81          r2 = r1 + 1
0x01E87556  app+0x087456  52ee7923      b[r7+57] = r2
0x01E8755A  app+0x08745A  81f924c8      if (r1 < 100) goto 72
0x01E8755E  app+0x08745E  4020          r0 = 0
0x01E87560  app+0x087460  52ee7903      b[r7+57] = r0
0x01E87564  app+0x087464  8890          r0 = sp + 16
0x01E87566  app+0x087466  01e1807a      r1 = r7 + 2688
0x01E8756A  app+0x08746A  0216          r2 = r0
0x01E8756C  app+0x08746C  108d          rep 4 14 {
0x01E8756E  app+0x08746E  1305          r3 = [r1++=4]
0x01E87570  app+0x087470  a305          [r2++=4] = r3
0x01E87572  app+0x087472  4120          r1 = 0
0x01E87574  app+0x087474  beeaa465      call -210104
0x01E87578  app+0x087478  4840          r0 = b[r4+0] (u)
0x01E8757A  app+0x08747A  5940          r1 = b[r5+0] (u)
0x01E8757C  app+0x08747C  00fc0e02      if (r0 > 1) goto 28
0x01E87580  app+0x087480  81f80c06      if (r1 != 3) goto 24
0x01E87584  app+0x087484  50ee7b02      r0 = b[r7+43] (u)
0x01E87588  app+0x087488  50ee7e12      r1 = b[r7+46] (u)
0x01E8758C  app+0x08748C  1019          r0 |= r1
0x01E8758E  app+0x08748E  61ffff001800  if ((r0 & 0xFF) != 0) goto 48
0x01E87594  app+0x087494  bfea0ddd      call -17894
0x01E87598  app+0x087498  4840          r0 = b[r4+0] (u)
0x01E8759A  app+0x08749A  5940          r1 = b[r5+0] (u)
0x01E8759C  app+0x08749C  81f80306      if (r1 != 3) goto 6
0x01E875A0  app+0x0874A0  80fc1002      if (r0 <= 1) goto 32
0x01E875A4  app+0x0874A4  1486          goto 76
0x01E875A6  app+0x0874A6  50ee7b12      r1 = b[r7+43] (u)
0x01E875AA  app+0x0874AA  b1e80100      if (r1 != 1) {
0x01E875AE  app+0x0874AE  4120          r1 = 0
0x01E875B0  app+0x0874B0  50ee7e22      r2 = b[r7+46] (u)
0x01E875B4  app+0x0874B4  b2e80100      if (r2 != 1) {
0x01E875B8  app+0x0874B8  4220          r2 = 0
0x01E875BA  app+0x0874BA  2119          r1 |= r2
0x01E875BC  app+0x0874BC  81f81602      if (r1 != 1) goto 44
0x01E875C0  app+0x0874C0  00f91404      if (r0 >= 2) goto 40
0x01E875C4  app+0x0874C4  50ed740a      r0 = h[r7+164] (u)
0x01E875C8  app+0x0874C8  c021          r0 += 1
0x01E875CA  app+0x0874CA  8117          r1 = r0.l (u)
0x01E875CC  app+0x0874CC  50ed750a      h[r7+164] = r0
0x01E875D0  app+0x0874D0  03ff6410b104  if (r1 < 100) goto 2402
0x01E875D6  app+0x0874D6  4020          r0 = 0
0x01E875D8  app+0x0874D8  50ed750a      h[r7+164] = r0
0x01E875DC  app+0x0874DC  50ee7005      r0 = b[r7+80] (u)
