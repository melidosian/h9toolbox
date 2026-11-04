# Main APRS UART receive ring producer and consumer

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

The producer is the routine at app+`0x078EFE..0x078FD6`. Its `r1 == 3`
branch calls a serial object's read callback through the function pointer at
`[object+16]`. The call passes a global scratch buffer at RAM `0x159F0`, a
capacity of `1024`, and zero flags. The callback's low 16-bit return value is
the received byte count. The routine checks for a `PY`-prefixed control
message before placing ordinary bytes in the ring; that control path has not
yet been fully reconstructed. It computes ring free space before copying. If
the batch fits, it copies each byte to RAM `0x16634 + producer_index`, wraps
the index with `& 2047`, stores the new producer position at `0x16E34`, and
sets the ready byte at `0x16E38`. When the batch does not fit, it takes an
overflow branch without a partial copy. The branch also writes a state flag,
whose later use remains untraced.

`replacement-source/main/h9_main_uart_ring.*` ports that batch drain and
passes completed frames to a callback. `h9_main_aprs_session_poll` now asks a
board-supplied read callback for at most `1024` bytes and places a complete
batch into the ring if it fits. Host tests cover wraparound, the `1024`-byte
poll limit, frame handoff, a full-ring drop, and an end-to-end split frame
through the read callback. Counting dropped bytes is a replacement policy;
the stock overflow flag's behavior is still unknown. The `PY` control
intercept, serial object construction, peripheral setup, and board callback
remain unmapped. The stock producer's `r1 == 3` caller has not been traced to
a specific UART instance.
