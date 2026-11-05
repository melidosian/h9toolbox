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
   limit. It also copies the record's field at `+0x10` into the UART1 object
   word at RAM `0x1E34C` before configuring baud. The stock APRS record
   supplies storage `0x10CFC`, capacity `2048`, read limit `1024`, and value
   `10` for that field.
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
radio. The independent decode at app+`0x0C5A44` loads the record's `+0x10`
word, and app+`0x0C5A90` writes it to UART1 object `+0x20` (`0x1E34C`).
The target adapter now makes this copy immediately before the baud helper;
for the stock APRS profile and a timebase of 1000, the helper writes `10` to
UART register `+0x10`.

The replacement `h9_main_uart1_board_open` ports the register and callback
order with mandatory injected pin, ROM DMA, IRQ, and baud hooks. It rejects an
occupied UART before any write, and requires the record's receive size and
threshold to match the existing link. Its baud hook can now use the separate
target binding. The UART1 open branch calls the pin helper at app+`0x0C5A0A`;
the helper at app+`0x078796` receives selector `1`.
Its first MMIO block is `0x001E5100`, where it sets bit 7, clears bit 6 at
`+0x24`, and sets mux field `01` at `+0x1C`. The selector-1 branch calls the
GPIO-bank lookup for pins `0x20` and `0x21`; the table at app+`0x000554`
resolves both to `0x001E5080`. For pin 32 it clears bit 0 at `+8` and sets
bit 0 at `+12`. For pin 33 it sets bit 1 at `+8` and `+16`, clears it at
`+20` and then `+8`, and sets it at `+12`. The independent Pi32v2 decode
covers app+`0x078796..0x07882A`.
`h9_main_uart1_pins.*` ports those writes with injectable MMIO and target
addresses. Neither its electrical effect nor simultaneous pin ownership is
hardware-verified. The UART1 ROM DMA initializer call sites at
app+`0x0C5A64` and `0x0C5A6C` reach app trampoline `0x078792`, whose first
instruction jumps to ROM `0x02002948`. The replacement DMA adapter exposes
that mapped entry with a guard for the two observed descriptor addresses.
`h9_main_uart1_board_target.*` now wires pin setup, ROM DMA initialization,
the existing IRQ registration adapter, and baud setup into the UART1 open
sequence. Its no-argument vector bridge dispatches to the APRS UART link.
Before any MMIO, it requires a nonzero clock state, matching link DMA hooks,
and matching UART pointers. On Pi32 it also checks that RX and TX DMA
addresses equal their backing pointers. The open hooks return success or
failure; if a later hook fails, earlier writes may remain. A host test opens
this adapter with fake MMIO and ROM calls and drives an RX interrupt through
the registered vector. The main reset/board entry does not call it yet, and
hardware behavior, fallback UART selection, and DMA cache coherency remain
unverified. The Pi32 diagnostic link remains unflashable.
