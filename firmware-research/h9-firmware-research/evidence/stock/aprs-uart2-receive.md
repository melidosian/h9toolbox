# APRS controller USART2 receive and forwarding

Source: APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Addresses assume image base `0x08002000`. These are static disassembly and
literal-value observations, not a board traffic capture.

The stock vector table puts the USART2 interrupt in slot 44, pointing to
`0x08004FB1`. That handler uses the descriptor pointer at `0x20000D94`,
tests USART IDLE status/enable bit `0x10`, clears the condition by reading
status and data, then calls the generic receive handoff at `0x0800423C`.
The descriptor's peripheral base is `0x40004400`, set at `0x08002974`.
The same setup selects 9,600 baud. The neighboring slot 26 points to
`0x080028B1`, which calls the generic DMA handler for the descriptor at
`0x20000DE0`; its channel-register literal is `0x40020030`.
At `0x08002ABE`, the stock setup calls `0x0800342C` with that descriptor and
request 8. The latter enables bit 0 at `0x40021040` and writes request 8 to
bits 16..20 at `0x4001001C`, preserving other request fields. The DMA base
literal at `0x0800347C` is `0x40020008`, so channel index 2 selects that
bit field. This identifies the USART2 receive DMA request mapping.

On the USART2 branch of `0x0800423C`, the handoff computes
`1024 - DMA_remaining`, stores a receive count and tick in the status
record at `0x20001624`, and publishes data for the application if the
processing flag allows it. It toggles the current buffer pointer at
`0x20000128` between `0x20000E24` and `0x20001224`, both 1,024-byte
areas. The startup record initializes that pointer to `0x20000E24`.
The application path at `0x0800555C` uses the pending pointer and 16-bit
length at `0x2000012C`/`0x20000130`, caps the forwarded length at
`0x3FC` (1,020), then calls `0x080048F4`. That wrapper calls the serial
framer with command `0x10`. It clears the pending pointer/length/flags
afterwards. The same `0x20001624` status record contributes to command
`0x13` replies; its fields' full meanings remain untraced.

The replacement's `h9_aprs_uart2_dma.*` binds the observed channel register,
request mapping, USART2, vector slots 26 and 44, and the 1,024-byte receive
buffer. It forwards at most 1,020 bytes as serial command `0x10`, as stock
does. A completed chunk pauses receive while the main loop forwards it,
then rearms. This first pass can lose bytes during that pause and does not
reproduce stock double buffering or every UART error branch. The retained
`h9_aprs_uart2_rx.*` RXNE path is a host-tested fallback, not the diagnostic
entry's active receive path. Host tests cover short and full DMA chunks,
transfer-error rearm, and request mapping. The ARM diagnostic link checks
both interrupt slots and reports static RAM headroom; no radio-side UART
test has run.
