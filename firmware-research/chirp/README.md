# CHIRP: verified writes above 0x3000, and serial / USB-C uploads on H9 Toolbox Firmware

A patch to the TD-H9 CHIRP module `tdh8.py` ("Ver beta 103126.0", Fred Trimble, GPL-2.0-or-later; issue
https://chirpmyradio.com/issues/12216, attachment 16113). The module is **not** copied here: get it from the issue,
then

    patch tdh8.py < tdh8-h9toolbox.patch

and load the result in CHIRP the way the issue describes for test modules (CHIRP's Load Module, which needs its developer mode turned on).

## Why

The stock firmware's serial / USB-C clone write only programs flash and erases only at addresses 0 and 0x5000, so a
write above 0x3000 (Morse, SMS, GNSS, APRS settings) that needs a bit set is ACKed but does not take (a 1 -> 0
change does). Bluetooth's write routine is sector-aware, which is why `ble-serial` always worked. H9 Toolbox Firmware
(build `b0135e0c` and later) sends serial / USB-C writes at 0x2000-0x3FFF through that routine, so uploads over the
K-plug or USB-C work there too. Details: `../resource-flash-log.md`, 2026-11-11.

## What the patch changes

* Blocks written at 0x3000 and above are **read back and compared** (one retry). A mismatch raises a `RadioError`
  that names the block and, when the radio kept old bits, says that this connection cannot set bits there on this
  firmware (use BLE, or H9 Toolbox Firmware). Before, the upload "succeeded" with no sign of the failure.
* The upload prompt says so.
* One bug fix in `_read_block`: the unexpected-header branch logged an unset `_msg` (a `NameError`); it now names the
  block.

Nothing else changes: radios that already work (BLE, or H9 Toolbox Firmware) pass the read-back, so nothing is slower
or different for them (9 extra block reads per upload). Read-back needs nothing from the firmware, so it also helps on
stock firmware, where it turns the silent failure into an error.

## Checks

`test_chirp_patch.py` imports the module with a stand-in for the CHIRP API (`chirp_stubs.py`) and runs its upload
against simulated radios from `../module-loader/test_serial_clone.py`: a radio with the sector-aware write takes every
change; stock serial is reported; clearing bits raises no false alarm; the original module passes silently on stock
serial (the bug) and works on the fixed radio.

    CHIRP_TDH8_ORIGINAL=tdh8.py CHIRP_TDH8_PATCHED=tdh8-patched.py python3 -I test_chirp_patch.py

On hardware (build `b0135e0c`): `../module-loader/serial_clone_test.py` (single-bit and `--range` tests) passed over USB-C and a CH340 K-plug, and an upload of a Talk Around change (a 0 -> 1 write at 0x3014) from the patched module in CHIRP worked over both and read back.
Not yet run: the error path on stock firmware, a Bluetooth (`ble-serial`) upload, a main-region channel edit through the patched module.

Also seen in the module (not changed): `_do_upload` compares `radio.ident_mode` with the tuple
`_enter_programming_mode` returns, so it always logs "Model mismatch during Upload!" and goes on.
