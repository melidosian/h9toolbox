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
pending or being processed.

The stock vector table has DMA channel 1 in slot 25 (IRQ 9, handler
`0x080028A0`) and USART1 in slot 43 (IRQ 27, handler `0x08004F64`).
`0x08004A50` configures DMA channel registers at `0x40020008` with initial
CCR `0x3080` and associates them with USART1 at `0x40013800`. Its receive
start path `0x08004598`/`0x08003620` programs CNDTR, CPAR, and CMAR, then
enables CCR. The USART1 handler reads status and data to clear IDLE and
calls `0x0800423C`, which reads CNDTR from channel offset `+4`. The
replacement `h9_aprs_uart1_dma.*` now binds these register blocks, arms
DMA, handles the IDLE length path, and binds both IRQ vector slots in the
diagnostic link. On full DMA completion it publishes 512 bytes directly;
this is a replacement-side recovery behavior, not established stock behavior.

USART clock/baud configuration, GPIO pin mapping, NVIC enablement, and the
physical main-radio connection remain to be implemented and tested. Fake
register tests and a diagnostic link establish compilation and register
selection only; they do not establish working reception on the radio.
