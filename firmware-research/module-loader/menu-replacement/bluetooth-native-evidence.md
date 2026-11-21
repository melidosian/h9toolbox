# Native Bluetooth services used by the replacement

Static analysis against factory TD-H9 v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
Offsets below are app offsets; CPU addresses add `0x01E00100`. This is static
evidence, not an observed radio connection or device write.

- BT Name is a native read-only view: `0x6CA54`'s Bluetooth row 2 calls the
  value renderer `0x6792C`, which reads VM key 101, 32 bytes, using
  `syscfg_read` at `0x2C42A`. The reset/default builder at `0x70D22` writes
  the matching classic name with VM write `0x2C39C`. The replacement displays
  that actual name and does not introduce an unverified name setter.
- BT PIN Code is also read-only in the native menu: row 10 at `0x6CB68`
  renders the constant `000000` at app+`0xB1741`. No PIN write is supplied.
- BT Int Mic and BT Int Spk read config RAM `0xC0F0 + 203/204`; the native
  setting saver at `0x73BC2/0x73BD0` calls `0x50704/0x50720` respectively.
  Both update that RAM and apply the hardware control. These controls have no
  saved partition byte in the stock row path, so the new virtual fields apply
  immediately without a CPU restart.
- Bluetooth pairing discovery uses the same 980-byte native pool layout as
  `0x6CDD0..0x6CE40`: ten 12-byte entries, ten 40-byte names, ten six-byte
  addresses, then ten 40-byte display strings. Core RAM `0x11AD0 + 532/536`
  holds the pool/list; config+199 selects new discovery (1) or saved peers (2),
  and config+198 is the result count. The asynchronous callback at
  `0x89A8A..0x89AD8` checks these fields before appending, with a ten-peer cap.
- `0x6A778` selects the native Bluetooth role; `0x4E69C` starts inquiry, as
  called by the stock discovery path at `0x6CE54/0x6CE5E`.
  `0x6A892` fills that same pool from the paired-peer partition. Its lower
  readers are `0x6A858/0x6A880`.
- `0x75720` is the native **connect-and-remember backend**, not its pairing
  screen. It stops prior controller operations, connects the selected entry
  through `0x756F2 -> 0x62EE0`, and maintains the ten-record paired list through
  `0x756F6/0x75708`. It does not call an LCD renderer. The old wait/status UI
  begins at `0x7589E`; that screen is not reused. Config+194 equals 2 on the
  native successful-connection path (`0x640A4..0x640B2`).
- The replacement disables the asynchronous list mode before releasing the
  pool, sends native command 17 used by the connect path, and calls the native
  pool cleanup `0x6A78A`. Its screen handles selection, retry, success,
  disconnect, timeout and Back using the Toolbox graphics/key interface.
- BT Reset's native saver `0x73C1E -> 0x7401A` calls `0x70F60`. That backend
  resets the paired-peer area, regenerates the VM name/address records and
  writes partition 4 offset 7969 with `{0,0,3,2,0,0}` for On/Mode/gains/OD.
  The replacement calls it only after its caller's confirmation and a ready
  resource chip, then verifies this partition readback and runtime defaults.

Keep the following service helpers if removing native menu drawing code:
`0x6A778`, `0x6A78A`, `0x6A858`, `0x6A880`, `0x6A892`, `0x756F2`, `0x756F6`,
`0x75708`, `0x75720` and their transitive non-UI callees. The inquiry callback
at `0x89A8A` must remain active.

Validation: `test_radio_bluetooth.c` exercises the production UI/storage layer
with a mock platform, including direct inline control writes, range/readback
failures, actual name/PIN display, asynchronous discovery, saved-peer display,
selection/connection and cleanup. GCC host checks pass with UBSan/bounds.
The production C and native assembly also cross-compile/link with JieLi Pi32v2.
Hardware name/readback, discovery, headset connection, audio, reset and cancel
behavior remain to be checked on a radio.
