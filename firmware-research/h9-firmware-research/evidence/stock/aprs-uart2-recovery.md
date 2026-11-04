# APRS controller USART2 recovery path

Source: pinned APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Addresses use the inferred `0x08002000` image base. The read-only Ghidra
report is generated locally from functions `0x080054C8` and `0x08002AAC`.

The stock main loop calls `0x080054C8`. That function handles a pending
USART2 batch through `0x0800555C`, then increments a loop counter. Each 1,000 loop iterations it
checks whether more than 2,000 SysTick units have passed since a saved time.
When USART2's descriptor status byte at `0x20000DDA` is `0x20` and another
state byte at `0x20000E01` is `1`, it invokes `0x08002AAC`. The latter retries
USART2 reception, then resets its DMA and USART state after repeated failures.
The loop count is not a clock, so the observed check cadence cannot be
converted to a precise period without a runtime trace.

The recovered `0x08002AAC..0x08002C7A` instructions show up to three
receive-DMA start attempts through `0x08004598`; a nonzero return increments
status word 4 at `0x20001634`, and zero increments status word 5 at
`0x20001638`. If all three attempts fail, the routine toggles RCC bit 17,
resets the USART/DMA state, and makes one final start attempt with the same
counter rule. The start helper returns zero on success, one for invalid
arguments, and two for a busy descriptor. Read-only Ghidra exports are in
ignored `output/stock/aprs-dma-restart-precise.txt` and
`output/stock/aprs-dma-restart-callees.txt`. The literal pool at
`0x08002C7C..0x08002C90` was checked against the pinned image.

The replacement currently has different USART2 DMA buffering and a bounded
raw transmit writer. Its writer latches `faulted` after a TXE/TC timeout.
The diagnostic board loop now checks recovery every 1,000 passes and only
when more than 2,000 SysTick units have elapsed since its last attempt. It
uses the replacement's latched raw-TX fault and idle receive-batch/spill
state in place of the two unmapped stock descriptor-status bytes. The DMA
stop rechecks those flags with interrupts masked. A restart
clears the TX fault only after USART2 register configuration succeeds and
rearms reception; timeout counts remain for diagnostics. This is a
replacement recovery policy, not a byte-for-byte port of `0x08002AAC`.
Its failed and successful recovery counts now populate status words 4 and 5;
precondition refusals with no attempted restart leave those counts unchanged.
The 9,600-baud peer and hardware behavior are still unverified.
