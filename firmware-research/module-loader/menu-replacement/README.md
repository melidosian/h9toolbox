# Unified radio and Toolbox menu experiment

The local branch `codex/0.1.6-rc.1` is the H9 Toolbox Firmware 0.1.6 release
candidate.
The target is one sectioned on-radio menu for radio and H9 Toolbox settings,
entered with the normal Menu key. The stock menu UI must be removed only after
every stock setting and action has an equivalent path. Keep the stock setting
storage and radio control routines until their replacements are verified.

## Unified menu build

`--far-menu --unified-menu` builds the unified entry, settings and radio tools.
The coverage check accounts for all 110 active stock rows. It maps 105
to an implemented setting, action or Toolbox app and marks the five
radio SMS controls as retired. The nine CW rows remain disabled in this
dev build. The result has 122 stored fields, two inline APRS controls,
four APRS message entries and 16 native action rows.

The top level starts with Radio Settings, Channel Settings and APRS.
Radio Settings contains general settings and radio sections. Channel Settings
opens the list of 199 memories. Hold Up or Down to scroll, or type a
channel number to jump, then press Menu to edit it. Each row shows the
channel number followed by its long name or short name. Back returns to the same
place in the list. App bundles can declare their menu
category. Maze Chase is listed under Games.

Menu opens the highlighted folder or row. Back returns to the previous
folder, and Back at the top level returns to the radio. The menu restores
your folder and selection when a Toolbox app returns. Tap Up or Down to move
one row. Hold either key for half a second to start scrolling. Repeats
speed up at one second and again at 1.8 seconds. Keypad shortcuts are
numbered within the open folder.

Radio Settings → Display & buttons → Menu color uses the 18-color factory palette.
Changes preview live. Menu saves the color for the whole menu, and Back
restores the saved choice.

Press Menu on a value to edit its row. Use Up/Down or type a keypad value,
then press Menu to save or Back to cancel. Squelch accepts any digit from 0
to 9. Multi-digit inputs show their typed prefix and reject out-of-range
saves. Channel numbers, scan limits and the APRS beacon interval in
seconds also take digits. Text and DTMF sequences use the character editor.

APRS → APRS Beacon → Beacon Type selects Off, Timed, PTT or Smart. Timed
and PTT use the native triggers. Smart uses the resident scheduler. Saving
a type switches off the other triggers. Re-saving a legacy resident Timed
schedule converts its interval from minutes to native seconds. APRS Mute
is under APRS → APRS Settings.

APRS → Messages opens New Message, Inbox, Sent and Drafts directly in the
APRS client. It uses the existing APRS mailbox, replies and delivery
indicators. The Stations screen's 3 MSG tab opens the same Messages menu.
These APRS client changes require the app bundle produced with this firmware.

The menu programs all 199 memories. Channel records preserve
unrelated bits, store actual TX frequency for VFO repeater offsets, and
verify writes before publishing a memory. Use Up or Down for tone and
other choice rows. Channel frequency rows use MHz. Enter three MHz digits followed by up to
five fractional digits, then press Menu to save. For example, `14223`
saves as 142.230 MHz and `14223456` saves as 142.23456 MHz. Back cancels
the edit.

Each memory has an eight-character Short Name used by the stock image and
an optional 32-character Long Name. Edit either after selecting a memory
in Channel Settings. Name + number view uses a configured long name and falls
back to the short name when the field is empty. For an overflowing name,
the display holds the first page for one second, advances one cell every
230 ms, and holds the end for one second. Single, Dual and Classic use
their stock type size and name area. Deleting a memory in this editor
clears its long name when the label store is available.

The Toolbox programmer edits the same Long Name. CSV and JSON carry it
as optional channel metadata, and CHIRP .img files keep their stock
format. The programmer checks for the firmware's read-only H9LN reply at
clone address 0x3F00 before using the selector at 0x3F20 and data at
0x3F40. The clone response status and the label readback verify each save.

The programmer reads a backup automatically after connecting over Bluetooth
or USB-C. The working image and staged edits remain open if a backup was
already loaded. For USB-C, power on normally and select the radio's serial
port in Chrome or Edge. RC3 adds sector-aware writes for the channel
region and advertises them in bit 0 of H9LN status byte 7. Earlier firmware
can back up over USB-C and uses Bluetooth to write changes.

The DTMF and fixed position fields
use the native image formats verified against the TD-H9 CHIRP driver.

The action rows implement CTCSS/DCS seek, frequency capture, range scan,
spectrum, Bluetooth pairing, radio/modem information, the three stock
manual QR codes and confirmed resets. Bluetooth name and PIN are read-only,
as in the stock UI. Destructive reset actions require confirmation. Tests exercise guard refusal and
cancellation as well as the normal save paths.

Saved settings are applied when leaving the menu or opening an action.
The apply sequence follows the native post-reset reload. It reinitializes RF
and can interrupt an active Bluetooth session. The internal mic and speaker
controls apply immediately. A busy transmitter defers bulk apply and scan.
This path does not reset the CPU.

The firmware replaces five stock menu functions covering 11,950 bytes:
the root, list builder, category entry, setting getter and setting saver.
The replacement keeps the RF, storage and Bluetooth services it calls.
The loader enters the far menu directly after verifying its CRC, including
after a Toolbox app returns. Stock USB recovery remains available.

