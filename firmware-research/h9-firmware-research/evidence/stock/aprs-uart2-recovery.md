# APRS controller USART2 recovery path

Source: pinned APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Addresses use the inferred `0x08002000` image base. The read-only Ghidra
report is generated locally from functions `0x080054C8` and `0x08002AAC`.

The stock main loop calls `0x080054C8`. That function handles a pending
USART1 batch, then increments a loop counter. Each 1,000 loop iterations it
checks whether more than 2,000 SysTick units have passed since a saved time.
When USART2's descriptor status byte at `0x20000DDA` is `0x20` and another
state byte at `0x20000E01` is `1`, it invokes `0x08002AAC`. The latter retries
USART2 reception, then resets its DMA and USART state after repeated failures.
The loop count is not a clock, so the observed check cadence cannot be
converted to a precise period without a runtime trace.

The replacement currently has different USART2 DMA buffering and a bounded
raw transmit writer. Its writer latches `faulted` after a TXE/TC timeout.
The diagnostic board loop now waits two seconds after observing that fault,
then attempts a USART2/DMA restart when no receive batch or spill ring is
pending. The DMA stop rechecks those flags with interrupts masked. A restart
clears the TX fault only after USART2 register configuration succeeds and
rearms reception; timeout counts remain for diagnostics. This is a
replacement recovery policy, not a byte-for-byte port of `0x08002AAC`.
The 9,600-baud peer and hardware behavior are still unverified.
