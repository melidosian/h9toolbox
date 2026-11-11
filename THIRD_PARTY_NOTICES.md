# Third-party code in H9 Toolbox

## smartbeacon (APRS BEACON)

`firmware-research/module-loader/modules/aprs/third_party/smartbeacon/` holds `smartbeacon.c` and `smartbeacon.h`,
the speed- and turn-driven APRS beacon scheduler from <https://github.com/tomaszkuehn/TD-H9-aprs-smartbeacon>
(commit `4857fb6e1cb7621e3c834b599c87032d835a7a0b`), copyright (c) 2026 Tomasz Kuehn, MIT licence. The files and
the licence text are included unchanged. H9 Toolbox's APRS BEACON screen (`aprs_beacon.c`, `beacon_core.c`) feeds it
GPS speed and course and sends the position it asks for. The upstream project's documentation (CC BY 4.0) describes
how the author reverse-engineered the radio's APRS module; none of it is copied here.
