# Main audio module name table

Pinned TD-H9 main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The JLFS app virtual base is `0x01E00100`; the addresses below are virtual.

At app+`0x02DBD8`, consecutive pointer pairs refer to audio module names and
code. The first entries are:

| Name | Name in app | Function pointer | Function in app |
| --- | --- | --- | --- |
| `audio_mc_device` | `0x0A22E8` | `0x01E8A960` | `0x08A860` |
| `audio_dec_init` | `0x0A22FE` | `0x01E8A9B2` | `0x08A8B2` |
| `bt_dec` | `0x0A2317` | `0x01E8AAAE` | `0x08A9AE` |
| `tone_dec` | `0x0A231E` | `0x01E8BA78` | `0x08B978` |
| `effect_adj` | `0x0A2327` | `0x01E48870` | `0x048770` |
| `audio_adc_demo` | `0x0A2338` | `0x01E8BB0E` | `0x08BA0E` |

Independent Pi32v2 decoding shows the `audio_mc_device` function returning
`1`, `audio_dec_init` returning a byte from RAM `0x11AD0 + 720`, and `bt_dec`
calling another function before returning a Boolean. The names and pointer
table identify a path for tracing Bluetooth audio setup. They do **not**
establish where received RF audio is routed into Bluetooth or where it can be
muted independently of the APRS decoder. No Bluetooth mute MMIO register or
call boundary has been validated from these entries. The controlled speaker
GPIO probe left Bluetooth audio audible.
