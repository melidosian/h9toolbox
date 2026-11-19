# Stock Morse/CW removal

`--disable-morse` removes the Morse category and 3,656 bytes of code exclusive
to the stock CW allocator, terminal entry, message transmit and key handling
paths. Shared RF, audio and settings routines keep their stock layout.
The November 18 bench check confirmed that Morse is absent and stock APRS
still opens on the first build. The additional cleanup of the decoder
tails and private helpers has been checked offline.

Pinned stock 1.0.38 findings:

- Shared menu-action handler starts at app+0x7693E. Its TBH table base is
  app+0x76982. Index 7 at +0x76990 is little-endian `92 00`, selecting
  +0x76AA6 (base + 2*0x92). This branch checks submenu 7 and displays
  `TX CW` at +0x76ADA. The enclosing routine handles unrelated menus.
- Default epilogue +0x76FAE restores the 12-byte frame and r15-r4/PC.
  Table entry `16 03` would route index 7 there. This is only a candidate
  no-op for one menu action, not global feature removal.
- CW key/terminal logic around +0x87684 also checks submenu 1, app_core+408,
  and +420 bit 0, managing timing/state at +2365/+2366/+2377. Rendering
  helpers +0x849B6/+0x84AB4/+0x84CEA use the +420 state too, which is cleared
  at +0x74DFE. This is not a proven persistent enable flag.
- Settings tooling describes image offset0x302A bit0 as Morse enable. The
  follow-up trace below identifies RAM0xB860 and the image-header translation.
- English menu label table +0xCC9AC has Morse pointer +0xCC9C8. The stock
  startup copy maps it to RAM `0x683C`. The category has 11 labels per locale
  in the nine descriptors at app+`0xB3350–0xB3390` (stride 8). The Morse
  label is index 7, followed by GNSS at 8 and APRS at 10. Menu action code
  at app+`0x7694C–0x76966` reads the selected entry through the ID map at
  `0x16277` before dispatch.
  The CW submenu includes terminal, key, speed, tone, power, threshold and message.

## Root menu

The top-level builder at app+`0x6D34A` calls `0x6811E` with selector 11.
The jump table at `0x68136` routes selector 11 to `0x68296`, which fills
the category list at RAM `0x12C77`. The list includes a count at `0x12C74`.
The menu's stack helper at `0x68044` copies the 106-byte list when it opens
a submenu. The selection code reads the resulting category ID for dispatch.

The patch at `0x682B6` writes ten entries and skips ID 7. GNSS, Site Type,
and APRS retain IDs 8, 9, and 10. The locale label descriptors are shared
across users of those IDs and keep their stock indices. The replacement
maintains the original instruction span, return frame and `r0` on exit.
The other audited entry paths provide separate guards for saved enabled
settings and stale menu state.

## Follow-up static trace

The runtime settings word is now identified at RAM `0xB860` (47200):

- app+`0x7289E` selects logical settings address `0x3022`, and +`0x728A4`
  calls the stock settings reader. The non-FF result is stored to `0xB860`
  at +`0x728B6`. The next read at +`0x728C0` selects `0x3023` and stores
  the high byte. The eight-byte difference from CHIRP/browser file offsets
  `0x302A/B` is explained by the programmer image identity header: the
  hardware-verified 2026-11-09 resource-flash log maps file bytes after its
  eight-byte header directly to rw_freq at resource0x900000. The stock reader
  +0x52BB2 adds the selector4 partition base to the logical offset. This
  resolves the address mismatch; it does not authorize writing stock settings.
- app+`0x6EFA4` decodes this word into CW speed, tone, threshold and power
  runtime state. The speed function at +`0x6EF5A` stores app_core+2364/2365.
- Terminal-entry paths +`0x6F96E` and +`0x6FB30` check bit 0 of this word
  before calling the CW-context allocator at +`0x6F000`. The allocator
  creates a 244-byte object at app_core+408 and initializes the message
  pointer at +412. Both callers explicitly check allocation failure.
- The menu setter +`0x72D1C` changes the low enable bit for selector 0 and
  other CW fields for other selectors. +`0x737A2` also updates enable and
  calls cleanup +`0x72CC2` on off. Cleanup frees context and clears +408,
  +412, +416 and keying state after shared RF/audio cleanup.

The settings decoder and shared cleanup remain in place. The patch uses stock
disabled paths and leaves the saved fields available to the factory firmware.

## Code removal

`morse_disable.py` requires the exact pinned stock SHA and verifies all patch
sites before changing an image. At +0x6F96E/+0x6FB30 it replaces the six-byte
conditional enable tests with a short unconditional branch plus two NOPs to
the same stock Off path, +0x6F5CC. It changes the +0x76990 dispatch halfword
from 0x0092 to 0x0316, landing at the existing matching-frame epilogue +0x76FAE.
The earlier keying branch at +0x8769A now always takes its stock inactive path
at +0x87E38, before loading the CW context or reaching the old test at +0x876B2.

| Removed range, end exclusive | Entry and boundaries |
| --- | --- |
| 0x6F000–0x6F03C | Only direct calls at +0x6F97C and +0x6FB3E, inside the retired entry prefixes. +0x6F03C starts a shared routine. |
| 0x6F974–0x6F9A6, 0x6FB36–0x6FB4C | Fallthrough behind the two disabled terminal gates. The following shared instructions remain. |
| 0x76AA6–0x76B52, 0x76BCE–0x76CDC | CW transmit setup and character loop. Index 7 was the only jump-table entry into the setup. Indices 8 and 10 still point to +0x76B52 and +0x76B56. Branches into the transmit loop originate in the removed code. |
| 0x84A82–0x84AB4 | Dot/dash lookup used by the Morse decoder. The display routine starting at +0x84AB4 remains. |
| 0x84E98–0x85666 | Tone and text helpers, the dedicated key handler entered by +0x876B8, and the character decoder entered by +0x87CE8 and +0x87E9E. The next complete function starts at +0x85666. |
| 0x876A0–0x87710 | Fallthrough below +0x8769A, including the call to +0x84F88. Other entries to the shared key routine bypass this block. |
| 0x87B8C–0x87E38, 0x87E76–0x87F64 | CW threshold, tone handling and display updates reached from the removed key prefix. The intervening stock packet handling and register restore at +0x87E38–0x87E76 remain. |

The static scan checked aligned direct transfers, instruction boundaries,
embedded literal pointers and the local switch tables. Apparent incoming
transfers from +0x70A68, +0x76B3A, +0x84930, +0x85710, +0x86398, +0x867A4,
+0x867C6, +0x87E70, +0x8810A and +0x88522 decode inside longer instructions.
The apparent odd pointers
at +0xA9BD and +0x42BEF span adjacent instructions. Each removed range has
a pinned SHA-256. Regression checks verify the saved branch targets, range
bounds and rejection of modifications to the expected input. The resident
build reuses the first 248 bytes at +0x84F88 for the relocated dev KISS helpers.

Before release, bench-check startup with the stock CW preference enabled
and disabled. Exercise menu scrolling, GNSS and APRS entry, ordinary voice,
keypad tones, PTT and recovery.
