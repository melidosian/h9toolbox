# Main APRS UART receive ring consumer

Source: stock v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The Pi32v2 r3 decode covers app+`0x087218..0x08742C`; the virtual base
`0x01E00100` is inferred from JLFS entry metadata.

At app+`0x087298..0x0872B4`, the receiver loads two 16-bit ring positions:
the producer at RAM `0x16E34` and the consumer at RAM `0x16E36` (base
`0x159F0` plus `2594*2` and `2595*2`). It computes the modular difference
with a `2048`-byte
ring. If none are pending, it skips the frame parser. If more than `1024`
are pending, it copies only `1024` into a stack batch. Each copied byte uses
`(consumer + index) & 2047`; the consumer position is advanced by that count
before it processes the batch at app+`0x087340`.

`replacement-source/main/h9_main_uart_ring.*` ports that batch drain and
passes completed frames to a callback. It also provides a single-producer
byte push for a future UART interrupt. Host tests cover wraparound, the
`1024`-byte poll limit, frame handoff, and a full-ring drop. Refusing a byte
when full and counting drops are replacement policies, not recovered stock
overrun behavior. UART/DMA register setup, interrupt wiring, and the board's
serial peripheral remain unmapped.
