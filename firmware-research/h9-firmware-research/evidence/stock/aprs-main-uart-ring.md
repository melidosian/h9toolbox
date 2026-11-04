# Main APRS UART receive ring producer and consumer

Source: stock v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The Pi32v2 r3 decode covers app+`0x087218..0x08742C`; the virtual base
`0x01E00100` is inferred from JLFS entry metadata.

At app+`0x087298..0x0872B4`, the receiver loads two 16-bit ring positions:
the producer at RAM `0x16E34` and the consumer at RAM `0x16E36` (base
`0x159F0` plus `2594*2` and `2595*2`). It computes the modular difference
with a `2048`-byte
ring. If none are pending, it skips the frame parser. If more than `1024`
are pending, it copies only `1024` into a stack batch. Each copied byte uses
`(consumer + index) & 2047`; the consumer position is advanced by that count
before it processes the batch at app+`0x087340`.

The producer is the routine at app+`0x078EFE..0x078FD6`. Its `r1 == 3`
branch calls a serial object's read callback through the function pointer at
`[object+16]`. The call passes a global scratch buffer at RAM `0x159F0`, a
capacity of `1024`, and zero flags. The callback's low 16-bit return value is
the received byte count. The routine checks for a `PY`-prefixed control
message before placing ordinary bytes in the ring. A read of at least six
bytes with prefix `PY 02 01` is accepted as control when byte 5 equals the
8-bit sum of bytes 0 through 4. The independent Pi32v2 decode at
app+`0x078F44..0x078F5C` starts the sum with `0x50` (the `P` byte), then adds
bytes 1 through 4; a prior reading that omitted `P` was incorrect. Command
byte 4 equal to `0x55` sets a flag at
RAM `0x11B32` (app+`0x078FD8`); command `0x0D` calls the stock reply helper
at app+`0x07861E` with argument 1. That helper constructs a seven-byte
`THD 00 01 13 14` reply and sends it through a serial writer. The flag's
later use and the meaning of the reply remain untraced. Other valid `PY`
control messages bypass the APRS ring. For ordinary bytes, the routine
computes ring free space before copying. If
the batch fits, it copies each byte to RAM `0x16634 + producer_index`, wraps
the index with `& 2047`, stores the new producer position at `0x16E34`, and
sets the ready byte at `0x16E38`. When the batch does not fit, it takes an
overflow branch without a partial copy. The branch also writes a state flag,
whose later use remains untraced.

`replacement-source/main/h9_main_uart_ring.*` ports that batch drain and
passes completed frames to a callback. `h9_main_aprs_session_poll` now asks a
board-supplied read callback for at most `1024` bytes and places a complete
batch into the ring if it fits. Host tests cover wraparound, the `1024`-byte
poll limit, frame handoff, a full-ring drop, and an end-to-end split frame
through the read callback. Counting dropped bytes is a replacement policy;
the stock overflow flag's behavior is still unknown. The two observed `PY`
control effects are ported to `h9_main_aprs_session.*`; serial object
construction, peripheral setup, and board callback remain unmapped. The
registration routine starts at app+`0x088180`. It constructs a 32-byte serial
open record at stack+`0x24`, with pin identifiers `0x20,0x21` at offsets `0,1`,
RAM pointer `0x10CFC` at `+4`, sizes `2048` and `1024` at `+8,+12`, value `10`
at `+16`, producer callback `0x01E78FFE` at `+20`, zero at `+24`, and baud
`19200` at `+28`. It passes that record to app+`0x0C57BC` and stores the
returned serial object at RAM `0x11CFC`. The later producer reads from that
object's callback slot `+16`. Ghidra's seeded decompilation and the separate
Pi32v2 decode agree on the stack record; the semantic roles of the RAM
pointer, two sizes, and value `10` still need board verification.

