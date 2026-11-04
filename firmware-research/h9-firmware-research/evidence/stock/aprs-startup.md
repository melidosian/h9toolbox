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
  compact data expansion routine; the second writes zero 32-bit words. The
  compressed stream is 32 bytes at `0x08006378..0x08006397`, of which 31
  bytes are consumed and one trailing byte is zero. Its decoded 332-byte
  record initializes `0x200000C0` to 8,000,000, the tick counter at
  `0x200000C4` to zero, `0x200000C8` to 4, and the tick step at
  `0x200000CC` to **1 ms**. The clock setup later selects 24 MHz. Other
  nonzero initialized words are emitted by `decode_aprs_startup.py`.
  A replacement image may use normal uncompressed `.data` copying instead.
- `0x080021FC` copies 48 words from the image vector table at `0x08002000`
  to `0x20000000`, writes that RAM address to SCB VTOR at `0xE000ED08`, then
  calls the application's later initialization at `0x08005DE8`.

`replacement-source/aprs/h9_aprs_clock.*`, `h9_aprs_startup.c`, and
`link_check_startup.S` now implement the reset-register writes, ordinary
`.data` copy, `.bss` zeroing, 48-vector RAM relocation, and VTOR setup. The
linker reserves the first `0xC0` RAM bytes for those vectors. The linked
diagnostic image now maps SysTick, DMA channel 1, DMA channel 3, ADC1, USART1, and USART2 slots to
replacement handlers. The 19 reserved slots remain zero, as in stock. Stock
vector 2 (NMI) points to `0x08004906` and returns; vector 3 (HardFault)
points to `0x0800486C` and spins after a `nop`; vector 11 (SVC) and vector
14 (PendSV) point to one-instruction returns at `0x08004982` and
`0x08004980`. The replacement now supplies matching first-pass handlers.
Other populated peripheral slots retain a default spin handler. The pinned
image check verifies these vector values and handler bytes, and the link
check verifies the new vector topology. Its
board entry now selects the observed 24 MHz internal-clock path, configures
USART1 and DMA channel 1, polls received UART batches, and sends serial
replies through USART1. This source path
is untested on the board. ADC/DAC initialization, other interrupts, and the
full APRS application are still absent. A replacement SysTick handler is now
linked at vector 15, but its behavior has not been tested on hardware.
