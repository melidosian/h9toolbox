# APRS board initializer call graph

Source: pinned APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
The addresses below were inspected in the read-only Cortex-M0 Ghidra project;
the instruction listing was checked alongside the pseudocode.

At `0x08005DE8`, stock starts its tick helper, selects the internal clock,
calls board initializer `0x080056EC`, prints the boot banner, then loops over
USART1 receive batches, decoded APRS bytes, and USART2 maintenance.

The board initializer calls `0x08002604` for the demodulator, `0x080051AC`
for the receive-frame object, `0x08005F20` for USART1 receive setup,
`0x08005800` for serial framing state, `0x080048E8` to store the USART1
descriptor pointer, and `0x08002974` for USART2 setup. It then configures
PA0 as an output and drives it low. The replacement diagnostic entry has
separate source for these broad operations, though it does not reproduce
every stock descriptor field or establish the board's external signals.

`0x08005800` stores object pointers at RAM `0x200000FC`, `0x20000100`, and
`0x20000104`, clears `0x2000010E`, and calls `0x08005B54`. That helper clears
RAM bytes `0x2000010C` and `0x2000010D`, zeros word `0x20000108`, stores
`0xFE` at `0x2000010F`, and zeros 792 bytes at `0x200005EC`. The location
and size suggest serial frame state, but its precise field semantics are
not proven. The replacement decoder uses explicit `has_command` and length
fields instead of this sentinel layout.

`0x08005F20` prepares the USART1 receive descriptor and DMA path; stock
uses 115,200 baud. `0x08002974` prepares the separate USART2 descriptor at
9,600 baud. The replacement UART board and DMA modules represent these
interfaces, while physical traffic and initialization order remain untested.