Run the host harnesses from `module-loader/`:

```text
python -I test_build.py
python -I menu-replacement/test_radio_menu_patch.py
python -I menu-replacement/test_coverage.py
python -I menu-replacement/run_host_tests.py
```

The C harnesses use the `h9-hostcc` Docker image. The coverage JSON generated
alongside the firmware lists every stock row and its corresponding endpoint.

## Channel screen and menu themes

Open **Radio Settings → Display & buttons → Toolbox theme** to preview
Focus, Split, Daylight, Focus Light, or Daylight Dark across the Toolbox
menu tree. Press MENU to apply the theme for the current power session.
Stock uses the existing Menu Color preference.

**Channel screen preview** is in the same folder. It snapshots the current
A/B frequencies, memory numbers, names, and selected side. Use UP/DN to
cycle through the five layouts and BACK to return. Focus and Focus Light
put the memory name first and wrap long active names across two lines.
VFOs and unnamed memories show the frequency in the primary position.
The native home screen continues to handle live receive, transmit, and
keypad states.

## Quick Change

Hold **Back** on the unlocked radio home screen to open a 3×3 grid,
including when the squelch is open. Press
`1`–`9` to choose the matching tile, or use Up/Down and Menu.
Back or `#` closes the grid. PTT exits to the radio. Hold `*` on the home
screen to lock or unlock the keypad. Hold `#` on the home screen to start
the radio’s normal scan. Press `0` in the grid to open Radio Settings.

| Key | Setting | Action |
| --- | --- | --- |
| 1 | TX power | Cycle Low, Mid and High |
| 2 | Squelch | Pick 0–9 with the keypad or arrows |
| 3 | Tone | Choose Off, CTCSS or DCS. Press `#` to select TX, RX or both |
| 4 | Resume | Cycle the scan resume policy through Time, Carrier and Stop |
| 5 | Monitor / TX Freq | Toggle the squelch latch on a memory. Edit the TX frequency in VFO mode |
| 6 | VOX | Toggle voice-operated transmit. Reuses the last active gain during this session |
| 7 | Dual watch | Toggle |
| 8 | Width | Toggle 12.5 or 25 kHz |
| 9 | Bright | Cycle levels 1–5 and apply the level immediately on the grid |

In an editor, Menu saves and Back cancels. Enter a VFO TX frequency as
eight digits (for example, 14560000 for 145.60000 MHz). Enter the receive
frequency to return to simplex. Split frequencies must be in the same
supported transmit band. Power, tones, split and bandwidth changes are
saved for the selected memory or VFO. Squelch, VOX, dual watch and
brightness are saved as radio-wide preferences. Monitor is a live latch.
With the squelch open, Squelch, Monitor and Bright are available. Other
RF settings that require a reload can be saved when the squelch is closed.
The keypad layout follows [RadioFoundry's FM quick settings](https://radiofoundry.org/docs/user/08-fm).

### Experimental 33 cm transmit

In **Radio Settings → Radio**, **900TX** follows **500TX**. It enables
902–928 MHz on the H9 Toolbox v1.0.38 base in Ham or unlocked mode. It
defaults to Off and remembers the saved preference across reboots. With it enabled, the home-screen VFO keypad
accepts 33 cm frequencies, Channel Settings accepts 33 cm RX and TX
frequencies, and Quick Change accepts repeater splits within the band.

The bench build has passed offline checks. Measure output power,
synthesizer lock, and spurious emissions into a dummy load before using
the radio on 33 cm.

## Findings from the pinned factory image

`inventory.py` verifies the factory v1.0.38 app SHA-256 before reporting any
addresses. One record in the stock menu dispatch table at app+`0xB6BC4`
targets CPU `0x01E6D426` (app+`0x6D326`). The initial inventory did not establish
its role. Subsequent tracing identified the 104-byte function as the root
builder; `radio_menu_patch.py` now verifies its complete original byte span
before replacing the function. Hardware entry/return behavior remains
unverified. The submenu builder at
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

## Flash and bench verification

The menu body runs at main flash `0xEA000`. The resident APRS body starts at
`0xFA000`. The app-only firmware package does not contain either payload.
`prepare_trial.py` checks the stock-derived package, both far payloads, the
module bundle and the APRS Beacon screen checksum, then packs both far
payloads into one contiguous flash file.

`flash_trial.ps1` runs the Windows backup and flash in one elevated session.
It identifies the BR23 USB updater, checks the chip and stock-derived package,
saves the pre-trial app and far sectors, and verifies complete readback. Keep
the backup directory out of Git. The ROM USB update shortcut is hold 8 on
power-on. The separate stock HID updater is PTT + 2.

The menu runs directly from main flash, with app modules loaded from the
existing resource slot. A compatible installed bundle can stay in place.
The generated module bundle provides the matching app set for a radio that
needs it. Read and retain the current resource bundle before replacing it.

The 2026-11-20 bench flash verified both the main application image and the
full far region. The first trial retained the installed app bundle. Boot,
Menu and settings behavior must be observed separately from flash readback.
