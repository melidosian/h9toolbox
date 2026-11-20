# Unified radio and Toolbox menu experiment

Branch: `codex/radio-menu-replacement`, based on `origin/dev` at `67e446b`.
The target is one sectioned on-radio menu for radio and H9 Toolbox settings,
entered with the normal Menu key. The stock menu UI must be removed only after
every stock setting and action has an equivalent path. Keep the stock setting
storage and radio control routines until their replacements are verified.

## Findings from the pinned factory image

`inventory.py` verifies the factory v1.0.38 app SHA-256 before reporting any
addresses. One record in the stock menu dispatch table at app+`0xB6BC4`
targets CPU `0x01E6D426` (app+`0x6D326`). Its role as an entry point is
unverified; it must not be patched as a Menu-key hook. The submenu builder at
app+`0x6811E` fills item IDs at RAM `0x12C77`.
The verified label inventory is saved in `stock-menu-v1.0.38.json`. The ten
active categories in the dev image are Radio Setting, Program Chann,
Analog Set, Radio Info, Scan, Bluetooth, SMS, GNSS, Site Type and APRS. Morse
is category 7 in the factory image and is disabled in the dev build. Its nine
rows are Morse On/Off, CW Terminal, CW Key, CW Speed, CW Tone, CW Power,
RX Threshold, CW Message and Reset. The
pinned submenu builder declares **119 entries in the factory image, 110
active in dev**. These include actions as well as saved settings; the CHIRP
setting list does not cover the entire on-radio menu.
The inventory also records the pinned category action jump table at
app+`0x76982`. These are category-level targets, not implementations for
individual rows. Radio Setting currently routes to the common epilogue;
its individual setting handlers still need tracing.

The starting grouped Toolbox MENU module was 6,246 bytes; the compatible
non-preview build after these changes is 6,270 bytes. Its safe ceiling is
6,528 bytes because shared satellite state begins at RAM `0x2B880`; only 258
bytes remain. A complete radio settings editor must live outside that slot.

## Offline unified list preview

`generate_catalog.py` turns the pinned JSON inventory into
`stock_menu_catalog.h`. With `--far-menu --radio-menu-preview`, the grouped
Toolbox UI includes all 110 active stock row labels under their ten native
category headings. The expanded list uses a three-digit position counter and
can jump to three-digit row numbers. Its list storage occupies the unused RAM
module slot at `0x29F20` while the far menu runs, rather than consuming the
radio task stack. Host tests exercise the ordinary and preview menus.
The preview body is 8,234 bytes at `0xEA000`; its compiled menu frame is
284 bytes. The preview's `.fw` package is byte-identical to the dev baseline,
so it does not contain this far body or a normal Menu-key hook.

Selecting a stock row only displays `STOCK CONTROL NOT WIRED`. No stock
setting is read, changed or saved through the preview, and the normal Menu
key still enters the stock UI. This flag is for offline layout and navigation
validation only. It is not an installable unified menu.

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

1. Inventory all 110 active native menu items, their saved fields, legal
   values, immediate effects, and action screens. Preserve all ten categories and their
   radio behavior, including channel programming, scan, Bluetooth, SMS, GNSS
   and APRS.
2. Add those items to the Toolbox list and implement their read, edit, save
   and action paths. The existing APRS settings journal remains available.
3. Trace the normal Menu-key call site. Bench-check a complete settings round
   trip and the normal radio key flow. Then redirect that call site to the new
   entry and remove only
   stock UI functions proven unused. Rebuild and validate the app-only
   package, far payload and module bundle together.
4. Flash with main-image and far-region backups, verify readback, then check
   radio settings, RF, Bluetooth, APRS and both recovery routes on hardware.

The source branch must remain an offline experiment until those steps pass.
