# APRS controller status reply

Source: APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
The Thumb routine at `0x0800586C..0x08005A82` constructs 48 bytes on its
stack and sends serial command `0x13` through `0x08004894`. All addresses in
this note refer to the controller image, whose inferred flash base is
`0x08002000`.

Bytes 0 through 39 copy ten 32-bit little-endian words from RAM
`0x20001624..0x2000164B`. The routine first calls `0x08003890` for a tick
count ([tick trace](aprs-systick.md)), then subtracts the word at
`0x20001644` from it and narrows to 16
bits. Byte 40 copies RAM `0x2000164C`. Bytes 41 and 42 both copy the low
byte of the halfword at RAM `0x20000130`; byte 43 copies its high byte.
Bytes 44 and 45 copy RAM `0x20000132` and `0x20000133`. Bytes 46 and 47 are
the low and high bytes of the narrowed tick difference. The repeated low
byte at 41 and 42 is present in the stock instructions; it is not a presumed
16-bit field layout.

`replacement-source/aprs/h9_aprs_status.*` implements this byte layout from
an explicit snapshot. The host test sends the encoded record through command
`0x13` and parses the resulting frame. The stock startup data initializes the
tick step to 1 ms; see [the startup trace](aprs-startup.md).

Read-only Ghidra references to the status block show the following writer
families. The full generated reference report is in ignored
`output/stock/aprs-status-ram-refs.txt` and the selected decompilations are in
`output/stock/aprs-status-writers.txt`:

- Word 0 (`0x20001624`) increments in the USART1 receive callback at
  `0x0800423C`.
- Word 1 (`+4`) increments when `0x080054C8` handles a receive frame-ready
  flag. Word 2 (`+8`) increments in `0x0800555C` after a packet is forwarded.
- Word 3 (`+0x0C`) increments on a receive buffer conflict in USART1 or
  USART2 callbacks. Words 4 and 5 (`+0x10`, `+0x14`) count branches in the
  serial DMA restart routine `0x08002AAC`.
- Word 6 (`+0x18`) increments when a receive chunk is staged. Word 7
  (`+0x1C`) holds the recent chunk length; word 8 (`+0x20`) holds its tick.
  Word 9 (`+0x24`) has both USART1 error and periodic recovery writers.
- Byte 40 (`+0x28`) tracks a receive buffer side. The halfword at
  `0x20000130` and bytes `0x20000132/133` are written from serial receive
  state; their full lifetimes remain to be traced.

The diagnostic replacement entry now supplies a live reply with the same
48-byte envelope. It counts completed/rejected UART1 batches and valid/forwarded
APRS frames, tracks the last UART1 length and tick, and reports observed
USART2 batches and errors. Words 4 and 5 remain zero because the stock retry
driver is unported. Some other counters are **replacement diagnostics**, not
exact reproductions of the stock branches: for example, replacement word 1
counts CRC-valid frames while stock increments on frame-ready, and word 6
counts forwarded USART2 batches while stock increments when chunks are staged.
The reply may see different interrupt counters from different instants; it is
diagnostic data, not a transactionally consistent hardware snapshot. No radio
has run this entry, and the linked image remains unflashable.
