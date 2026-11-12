# CHIRP: a note in the TD-H9's Radio Information, and a read-back of writes above 0x3000 (optional)

A small, optional patch to the TD-H9 CHIRP module `tdh8.py` ("Ver beta 103126.0", Fred Trimble, GPL-2.0-or-later; issue
https://chirpmyradio.com/issues/12216, attachment 16113). The module is **not** copied here: get it from the issue, then

    patch -p1 tdh8.py < tdh8-h9toolbox.patch

and load the result in CHIRP the way the issue describes for test modules (CHIRP's Load Module, which needs its
developer mode turned on).

## Why

The stock firmware's serial / USB-C clone write only programs flash and erases only at addresses 0 and 0x5000, so a
write above 0x3000 (Morse, SMS, GNSS, APRS settings) that needs a bit set is ACKed but does not take (a 1 -> 0 change
does). Bluetooth's write routine is sector-aware, which is why `ble-serial` always worked. H9 Toolbox Firmware (build
`b0135e0c` and later) sends serial / USB-C writes at 0x2000-0x3FFF through that routine, so uploads over the K-plug or
USB-C work there with **no change to the module's code**. Details: `../resource-flash-log.md`, 2026-11-11.

## What the patch does (30 lines added, 1 changed)

* **Radio Information**: the text the module already shows for the TD-H3 Plus / TD-H9 ("a ble-serial connection must be
  used ...") now says the TD-H9 also works over K-plug and USB-C serial if it runs H9 Toolbox Firmware
  (h9toolbox.com/firmware/), and that on other firmware a serial or USB-C upload can silently fail to set these.
* **Read-back**: blocks written at 0x3000 and above are read back and compared (one retry). A mismatch raises a
  `RadioError` naming the block, saying that stock firmware cannot set bits there over serial / USB-C and that BLE or H9
  Toolbox Firmware can, instead of the upload "succeeding" with no sign of the failure. It needs nothing from the
  firmware, so it also helps on stock firmware. Cost: 9 extra block reads per upload.

## Checks

`test_chirp_patch.py` imports the module with a stand-in for the CHIRP API (`chirp_stubs.py`) and runs its upload against
simulated radios from `../module-loader/test_serial_clone.py` (a radio with the sector-aware write takes every change;
stock serial is reported; clearing bits raises no false alarm; the original module passes silently on stock serial, the
bug), and checks the TD-H9's information text.

    CHIRP_TDH8_ORIGINAL=tdh8.py CHIRP_TDH8_PATCHED=tdh8-patched.py python3 -I test_chirp_patch.py

On hardware (build `b0135e0c`): `../module-loader/serial_clone_test.py` passed over USB-C and a CH340 K-plug; the
**unmodified** module uploaded a Talk Around change (0 -> 1 at 0x3014) over USB-C and it read back, and an earlier,
larger version of this patch did too over both (so no code change is needed for the writes). Not yet seen: this version
in CHIRP, the warning on stock firmware, the new text in Radio Information, a Bluetooth upload.

## Noticed in the module (not changed)

* `_do_upload` compares `radio.ident_mode` with the tuple `_enter_programming_mode` returns, so it always logs "Model
  mismatch during Upload!" and goes on.
* In `_read_block`, the unexpected-header branch logs `_msg` before it is set (a `NameError` if it is ever reached).
