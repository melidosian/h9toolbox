# Main built-in speaker GPIO boundary

Pinned main app: v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.

The independent Pi32v2 decode of app+`0x052228` shows a writer that passes
pin number `3` and the requested value to app+`0x0000D2`. It then changes
only bit 0 of RAM `0x00011B4D` to record the requested audio state. The pin
lookup at app+`0x0000BA` indexes a four-entry table at app+`0x000554` by
`pin >> 4`. The first entry is MMIO `0x001E5000`, so pin 3 addresses bit 3
of that bank. The pin writer samples MMIO `0x001E0200` before the update.
The source verifier checks the exact writer bytes and GPIO bank table against
the pinned image.

`replacement-source/main/h9_main_speaker_gate.*` ports this direct
pin/state writer. A controlled stock-derived probe on 2026-11-03 silenced the
built-in speaker while Bluetooth audio remained audible; restoring stock
returned speaker audio. This boundary is therefore insufficient for the
requested APRS mute on both outputs. It is also global rather than tied to
an APRS receive event. The source module is not connected to the audio policy
or to boot code; no linked diagnostic ELF is safe to flash.
