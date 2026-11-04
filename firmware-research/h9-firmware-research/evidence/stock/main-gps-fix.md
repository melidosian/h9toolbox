# Main APRS GPS sentence handling

Source: pinned stock v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The corrected Pi32v2 Ghidra project `H9mainFixed` was inspected at
app+`0x086656` (virtual `0x01E86756`) with `H9Inventory.java`;
the local detailed report is ignored under
`output/stock/main-gps-parser.txt`.

The incoming command-`0x10` collector at app+`0x086EF4` passes
newline-terminated `$` sentences to this parser. The stock parser rejects
null/non-`$` input and short strings, locates the `*` checksum suffix, XORs
the bytes after `$` up to `*`, and compares two hexadecimal checksum
characters. Its decompiled state writes include time/date bytes, a validity
flag, and coordinate-like numeric fields. It uses a 2000-based two-digit
year conversion. Ghidra reports unresolved and unimplemented Pi32
instructions in the rest of this long function, so its full sentence
dispatch, conversion details, and all side effects are **not established**.

The replacement `main/h9_main_gps_fix.*` therefore implements bounded,
checksum-validated RMC and GGA parsing as an explicit first-pass policy.
It accepts any two-letter uppercase talker ID, parses UTC time, RMC date,
fix validity, and signed latitude/longitude in degrees times `10^7`.
It clears position on a valid no-fix sentence and leaves the prior record
unchanged on malformed input. `h9_main_aprs_session` retains its raw
sentence callback and adds a parsed-fix callback. Host tests cover a valid
RMC, GGA in southern/western hemispheres, no-fix status, bad checksum,
invalid coordinate, and a framed serial sentence flowing through the
session. This is source-level behavior; the radio's GPS peer, UI updates,
and persistent data path remain untested.
