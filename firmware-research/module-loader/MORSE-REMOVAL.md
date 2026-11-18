# Morse/CW disable prototype and removal investigation

The requested combined APRS build should drop stock Morse/CW. The opt-in
`--disable-morse` prototype now disables the two identified terminal-entry
paths, keying branch and CW transmit-message action. It does not hide stock menu labels,
remove the complete subsystem, or reclaim code/data. Hardware validation is
still required.

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
  `0x16277` before dispatch. A future menu change must preserve those IDs.
  The CW submenu includes terminal, key, speed, tone, power, threshold and message.

Before removal: trace the persisted enable load and all CW dispatch/keying
paths, disable them consistently even with an existing enabled setting,
then remove/skip menu entries in every locale. Only reclaim code/data after
incoming references are demonstrated exclusive. Shared speaker, keypad,
beep and ordinary RF routines must remain. Bench-test boot/menu/voice/recovery.

The prototype is partial: feature entry is disabled, not full menu/code removal.

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

These are useful candidate disable boundaries, not a complete incoming-reference
proof. Do not simply remove allocator/cleanup or reuse the code range: the
menu, drawing, keying and shared-radio callers must be considered together.
No EEPROM write is introduced. The entry-disable prototype below uses these
existing guards rather than changing persisted user settings.

## Opt-in offline prototype

`morse_disable.py` requires the exact pinned stock SHA and verifies all patch
sites before changing an image. At +0x6F96E/+0x6FB30 it replaces the six-byte
conditional enable tests with a short unconditional branch plus two NOPs to
the same stock Off path, +0x6F5CC. It changes the +0x76990 dispatch halfword
from 0x0092 to 0x0316, landing at the existing matching-frame epilogue +0x76FAE.
The keying check at +0x876B2 now branches to its existing inactive path at
+0x87E38. All four sites require exact stock signatures before applying.
No RF/audio helper, context allocator/cleanup or saved setting is altered.

Independent review confirmed both decoded branch destinations, matching
stack restore, two direct allocator callers behind these gates, and no direct
transfers into the patched instruction interiors. This static scan cannot
exclude computed indirect entry points. Tests verify exact changed-byte
bounds, target decoding, wrong-base refusal and conflict refusal. Combined
build/package changed-sector verification passes with `--aprs-mute --disable-morse`.

Before release, bench-check CW settings already ON/OFF, both terminal/key
entry modes, message action, exit/back behavior, ordinary voice/keypad/beeps,
PTT and recovery. Menu labels remain present and stock firmware restored
later will still see the user's old CW setting. No code space is reclaimed.
