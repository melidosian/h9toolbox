# TD-H9 v1.0.38 main RAM map (static analysis, 2026-11-08)

Derived from the stock app (SHA-256 `11a07bc0…`) startup code at app+`0x0`,
the heap setup at app+`0x05FA90`, and every 32-bit address load
(`cN ff imm32`) that points into each range. Nothing here has been measured
on a running radio yet.

| Range | Size | Use | Evidence |
|---|---|---|---|
| `0x00000–0x0DA77` | 55,928 B | RAM vectors and RAM-resident code | startup copies `0xDA78` bytes from app `0xC618C` (flash `0x01EC628C`) to address 0; app+`0x05FF52` fills the first 16 words with ones |
| `0x0DA78–0x0E28F` | ~2 KiB | startup stack | `sp = ssp = 0xE290` at app+`0x4` |
| `0x0E2A0–0x0E2DF` | 64 B | launch arguments | copied by the call at app+`0x0` |
| `0x0E2E0–0x1E4D3` | 66,036 B | `.data`/`.bss` | startup clears `0xE2E0` + `0x101F4` |
| `0x1E4D8–0x1E4FF` | 40 B | linker padding (resource service state) | no stock reference |
| `0x1E500–0x2BEFF` | 55,808 B | heap, part 1 | heap setup: `r5 = 0x2BF00` at app+`0x05FA9E`, start `(0x1E4D8 + 127) >> 7` |
| `0x2BF00–0x2BFFF` | 256 B | interrupt vector table (64 words) | app+`0x02466C` stores handler `n` (`n ≤ 63`) at `0x2BF00 + 4n`; app+`0x090516`/`0x0906B4` copy 24 words back |
| `0x2C000–0x2D1FF` | 4,608 B | most likely the main/interrupt stack | `r4 = 0x2C000` right after heap setup (app+`0x05FB64`); no other reference |
| `0x2D200–0x2D27F` | 128 B | second RAM section (empty `.bss`) | startup clears `0x2D200` + 0 |
| `0x2D280–0x2FF7F` | 11,520 B | heap, part 2 | heap setup: `0x2D27F` rounded up, end `0x2FF80` |
| `0x2FF80–0x2FFFF` | 128 B | retained startup record | app+`0x05E496`, `0x04F55A` |

## Heap

- 128-byte blocks, bitmap at `0x1D7D0` (`0xDC` bytes: 48 level-0 words, a
  level-1 summary from word 48, then bounds and counters).
- Halfword `0x1D7D0 + 216` counts **used** blocks and `+218` holds the
  **total** (`0x20F` = 527 blocks = 67,456 B), so free heap is
  `(h[0x1D8A8] − h[0x1D8A6]) × 128` (halfwords at `0x1D7D0 + 218` and `+ 216`).
- The block allocator is RAM code at `0x166A` (rounds up to blocks, checks
  the free count, claims bitmap bits). It has no direct call or address-load
  callers, so it is reached through a table or the ROM; its calling
  convention is not yet confirmed. `0x1634`/`0x1650` are the interrupt
  disable/enable pair it uses (122 and 111 callers).
- The heap total is derived from the part-1 end in `r5`
  (`r5 & ~0x7F` feeds the size fields), so changing the one immediate at
  app+`0x05FA9E` (bytes `c5 ff 00 bf 02 00`) shrinks the heap consistently.

## Free RAM conclusion

There is no large fixed RAM region that stock leaves unused; every range
above has an owner. Free RAM at run time is the heap's free blocks. RAM is
executable (stock runs 55,928 bytes of code from `0x0`).
