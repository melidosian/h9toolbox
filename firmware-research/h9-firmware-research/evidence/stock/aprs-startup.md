# APRS controller startup observations

The pinned `APRS-1.0.15.bin` is 17,304 bytes, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Addresses below use the inferred `0x08002000` image base. The image's initial
stack pointer is `0x20001A50`; reset vector is `0x080020D5`.

- `0x080020D4` calls `0x0800498C`, then branches to `0x080020C0`.
  `0x0800498C` reads the 32-bit register at `0x40021004`, preserves its
  upper 16 bits, sets bit 13, inserts the low 13 bits of the factory
  halfword at `0x1FFF0F04`, and writes the result back to `0x40021004`.
  It then writes `0x08002000` to SCB VTOR at `0xE000ED08`. The replacement
  ports these two writes before memory initialization; broader clock and
  peripheral setup remains unmapped.
- `0x080020C0` sets SP to `0x20001A50`, runs the two-entry initialization
  table via `0x0800219C`, then branches to `0x080021FD`.
- The table at `0x08006358` has records
  `(0x08006378, 0x20000000, 0x14C, 0x080021C0)` and
  `(0x08006398, 0x2000014C, 0x1904, 0x08005048)`. The first callback is a
  compact data expansion routine; the second writes zero 32-bit words. A
  replacement image may use normal uncompressed `.data` copying instead.
- `0x080021FC` copies 48 words from the image vector table at `0x08002000`
  to `0x20000000`, writes that RAM address to SCB VTOR at `0xE000ED08`, then
  calls the application's later initialization at `0x08005DE8`.

`replacement-source/aprs/h9_aprs_clock.*`, `h9_aprs_startup.c`, and
`link_check_startup.S` now implement the reset-register writes, ordinary
`.data` copy, `.bss` zeroing, 48-vector RAM relocation, and VTOR setup. The
linker reserves the first `0xC0` RAM bytes for those vectors. The linked
diagnostic image now maps DMA channel 1 and USART1 slots to replacement
receive handlers, while other interrupt slots retain default handlers. Its
board entry now selects the observed 24 MHz internal-clock path, configures
USART1 and DMA channel 1, and polls received UART batches. This source path
is untested on the board. ADC/DAC initialization, the rest of the timebase,
other interrupts, and the full APRS application are still absent.
