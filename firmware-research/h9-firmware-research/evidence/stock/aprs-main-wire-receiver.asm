; Selected independent Pi32v2 r3 decode of stock v1.0.38 app.bin.
; SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; The 0x01E00100 virtual address base is inferred from JLFS entry metadata.
; app+0x087218 polls the UART ring and runs a four-state frame receiver.
; r5 = 88560, r4 = 256, r9 = 1027, r12 = 1028, r15 = 1029.
; [r5+r4<<2] is state; b[r5+1028] is command;
; h[r5+1027<<1] is length; b[r5+1029+index] is payload.

app+0x087218  7f04          [--sp] = {rets, r15-r4}
app+0x08721a  f0e8001c      sp += -1024
app+0x08733a  48e0c000      r8 = 192
app+0x087340  d8ec5a14      r1 = [r5+r4<<2]
app+0x087344  7007          r0 = b[r7++=1] (u)
app+0x087346  01fc5806      if (r1 > 3) goto 176
app+0x08734a  0101          tbb [r1]
app+0x087350  30e8c040      if (r0 == 192) {
app+0x087354  d8ed59a9      h[r5+r9<<1] = r10
app+0x087358  d8ec5bb4      [r5+r4<<2] = r11
app+0x08735e  b0e8c040      if (r0 != 192) {
app+0x087362  d8ee510c      b[r5+r12] = r0
app+0x087366  d8ec5bd4      [r5+r4<<2] = r13
app+0x08736c  10f81680      if (r0 == 192) goto 44
app+0x087370  90f81fb6      if (r0 != 219) goto 62
app+0x087374  d8ec5be4      [r5+r4<<2] = r14
app+0x08737a  10f81fba      if (r0 == 221) goto 62
app+0x08737e  90f830b8      if (r0 != 220) goto 96
app+0x087386  70fc27fe      if (r0 > 1023) goto 78
app+0x087392  d8ee018f      b[r0+r15] = r8
app+0x087396  d8ec5bd4      [r5+r4<<2] = r13
app+0x08739c  d8ec5ba4      [r5+r4<<2] = r10
app+0x0873a0  d8ed5819      r1 = h[r5+r9<<1] (u)
app+0x0873a4  d8ee500c      r0 = b[r5+r12] (u)
app+0x0873a8  bfea88fe      call -752 ; app+0x0870bc command dispatch
app+0x0873b6  71fc0ffe      if (r1 > 1023) goto 30
app+0x0873cc  713b          r1 = 219
app+0x0873ce  d8ee011f      b[r0+r15] = r1
app+0x0873d8  d8ec5ba4      [r5+r4<<2] = r10
app+0x0873dc  d8ed59a9      h[r5+r9<<1] = r10
app+0x0873e2  d8ec5bd4      [r5+r4<<2] = r13
app+0x0873ea  71fc06fe      if (r1 > 1023) goto 12
app+0x0873f6  d8ee110f      b[r1+r15] = r0

; The dispatcher branches on command 0x00, 0x10, 0x11, 0x21, and 0x51.
app+0x0870bc  7704          [--sp] = {rets, r7-r4}
app+0x0870c6  00f81da2      if (r0 == 81) goto 58
app+0x0870ca  00f84b20      if (r0 == 16) goto 150
app+0x0870ce  00f84e22      if (r0 == 17) goto 156
app+0x0870d2  00f85642      if (r0 == 33) goto 172
app+0x0870d6  80f88200      if (r0 != 0) goto 260
