# APRS-controller UART receive buffers

Source: pinned `APRS-1.0.15.bin`, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Addresses assume image base `0x08002000`. Ghidra decompilation of
`0x08005F20`, `0x08005F98`, and `0x08004AC8` was checked against the
independent ARM disassembly and literal words in the image.

`0x08005F20` configures a UART descriptor with base `0x40013800` and baud
value `0x1C200` (115,200). Its receive state uses a current-buffer pointer
at RAM `0x20000120`, a 16-bit received length at `0x20000124`, a ready byte
at `0x20000126`, and a processing byte at `0x20000127`. The two 512-byte
buffers begin at `0x20000994` and `0x20000B94`.

The application loop at `0x08005DE8` calls `0x08005F98`. When ready and
length are nonzero, it marks processing, feeds each byte in the current
buffer to serial parser `0x08005B8C`, changes the current pointer to the
other buffer, clears processing/ready/length, then calls `0x08004AC8` to
arm another 512-byte receive. `0x08004AC8` clears that destination buffer
and starts the UART/DMA receive operation through `0x08004598`.

`replacement-source/aprs/h9_aprs_uart_receive.*` ports the buffer handoff,
parser feed, and rearm callback. It rejects invalid lengths and overlapping
completions.

Cross-references to the pending length and ready flag locate the producer at
`0x0800423C`, reached from the UART interrupt dispatcher at `0x08004094`.
On the USART1 branch, it stops the active DMA receive, computes
`512 - DMA_remaining`, and rearms immediately if zero bytes arrived or the
consumer is processing. Otherwise it sets the ready flag. The replacement
source now exposes `h9_aprs_uart_receive_idle()` for this count-to-length
handoff. It conservatively refuses a completion while an earlier batch is
pending or being processed; a board IRQ handler must supply the actual DMA
counter and rearm callback. The actual DMA registers, IRQ vector binding,
UART pin setup, and physical main-radio connection remain to be implemented
and tested.