The open routine at app+`0x0C57BC` compares the pin identifiers with a device
table, selects one of three UART register blocks (`0x001E2000`, `0x001E2100`,
`0x001E2200`), registers interrupt numbers `0x12`, `0x13`, or `0x14`, and
returns a serial object rooted at `0x1E258`, `0x1E32C`, or `0x1E400`. Its
two 11-byte pin tables reside at copied RAM `0xDA60` and `0xDA6C`. The pair
`0x20,0x21` occurs at index 5, which maps to UART channel 1 (base
`0x001E2100`, interrupt `0x13`) when that channel is available. The availability
helper at app+`0x0C5652` tests control-register bit 0 for each UART. The
fallback when the preferred channel is busy, interrupt callback behavior, and
read/write timing remain untested. `replacement-source/main/h9_main_serial_profile.*`
ports the 32-byte open record and pin-table route without touching hardware;
no board adapter is connected yet.

The selected UART1 serial object has function slots for byte write
(`0x01EC5E3A`), byte read (`0x01EC5E74`), bulk read (`0x01EC5EFA`), and bulk
write (`0x01EC5F94`). In its DMA read path, the bulk reader calls
`0x01EC5C4A` with the ring at RAM `0x1E350`. That helper treats four 32-bit
fields as storage address, power-of-two capacity, produced count, and consumed
count. It returns the minimum of requested and available bytes, copies in
one or two segments across the ring boundary, and increments consumed.
The independent Pi32v2 decode at app+`0x0C5B4A..0x0C5B82` confirms the
`consumed & (capacity - 1)` wrap; Ghidra's rendered expression for that mask
is misleading. `replacement-source/main/h9_main_serial_dma.*` ports the read
helper and caps a request to capacity to avoid an out-of-bounds copy if a
caller supplies an oversized request. A host test connects it to the APRS
session and decodes a framed version reply across a physical buffer wrap.

The UART1 bulk writer at `0x01EC5F94` first calls ROM operation `0x02002C80`
with the TX DMA descriptor at `0x1E3B0`, sets UART control bits `0x2000` and
`0x0004`, places source address and length in registers `0x001E2114` and
`0x001E2118`, calls ROM operation `0x02002B66` with that descriptor and zero,
then clears control bit `0x0004`. The independent decode at
app+`0x0C5E94..0x0C5EC6` confirms this order. The replacement
`h9_main_uart1_tx.*` ports the register sequence with mandatory callbacks
for the two ROM operations; it has no board implementation for them yet.
The DMA source must remain valid through TX event 1. It cannot directly use
a temporary reply buffer from `h9_main_aprs_session_poll`; a persistent
staging/queue layer is required. The replacement
`h9_main_uart1_tx_queue.*` now accepts caller-owned DMA-addressed slots,
copies outgoing frames into them, and advances after event 1. Its queue
depth and overflow counter are replacement policies rather than recovered
stock behavior; the board still needs to allocate DMA-capable slot storage.

The interrupt routine at
`0x01EC5FC8` dispatches event numbers 1, 2, and 3 through the object callback.
The independent decode at app+`0x0C5EC8..0x0C5F7A` resolves the conditions:
control bits `0x0004` and `0x8000` signal TX event 1; bits `0x0008` and
`0x4000` signal a full RX chunk and event 2; bits `0x0020` and `0x0800`
signal a partial RX chunk and event 3 when register `+0x28` is nonzero.
For RX events, stock sets control bits `0x1480`, adds the configured chunk
or partial count to the DMA ring producer at `0x1E358`, conditionally sets
status bit `0x2000`, calls a ROM DMA acknowledgement routine with `0x1E360`,
then calls the application event callback. TX event 1 sets control bit
`0x2000`, acknowledges `0x1E3B0`, and calls the callback. The replacement
`h9_main_uart1_irq.*` ports that ordering with injected acknowledgement and
event callbacks; fake-register tests cover all three events and zero-length
partial RX. The ROM acknowledgement behavior, DMA setup,
cache/synchronization rule, actual interrupt-vector binding, and completed TX path are
not yet connected to the replacement board entry.
