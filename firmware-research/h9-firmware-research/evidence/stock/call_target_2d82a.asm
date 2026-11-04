; Linear decode of selected v1.0.38 app.bin bytes. Data can appear as instructions.
; The 0x1E00100 address mapping is inferred from JLFS entry-point metadata.
; app.bin SHA-256 11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3
; decoded instructions: 30; undecoded chunks: 12
0x01E2D92A  app+0x02D82A  f642          if (r6 != 0) goto -60
0x01E2D92C  app+0x02D82C  5604          {pc, r6-r4} = [sp++]
0x01E2D92E  app+0x02D82E  50ec0420      r3_r2 = d[r0+4]
0x01E2D932  app+0x02D832  c1ffa0e20000  r1 = 58016
0x01E2D938  app+0x02D838  50ec1121      d[r1+16] = r3_r2
0x01E2D93C  app+0x02D83C  0260          r2 = [r0+0]
0x01E2D93E  app+0x02D83E  2362          r3 = [r2+8]
0x01E2D940  app+0x02D840  9366          [r1+24] = r3
0x01E2D942  app+0x02D842  2a4d          r2 = b[r2+13] (u)
0x01E2D944  app+0x02D844  9a40          b[r1+0] = r2
0x01E2D946  app+0x02D846  0a66          r2 = h[r0+12] (u)
0x01E2D948  app+0x02D848  9a6e          h[r1+28] = r2
0x01E2D94A  app+0x02D84A  0a67          r2 = h[r0+14] (u)
0x01E2D94C  app+0x02D84C  9a6f          h[r1+30] = r2
0x01E2D94E  app+0x02D84E  50ec0021      r3_r2 = d[r0+16]
0x01E2D952  app+0x02D852  50ec1122      d[r1+32] = r3_r2
0x01E2D956  app+0x02D856  8000          rts
0x01E2D958  app+0x02D858  3739          r7 |= 33554432
0x01E2D95A  app+0x02D85A  e901          .byte 0xE9, 0x01
0x01E2D95C  app+0x02D85C  22cc          .byte 0x22, 0xCC
0x01E2D95E  app+0x02D85E  e401          .byte 0xE4, 0x01
0x01E2D960  app+0x02D860  0000          nop
0x01E2D962  app+0x02D862  0000          nop
0x01E2D964  app+0x02D864  0000          nop
0x01E2D966  app+0x02D866  0000          nop
0x01E2D968  app+0x02D868  0000          nop
0x01E2D96A  app+0x02D86A  0000          nop
0x01E2D96C  app+0x02D86C  0000          nop
0x01E2D96E  app+0x02D86E  0000          nop
0x01E2D970  app+0x02D870  a8cd          .byte 0xA8, 0xCD
0x01E2D972  app+0x02D872  e401          .byte 0xE4, 0x01
0x01E2D974  app+0x02D874  dcd0          .byte 0xDC, 0xD0
0x01E2D976  app+0x02D876  e401          .byte 0xE4, 0x01
0x01E2D978  app+0x02D878  0000          nop
0x01E2D97A  app+0x02D87A  0000          nop
0x01E2D97C  app+0x02D87C  48d3          .byte 0x48, 0xD3
0x01E2D97E  app+0x02D87E  e401          .byte 0xE4, 0x01
0x01E2D980  app+0x02D880  70d3          .byte 0x70, 0xD3
0x01E2D982  app+0x02D882  e401          .byte 0xE4, 0x01
0x01E2D984  app+0x02D884  0000          nop
0x01E2D986  app+0x02D886  0000          nop
0x01E2D988  app+0x02D888  b6d3          .byte 0xB6, 0xD3
