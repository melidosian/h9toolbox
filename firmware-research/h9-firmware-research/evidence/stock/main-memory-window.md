# Main one-time memory-window setup

Source: pinned stock main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The independent Pi32v2 decode of app+`0x05FB64..0x05FBF6` is in ignored
`output/main-heap-blocks-independent.asm`; Ghidra's listing is in ignored
`output/main-board-entry.txt`.

This code is inside the one-time branch guarded by RAM byte `0xD5F0`.
Immediately after the allocation-bitmap metadata, it writes `0x2C000` to
MMIO `0x100304` and `0x2D1FF` to `0x100308`, sets bit 4 of `0x100300`,
calls ROM `0x021127B4(0x2C000, 0, 0x1200)` to clear that RAM window,
clears bit 4, and sets bit 0. This is the observed register sequence; the
electrical purpose and safety of executing it alone have not been tested.

It then writes `0x2C000` at RAM `0x1D6F4`, `0x248000` at `0x1D6F8`,
`0x200000` at `0x1D740`, `0x20F000` at `0x1D744`, `0xF000` at
`0x1D748`, and `0x1E00000` at `0x1D74C`. It sets byte `0x1D6F0` to one,
stores halfword `0x1E0` at `0x1D750`, clears `0x79` bytes from
`0x1D754`, then sets 480 allocation bits starting at word `0x1D704`.
The final bitmap occupies 15 words.

`replacement-source/main/h9_main_memory_window.*` ports these writes with
injectable register and RAM pointers. It uses bounded C byte stores in
place of the stock ROM clear call and is invoked from the diagnostic board
entry only when the preceding `0xD5F0` guard was zero. Host tests verify
the RAM window, register end state, descriptor, clear boundary, and all
bitmap words. The Pi32 link check verifies the call order and key register
operations. Later stack guards, clock setup, interrupts, device drivers,
and application loop remain unported; this is not a bootable image.
