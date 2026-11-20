# Unified radio and Toolbox menu experiment

Branch: `codex/radio-menu-replacement`, based on `origin/dev` at `67e446b`.
The target is one sectioned on-radio menu for radio and H9 Toolbox settings,
entered with the normal Menu key. The stock menu UI must be removed only after
every stock setting and action has an equivalent path. Keep the stock setting
storage and radio control routines until their replacements are verified.

## Findings from the pinned factory image

`inventory.py` verifies the factory v1.0.38 app SHA-256 before reporting any
addresses. The stock Menu callback pointer is at app+`0xB6BC4` and targets
CPU `0x01E6D426` (app+`0x6D326`). The target sets up the native root list;
the root builder at app+`0x6811E` fills category IDs at RAM `0x12C77`.
The ten active categories in the dev image are Radio Setting, Program Chann,
Analog Set, Radio Info, Scan, Bluetooth, SMS, GNSS, Site Type and APRS. Morse
is category 7 in the factory image and is disabled in the dev build.

The current grouped Toolbox MENU module is 6,246 bytes. Its safe ceiling is
6,528 bytes because shared satellite state begins at RAM `0x2B880`; only 282
bytes remain. A complete radio settings editor must live outside that slot.

## Flash placement trial

`build.py --far-menu` compiles the existing grouped MENU into the plain
main-flash window beginning at `0xEA000` (CPU `0x01EE5000`). Its bundle entry
becomes an eight-byte RAM trampoline. The build writes
`menu_0xEA000_OFFLINE.bin` and records its hash in the manifest. The reserved
window ends before the resident TX service at `0xFA000`. The normal `.fw`
package does **not** contain this far blob; a radio trial must install the
matching firmware, far blob and module bundle as one coordinated operation,
with a backup and readback of the affected far sectors.

This trial only relocates the existing Toolbox menu. It does not yet replace
the native Menu callback or implement stock settings. No output from this
branch is ready for an on-radio menu replacement test.

## Work required before changing the Menu callback

1. Inventory every native menu item, its saved field, legal values, immediate
   effects, and action screens. Preserve all ten active categories and their
   radio behavior, including channel programming, scan, Bluetooth, SMS, GNSS
   and APRS.
2. Add those items to the Toolbox list and implement their read, edit, save
   and action paths. The existing APRS settings journal remains available.
3. Bench-check a complete settings round trip and the normal radio key flow.
   Then redirect the pinned Menu callback to the new entry and remove only
   stock UI functions proven unused. Rebuild and validate the app-only
   package, far payload and module bundle together.
4. Flash with main-image and far-region backups, verify readback, then check
   radio settings, RF, Bluetooth, APRS and both recovery routes on hardware.

The source branch must remain an offline experiment until those steps pass.
