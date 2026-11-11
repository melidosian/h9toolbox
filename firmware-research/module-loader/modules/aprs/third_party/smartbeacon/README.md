# smartbeacon (third party)

`smartbeacon.c` and `smartbeacon.h` are the portable smart-beaconing scheduler from
https://github.com/tomaszkuehn/TD-H9-aprs-smartbeacon (commit `4857fb6e1cb7621e3c834b599c87032d835a7a0b`,
2026-10-21), copyright (c) 2026 Tomasz Kuehn, **MIT licence** (`LICENSE`, here unchanged).
The files are vendored byte for byte. H9 Toolbox's APRS BEACON screen (`../../aprs_beacon.c`) feeds it
GPS speed and course once a second and sends a position when `sb_tick()` returns 1.
