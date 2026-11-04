; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 332; undecoded chunks: 1
0x01E61000  app+0x060F00  0b10          b[r0++=r8] = r3
0x01E61002  app+0x060F02  0403          rep 2 r4 {
0x01E61004  app+0x060F04  f797          goto -18
0x01E61006  app+0x060F06  40e03411      r0 = 4404
0x01E6100A  app+0x060F0A  d8ee1000      r0 = b[r1+r0] (u)
0x01E6100E  app+0x060F0E  048f          goto 30
0x01E61010  app+0x060F10  4020          r0 = 0
0x01E61012  app+0x060F12  048d          goto 26
0x01E61014  app+0x060F14  40e03511      r0 = 4405
0x01E61018  app+0x060F18  d8ee1000      r0 = b[r1+r0] (u)
0x01E6101C  app+0x060F1C  0488          goto 16
0x01E6101E  app+0x060F1E  40e03411      r0 = 4404
0x01E61022  app+0x060F22  d8ee1000      r0 = b[r1+r0] (u)
0x01E61026  app+0x060F26  42e03611      r2 = 4406
0x01E6102A  app+0x060F2A  d8ee1102      b[r1+r2] = r0
0x01E6102E  app+0x060F2E  0817          r0 = r0.b0 (s)
0x01E61030  app+0x060F30  5404          {pc, r4} = [sp++]
0x01E61032  app+0x060F32  7704          [--sp] = {rets, r7-r4}
0x01E61034  app+0x060F34  c6ff38260100  r6 = 75320
0x01E6103A  app+0x060F3A  14d6          r4 = r1  #
0x01E6103C  app+0x060F3C  6a40          r2 = b[r6+0] (u)
0x01E6103E  app+0x060F3E  05d6          r5 = r0  #
0x01E61040  app+0x060F40  ea41          b[r6+1] = r2
0x01E61042  app+0x060F42  6716          r7 = r6
0x01E61044  app+0x060F44  d2ee7053      b[r7++=48] = r5
0x01E61048  app+0x060F48  618d          call -102
0x01E6104A  app+0x060F4A  0116          r1 = r0
0x01E6104C  app+0x060F4C  5016          r0 = r5
0x01E6104E  app+0x060F4E  bfea44ff      call -376
0x01E61052  app+0x060F52  40e00040      r0 = 16384
0x01E61056  app+0x060F56  01e12260      r1 = r6 + 34
0x01E6105A  app+0x060F5A  9860          h[r1+0] = r0
0x01E6105C  app+0x060F5C  9861          h[r1+2] = r0
0x01E6105E  app+0x060F5E  9862          h[r1+4] = r0
0x01E61060  app+0x060F60  9863          h[r1+6] = r0
0x01E61062  app+0x060F62  d8ed7945      h[r7+r5<<1] = r4
0x01E61066  app+0x060F66  5704          {pc, r7-r4} = [sp++]
0x01E61068  app+0x060F68  30e80290      if (r0 == 2) {
0x01E6106C  app+0x060F6C  41e03811      r1 = 4408
0x01E61070  app+0x060F70  c2ffd01a0100  r2 = 72400
0x01E61076  app+0x060F76  d8ee2011      r1 = b[r2+r1] (u)
0x01E6107A  app+0x060F7A  413e          r1 = 30
0x01E6107C  app+0x060F7C  e79a          goto -76
0x01E6107E  app+0x060F7E  7a04          [--sp] = {rets, r10-r4}
0x01E61080  app+0x060F80  c28b          sp += -212
0x01E61082  app+0x060F82  beea5bd8      call -151370
0x01E61086  app+0x060F86  caffd01a0100  r10 = 72400
0x01E6108C  app+0x060F8C  4021          r0 = 1
0x01E6108E  app+0x060F8E  01e1c8a9      r1 = r10 + 2504
0x01E61092  app+0x060F92  9840          b[r1+0] = r0
0x01E61094  app+0x060F94  4024          r0 = 4
0x01E61096  app+0x060F96  9861          h[r1+2] = r0
0x01E61098  app+0x060F98  7028          r0 = 200
0x01E6109A  app+0x060F9A  9862          h[r1+4] = r0
0x01E6109C  app+0x060F9C  bfeaa00e      call -123584
0x01E610A0  app+0x060FA0  44e0ff7f      r4 = 32767
0x01E610A4  app+0x060FA4  d8e9bf40      h[sp+190] = r4
0x01E610A8  app+0x060FA8  f8e8be10      r1 = sp + 190
0x01E610AC  app+0x060FAC  5038          r0 = 88
0x01E610AE  app+0x060FAE  4222          r2 = 2
0x01E610B0  app+0x060FB0  beea3b5a      call -215946
0x01E610B4  app+0x060FB4  30e80200      if (r0 == 2) {
0x01E610B8  app+0x060FB8  d8e9be40      r4 = h[sp+190] (u)
0x01E610BC  app+0x060FBC  c817          r0 = r4.l (s)
0x01E610BE  app+0x060FBE  bfea140f      call -123352
0x01E610C2  app+0x060FC2  bfea1e0f      call -123332
0x01E610C6  app+0x060FC6  c014          r8 = 0
0x01E610C8  app+0x060FC8  c114          r9 = 0
0x01E610CA  app+0x060FCA  d0e9b580      d[sp+180] = r9_r8
0x01E610CE  app+0x060FCE  f8e8b410      r1 = sp + 180
0x01E610D2  app+0x060FD2  503c          r0 = 92
0x01E610D4  app+0x060FD4  4228          r2 = 8
0x01E610D6  app+0x060FD6  beea285a      call -215984
0x01E610DA  app+0x060FDA  80f80610      if (r0 != 8) goto 12
0x01E610DE  app+0x060FDE  d8e9b400      r0 = h[sp+180] (u)
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
0x01E61120  app+0x061020  4116          r1 = r4
0x01E61122  app+0x061022  beea7e5f      call -213252
0x01E61126  app+0x061026  0044          if (r0 == 0) goto 8
0x01E61128  app+0x061028  4016          r0 = r4
0x01E6112A  app+0x06102A  beea9c7f      call -196808
0x01E6112E  app+0x06102E  0482          goto 4
0x01E61130  app+0x061030  d0ecad4e      [r10+236] = r4
0x01E61134  app+0x061034  bfea261d      call -116148
0x01E61138  app+0x061038  14e114a5      r4 = r10 + 5396
0x01E6113C  app+0x06103C  4016          r0 = r4
0x01E6113E  app+0x06103E  beea4bdd      call -148842
0x01E61142  app+0x061042  c1ffc2a9e801  r1 = 32025026
0x01E61148  app+0x061048  4016          r0 = r4
0x01E6114A  app+0x06104A  beea74dd      call -148760
0x01E6114E  app+0x06104E  c1ff22aae801  r1 = 32025122
0x01E61154  app+0x061054  beea72dd      call -148764
0x01E61158  app+0x061058  c1ff24e30000  r1 = 58148
0x01E6115E  app+0x06105E  42e00002      r2 = 512
0x01E61162  app+0x061062  beea6edd      call -148772
0x01E61166  app+0x061066  41e00001      r1 = 256
0x01E6116A  app+0x06106A  4016          r0 = r4
0x01E6116C  app+0x06106C  beea74dd      call -148760
0x01E61170  app+0x061070  bfeaa64a      call -92852
0x01E61174  app+0x061074  0117          r1 = r0.b0 (u)
0x01E61176  app+0x061076  4016          r0 = r4
0x01E61178  app+0x061078  beea76dd      call -148756
0x01E6117C  app+0x06107C  d2eca40d      r0 = [r10+724]
0x01E61180  app+0x061080  d0e9ad80      d[sp+172] = r9_r8
0x01E61184  app+0x061084  d0e9a580      d[sp+164] = r9_r8
0x01E61188  app+0x061088  0043          if (r0 == 0) goto 6
0x01E6118A  app+0x06108A  d2eca85d      r5 = [r10+728]
0x01E6118E  app+0x06108E  1488          goto 80
0x01E61190  app+0x061090  4025          r0 = 5
0x01E61192  app+0x061092  d8e9c100      h[sp+192] = r0
0x01E61196  app+0x061096  402a          r0 = 10
0x01E61198  app+0x061098  d8e9c300      h[sp+194] = r0
0x01E6119C  app+0x06109C  7128          r1 = 200
0x01E6119E  app+0x06109E  d8e9c510      h[sp+196] = r1
0x01E611A2  app+0x0610A2  4932          r1 = 50
0x01E611A4  app+0x0610A4  d8e9c710      h[sp+198] = r1
0x01E611A8  app+0x0610A8  d8e9d100      h[sp+208] = r0
0x01E611AC  app+0x0610AC  c0ff44ac0000  r0 = 44100
0x01E611B2  app+0x0610B2  a032          [sp+200] = r0
0x01E611B4  app+0x0610B4  c0ff9aaae801  r0 = 32025242
0x01E611BA  app+0x0610BA  a033          [sp+204] = r0
0x01E611BC  app+0x0610BC  bfea75fe      call -790
0x01E611C0  app+0x0610C0  dee9d200      b[sp+210] = r0
0x01E611C4  app+0x0610C4  4028          r0 = 8
0x01E611C6  app+0x0610C6  dee9d300      b[sp+211] = r0
0x01E611CA  app+0x0610CA  f8e8c000      r0 = sp + 192
0x01E611CE  app+0x0610CE  beea97fc      call -132818
0x01E611D2  app+0x0610D2  d2eca50d      [r10+724] = r0
0x01E611D6  app+0x0610D6  beea34fd      call -132504
0x01E611DA  app+0x0610DA  0516          r5 = r0
0x01E611DC  app+0x0610DC  d2eca95d      [r10+728] = r5
0x01E611E0  app+0x0610E0  10e1bca5      r0 = r10 + 5564
0x01E611E4  app+0x0610E4  07f1e8a9      r7 = r10 + 2536  #
0x01E611E8  app+0x0610E8  a025          [sp+148] = r0
0x01E611EA  app+0x0610EA  c0ff54aae801  r0 = 32025172
0x01E611F0  app+0x0610F0  f069          [r7+36] = r0
0x01E611F2  app+0x0610F2  c0ff56aae801  r0 = 32025174
0x01E611F8  app+0x0610F8  f068          [r7+32] = r0
0x01E611FA  app+0x0610FA  a726          [sp+152] = r7
0x01E611FC  app+0x0610FC  a527          [sp+156] = r5
0x01E611FE  app+0x0610FE  bfeaf510      call -122390
0x01E61202  app+0x061102  f8e88c30      r3 = sp + 140
0x01E61206  app+0x061106  3016          r0 = r3
0x01E61208  app+0x061108  bfea1111      call -122334
0x01E6120C  app+0x06110C  4832          r0 = 50
0x01E6120E  app+0x06110E  d8e98f00      h[sp+142] = r0
0x01E61212  app+0x061112  4028          r0 = 8
0x01E61214  app+0x061114  d8e99100      h[sp+144] = r0
0x01E61218  app+0x061118  c014          r8 = 0
0x01E6121A  app+0x06111A  dee98c80      b[sp+140] = r8
0x01E6121E  app+0x06111E  3016          r0 = r3
0x01E61220  app+0x061120  bfea0d11      call -122342
0x01E61224  app+0x061124  16e19ca0      r6 = r10 + 4252
0x01E61228  app+0x061128  40d6          r0 = r4  #
0x01E6122A  app+0x06112A  a628          [sp+160] = r6
0x01E6122C  app+0x06112C  c1ff3e510000  r1 = 20798
0x01E61232  app+0x061132  beea3fe0      call -147330
0x01E61236  app+0x061136  41e06e05      r1 = 1390
0x01E6123A  app+0x06113A  d8ecab01      [r10+r1<<2] = r0
0x01E6123E  app+0x06113E  f8e89410      r1 = sp + 148
0x01E61242  app+0x061142  4224          r2 = 4
0x01E61244  app+0x061144  beea8ae0      call -147180
0x01E61248  app+0x061148  c0ff289be801  r0 = 32021288
0x01E6124E  app+0x06114E  40f03411      r0 = 4404  #
0x01E61252  app+0x061152  e067          [r6+28] = r0
0x01E61254  app+0x061154  d8eea000      r0 = b[r10+r0] (u)
0x01E61258  app+0x061158  dee98000      b[sp+128] = r0
0x01E6125C  app+0x06115C  403e          r0 = 30
0x01E6125E  app+0x06115E  dee98100      b[sp+129] = r0
0x01E61262  app+0x061162  4022          r0 = 2
0x01E61264  app+0x061164  dee98200      b[sp+130] = r0
0x01E61268  app+0x061168  49e00100      r9 = 1
0x01E6126C  app+0x06116C  dee98390      b[sp+131] = r9
0x01E61270  app+0x061170  4025          r0 = 5
0x01E61272  app+0x061172  d8e98500      h[sp+132] = r0
0x01E61276  app+0x061176  402a          r0 = 10
0x01E61278  app+0x061178  d8e98700      h[sp+134] = r0
0x01E6127C  app+0x06117C  d4e98980      [sp+136] = r8
0x01E61280  app+0x061180  f8e88000      r0 = sp + 128
0x01E61284  app+0x061184  beeaa0fb      call -133312
0x01E61288  app+0x061188  04e1d0a2      r4 = r10 + 720
0x01E6128C  app+0x06118C  c063          [r4+12] = r0
0x01E6128E  app+0x06118E  beeaeffb      call -133154
0x01E61292  app+0x061192  0116          r1 = r0
0x01E61294  app+0x061194  50d6          r0 = r5  #
0x01E61296  app+0x061196  c164          [r4+16] = r1
0x01E61298  app+0x061198  beea18e0      call -147408
0x01E6129C  app+0x06119C  40e08e02      r0 = 654
0x01E612A0  app+0x0611A0  c1ffbebfe801  r1 = 32030654
0x01E612A6  app+0x0611A6  d8ecab10      [r10+r0<<2] = r1
0x01E612AA  app+0x0611AA  01f13070      r1 = r7 + 48  #
0x01E612AE  app+0x0611AE  4064          r0 = [r4+16]
0x01E612B0  app+0x0611B0  beea0ce0      call -147432
0x01E612B4  app+0x0611B4  54ee6819      r1 = b[r6+152] (s)
0x01E612B8  app+0x0611B8  4021          r0 = 1
0x01E612BA  app+0x0611BA  bfea0efe      call -996
0x01E612BE  app+0x0611BE  4021          r0 = 1
0x01E612C0  app+0x0611C0  bfead2fe      call -604
0x01E612C4  app+0x0611C4  52ee4090      b[r4+0] = r9
0x01E612C8  app+0x0611C8  14e1eca9      r4 = r10 + 6636
0x01E612CC  app+0x0611CC  41e0401f      r1 = 8000
0x01E612D0  app+0x0611D0  4016          r0 = r4
0x01E612D2  app+0x0611D2  bfeaf910      call -122382
0x01E612D6  app+0x0611D6  4016          r0 = r4
0x01E612D8  app+0x0611D8  bfea2c11      call -122280
0x01E612DC  app+0x0611DC  8880          r0 = sp + 0
0x01E612DE  app+0x0611DE  bfeadb11      call -121930
0x01E612E2  app+0x0611E2  bfea3812      call -121744
0x01E612E6  app+0x0611E6  2295          sp += 212
0x01E612E8  app+0x0611E8  5a04          {pc, r10-r4} = [sp++]
0x01E612EA  app+0x0611EA  7404          [--sp] = {rets, r4}
0x01E612EC  app+0x0611EC  42e0690b      r2 = 2921
0x01E612F0  app+0x0611F0  c1ffd01a0100  r1 = 72400
0x01E612F6  app+0x0611F6  43e0680b      r3 = 2920
0x01E612FA  app+0x0611FA  d8ee1043      r4 = b[r1+r3] (u)
0x01E612FE  app+0x0611FE  d8ee1022      r2 = b[r1+r2] (u)
0x01E61302  app+0x061202  80e80440      if (r4 != r0) goto 8
0x01E61306  app+0x061206  d8ee1123      b[r1+r3] = r2
0x01E6130A  app+0x06120A  2416          r4 = r2
0x01E6130C  app+0x06120C  0482          goto 4
0x01E6130E  app+0x06120E  80e80420      if (r2 != r0) goto 8
0x01E61312  app+0x061212  3881          r0 = r3 + 1
0x01E61314  app+0x061214  4220          r2 = 0
0x01E61316  app+0x061216  d8ee1120      b[r1+r0] = r2
0x01E6131A  app+0x06121A  4016          r0 = r4
0x01E6131C  app+0x06121C  bfea62fe      call -828
0x01E61320  app+0x061220  0116          r1 = r0
0x01E61322  app+0x061222  4016          r0 = r4
0x01E61324  app+0x061224  3404          {rets, r4} = [sp++]
0x01E61326  app+0x061226  e699          goto -1102
0x01E61328  app+0x061228  b0e80110      if (r0 != 1) {
0x01E6132C  app+0x06122C  4023          r0 = 3
0x01E6132E  app+0x06122E  4021          r0 = 1
0x01E61330  app+0x061230  e79c          goto -72
0x01E61332  app+0x061232  7404          [--sp] = {rets, r4}
0x01E61334  app+0x061234  0c48          r4 = b[r0+8] (u)
0x01E61336  app+0x061236  beeaa5f4      call -136886
0x01E6133A  app+0x06123A  4016          r0 = r4
0x01E6133C  app+0x06123C  3404          {rets, r4} = [sp++]
0x01E6133E  app+0x06123E  f794          goto -24
0x01E61340  app+0x061240  7404          [--sp] = {rets, r4}
0x01E61342  app+0x061242  50ee0449      r4 = b[r0+148] (u)
0x01E61346  app+0x061246  beeab8f4      call -136848
0x01E6134A  app+0x06124A  4016          r0 = r4
0x01E6134C  app+0x06124C  3404          {rets, r4} = [sp++]
0x01E6134E  app+0x06124E  f78c          goto -40
0x01E61350  app+0x061250  7404          [--sp] = {rets, r4}
0x01E61352  app+0x061252  0416          r4 = r0
0x01E61354  app+0x061254  4063          r0 = [r4+12]
0x01E61356  app+0x061256  0043          if (r0 == 0) goto 6
0x01E61358  app+0x061258  718c          call -40
0x01E6135A  app+0x06125A  43ea0040      [r4+12] = 0x0
0x01E6135E  app+0x06125E  4062          r0 = [r4+8]
0x01E61360  app+0x061260  0043          if (r0 == 0) goto 6
0x01E61362  app+0x061262  718e          call -36
0x01E61364  app+0x061264  42ea0040      [r4+8] = 0x0
0x01E61368  app+0x061268  4032          r0 = 18
0x01E6136A  app+0x06126A  3404          {rets, r4} = [sp++]
0x01E6136C  app+0x06126C  ffeae953      goto -88110
0x01E61370  app+0x061270  7404          [--sp] = {rets, r4}
0x01E61372  app+0x061272  e29c          sp += -16
0x01E61374  app+0x061274  0416          r4 = r0
0x01E61376  app+0x061276  1448          if (r4 == 0) goto 80
0x01E61378  app+0x061278  4067          r0 = [r4+28]
0x01E6137A  app+0x06127A  004d          if (r0 == 0) goto 26
0x01E6137C  app+0x06127C  8020          [sp] = r0
0x01E6137E  app+0x06127E  4022          r0 = 2
0x01E61380  app+0x061280  8021          [sp+4] = r0
0x01E61382  app+0x061282  4068          r0 = [r4+32]
0x01E61384  app+0x061284  d0e90900      d[sp+8] = r1_r0
0x01E61388  app+0x061288  60f04019      r1 = 0x300000  #
0x01E6138C  app+0x06128C  4066          r0 = [r4+24]
0x01E6138E  app+0x06128E  8b80          r3 = sp + 0
0x01E61390  app+0x061290  4224          r2 = 4
0x01E61392  app+0x061292  8dea830c      call 1710342
0x01E61396  app+0x061296  416b          r1 = [r4+44]
0x01E61398  app+0x061298  014a          if (r1 == 0) goto 20
0x01E6139A  app+0x06129A  c0ff88240100  r0 = 74888
0x01E613A0  app+0x0612A0  beeaf3d5      call -152602
0x01E613A4  app+0x0612A4  406b          r0 = [r4+44]
0x01E613A6  app+0x0612A6  beea5e7e      call -197444
0x01E613AA  app+0x0612AA  4bea0040      [r4+44] = 0x0
0x01E613AE  app+0x0612AE  50ec4000      r1_r0 = d[r4+0]
0x01E613B2  app+0x0612B2  8161          [r0+4] = r1
0x01E613B4  app+0x0612B4  9060          [r1+0] = r0
0x01E613B6  app+0x0612B6  c460          [r4+0] = r4
0x01E613B8  app+0x0612B8  c461          [r4+4] = r4
0x01E613BA  app+0x0612BA  4065          r0 = [r4+20]
0x01E613BC  app+0x0612BC  0042          if (r0 == 0) goto 4
0x01E613BE  app+0x0612BE  beea527e      call -197468
0x01E613C2  app+0x0612C2  4016          r0 = r4
0x01E613C4  app+0x0612C4  beea4f7e      call -197474
0x01E613C8  app+0x0612C8  0284          sp += 16
0x01E613CA  app+0x0612CA  5404          {pc, r4} = [sp++]
0x01E613CC  app+0x0612CC  7704          [--sp] = {rets, r7-r4}
0x01E613CE  app+0x0612CE  c6fffc160100  r6 = 71420
0x01E613D4  app+0x0612D4  6460          r4 = [r6+0]
0x01E613D6  app+0x0612D6  0457          if (r4 == 0) goto 46
0x01E613D8  app+0x0612D8  4d98          r5 = r4 + 24
0x01E613DA  app+0x0612DA  4120          r1 = 0
0x01E613DC  app+0x0612DC  5016          r0 = r5
0x01E613DE  app+0x0612DE  8deaf10b      call 1710050
0x01E613E2  app+0x0612E2  4016          r0 = r4
0x01E613E4  app+0x0612E4  5195          call -150
0x01E613E6  app+0x0612E6  4060          r0 = [r4+0]
0x01E613E8  app+0x0612E8  0484          goto 8
0x01E613EA  app+0x0612EA  0760          r7 = [r0+0]
0x01E613EC  app+0x0612EC  4121          r1 = 1
0x01E613EE  app+0x0612EE  6180          call -128
0x01E613F0  app+0x0612F0  7016          r0 = r7
0x01E613F2  app+0x0612F2  84e8fa01      if (r0 != r4) goto -12
0x01E613F6  app+0x0612F6  5016          r0 = r5
0x01E613F8  app+0x0612F8  8dea200c      call 1710144
0x01E613FC  app+0x0612FC  4016          r0 = r4
0x01E613FE  app+0x0612FE  beea          .byte 0xBE, 0xEA
