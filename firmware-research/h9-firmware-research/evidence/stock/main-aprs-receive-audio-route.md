# APRS receive-audio side is not yet identified

Source: pinned TD-H9 main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Addresses below are Pi32 virtual addresses; subtract `0x01E00100` for app
file offsets. The observations come from the fixed Pi32v2 Ghidra project and
the independent decoder's checks of the same image.

The Dual Watch route byte at RAM `0x11AEC` is read by the RF setup selector
at `0x01E58E3E`. Its recognized direct writes are at `0x01E5CE06`, where
the restore/setup path copies the selected channel-state bit, and at
`0x01E5C45C`/`0x01E5C4D4`, where the APRS TX builder changes and restores
the side. It therefore cannot by itself certify which channel's receive
audio is currently audible. The working receive frequency at RAM `0x11C8C`
is copied from A or B during RF setup and also consumed by the APRS TX path;
matching it to the APRS channel is insufficient proof of the live audio tap.

The stock side helper at `0x01E50F58` returns `0x11B50` bit 7 when
`0x11B1E` is zero, otherwise `0x11B1D`. Its callers at `0x01E5A22C` and
nearby code use the result to arrange the two frequency/signal displays.
The `0x11B1D` byte is also written by the APRS TX builder and by the keypad
state handler at `0x01E525A0`. These uses do not establish a receiver-output
side signal.

The audio-side callback at `0x01E521CE` calls `0x01E65FA6` and
`0x01E8B7F2` when its guard allows an update. The latter toggles bit 2 in
an allocated audio object and notifies its linked listeners; that object is
allocated in a path that checks the `fm_inside` module name. This trace
does not identify the RF-to-Bluetooth analog tap or a Bluetooth mute control.
The separate speaker GPIO probe left Bluetooth audio audible.

These are recognized direct references in the current analyzed project;
indirect RAM writes may exist. The replacement mute plan therefore requires
an independently verified live receive-audio route before it suppresses
either output. The next useful trace is from the squelch/open event through
the selected AF output, including the separate APRS ADC and Bluetooth tap.
