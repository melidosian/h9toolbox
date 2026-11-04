# Main UART1 APRS serial-open path

Source: stock v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Offsets below are relative to the captured app; virtual addresses add
`0x01E00100`. The existing seeded Ghidra export is in ignored
`output/main-serial-open-decomp-seeded.txt`; the baud routine below was also
checked with the independent Pi32v2 r3 instruction decoder.

At app+`0x0C5652`, the availability check returns free when control-register
bit 0 is clear. The APRS pin pair `0x20,0x21` selects UART1. In the UART1
branch of the open routine at app+`0x0C57BC`, stock:

1. Calls the pin configuration helper.
2. Sets UART1 control at `0x001E2100` to `0x3400`.
3. Calls ROM operation `0x02002948` for RX and TX DMA descriptors
   `0x001E360` and `0x001E3B0`.
4. Registers IRQ `0x13`, priority `3`, with the UART1 handler.
5. Clears ring produced/consumed at `0x001E358/0x001E35C`, sets UART
   `+0x20` to receive storage address plus capacity, ORs control with `0x68`,
   sets `+0x1C` to storage address, and sets `+0x24` to the record's read
   limit. The stock APRS record supplies storage `0x10CFC`, capacity `2048`,
   and read limit `1024`.
6. Sets control bit 1 from the baud field's low bit, then calls the baud
   helper at app+`0x0C56E4` with `(baud & 0x1FFFFFE) >> 1`. For the APRS
   profile, that argument is `9600` from baud field `19200`.

The independently decoded baud helper at app+`0x0C56E4` clears control bit
0, sets `0x3400`, and writes `+0x08` to
`(((48000000 + half_baud/2) / half_baud) / 4) - 1` when RAM `0xD9E4` is
nonzero. For the APRS record's 9,600 argument this is `1249`. If control bit
5 is set, it also writes `+0x10` using an activity word at `0x1E34C` and
timebase word at RAM `0xD9DC`: for activity below 11 it uses
`(activity * timebase) / 1000`, otherwise
`(activity / 10) * ((int32_t)timebase / 10)`. It finally ORs control with
`0x3401`. The semantic purpose and live values of the activity and timebase
fields remain unverified. The replacement `h9_main_uart1_baud_configure`
implements this register sequence with injected pointers and refuses a zero
clock state before touching UART registers; it has not been exercised on the
radio.

The replacement `h9_main_uart1_board_open` ports the register and callback
order with mandatory injected pin, ROM DMA, IRQ, and baud hooks. It rejects an
occupied UART before any write, and requires the record's receive size and
threshold to match the existing link. Its baud hook can now use the separate
target binding, but pin setup, ROM DMA, IRQ registration, and a real board
entry are still absent. It does not yet select a fallback UART, prove DMA
cache coherency, or bind the main reset entry to this board adapter. The Pi32
diagnostic link remains unflashable.
