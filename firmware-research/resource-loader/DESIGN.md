# Resource-flash code loader (design, not built)

Goal: run code stored in the ~2.9 MiB of free space in the 16 MiB resource
flash (`resource` partition, `0xA1F000–0xCFFFFF`), so new features are no
longer limited by the few hundred bytes of free space left in the main app.

The CPU cannot execute from the resource chip (a separate SPI NOR with no
memory mapping), so modules are **copied into RAM and run there**. Stock
already runs 55,928 bytes of code from RAM at `0x0`.

See [the RAM map](../h9-firmware-research/evidence/stock/main-ram-map.md).

## 1. Where modules run: a reserved slice of the heap

No large fixed RAM region is unused, so take one from the heap at boot:
change the heap part-1 end at app+`0x05FA9E` from `0x2BF00` to
`0x2BF00 − N`. The stock heap setup derives its totals from that value, so
the heap simply becomes `N` bytes smaller and never hands out
`[0x2BF00 − N, 0x2BF00)`.

- Proposed `N` = 8 KiB (`0x29F00–0x2BEFF`, 64 of 527 blocks). The right
  size depends on measured peak heap use (phase 0).
- A fixed load address means modules are linked for it: no relocation, no
  allocator calling convention to reverse-engineer.
- Costs one more main-flash sector (`0x64000`) in the firmware build.

Alternative kept in reserve: call the stock block allocator (`0x166A`) at
run time, with modules linked position-independently or relocated.

## 2. Module format (stored in `resource`)

A directory sector at `0xA20000` (first free sector after the stock images),
then module bodies, each 4 KiB-aligned:

```
directory entry (32 bytes)        module header (32 bytes, at body start)
  magic   "H9MD"                    magic     "H9MB"
  id      u16                       format    u16 (1)
  flags   u16                       load_addr u32 (0x29F00)
  offset  u32 (chip address)        size      u32 (code+data, ≤ N)
  length  u32                       entry     u32 (offset from load_addr)
  crc16   u16                       bss       u32 (bytes to zero after load)
  name    char[14]                  stock     u32 (first word of the stock app SHA-256)
                                    crc16     u16 (over the body)
```

The loader refuses a module whose `stock` tag does not match the running
firmware, whose size exceeds the slot, or whose CRC fails.

## 3. Loader stub (in main flash, ~120–160 bytes)

1. Check the flash-ready marker (`0x12234 == 0x5AA5`).
2. Read the header with the stock NOR read `0x01E4FF22(buf, addr, len)`;
   validate magic, stock tag and size.
3. Read the body straight into `load_addr`, zero `bss`, recompute the CRC
   (the CRC-16 code already in the write service).
4. `csync`, then `call load_addr + entry` with a small API table pointer in
   `r0` (NOR read, the resource service's status, interrupt
   disable/enable, the panel draw routine).
5. Report the result in the resource-service status bytes.

Triggers:
- **Development:** a new write-service op (`W 0x4000`, op 3 "load and run
  module `id`"), so a module can be uploaded with the existing sector writer
  and run from the browser without reflashing.
- **Features:** the Brick Breaker launcher hook (hold Back) or a boot hook.

## 4. Build tooling

- Modules are assembled/linked with the pinned Pi32v2 toolchain at
  `load_addr`; a packer adds the header and CRC and emits a `.h9mod`.
- The page writes `.h9mod` files into `resource` with the existing guarded
  sector writer (same CRC-checked erase, per-block verification, read-back).

## 5. Phases

0. **Measure** (research image, not the merged firmware): S2 plus a
   RAM bulk-read slot, using the 696 bytes still free in S2's first cave.
   Dump RAM over Bluetooth in several states (idle, after programming,
   menus, receive/transmit, GNSS) and record the heap used-block counter.
   Confirms the map above and sizes `N`.
1. **Reserve:** patch the heap end; confirm the radio is stable (Bluetooth,
   programming, menus, long receive) with the smaller heap.
2. **Loader:** the stub and op 3; first module returns a constant and
   blinks the LED, proving RAM execution at `0x29F00`.
3. **First real payload:** move Brick Breaker (1,628 bytes) into a module
   loaded by the Back-hold launcher. That frees most of the first cave in
   H9 Toolbox Firmware for the loader and future hooks, and from then on
   new features live in the resource flash.

## Open questions

- Peak heap use (phase 0). If it leaves less than ~8 KiB, use a smaller slot
  or the allocator route.
- Whether all of SRAM is executable (expected; stock runs code from `0x0`).
- What, if anything, uses `0x2C000–0x2D1FF` besides the stack.
