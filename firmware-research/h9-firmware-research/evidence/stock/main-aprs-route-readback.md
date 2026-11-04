# Main APRS TX route readback

Source: pinned main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The independent Pi32v2 decode and the source-input verifier pin the branch
at app+`0x058D4C` and A/B frequency copies at app+`0x0567E6` and
`0x0568F6`; see [the setup trace](main-aprs-tx-setup.md).

The APRS selector uses bit 7 of RAM `0x11B50` for its Lock decision. The RF
setup routine instead uses RAM `0x11AEC` when Dual Watch at `0x12BA7` is on.
It copies the chosen side's RX/TX words from `0x11CA0/0x11CA4` (A) or
`0x11CA8/0x11CAC` (B) into working words `0x11C8C/0x11C9C`.

The replacement `h9_main_rf_state_read` captures both selectors and the
frequency pairs, then rereads the selectors to reject an observed change.
`h9_main_rf_snapshot_matches_plan` requires the setup side and working pair
to match the planned side; Lock and Current modes also require the logical
side to still match. `h9_main_tx_send` now requires this snapshot after its
prepared side/frequency readback and before its only keying callback. A
host test reproduces a B logical side with an A Dual Watch route and confirms
that no packet send is allowed; it also checks a wrong copied frequency pair.

This guard operates on firmware state, not a measurement of emitted RF.
The double read can detect a selector change during the copy, but it is not
an atomic lock against a later Dual Watch update; a live adapter must
serialize state through the final keying decision.
No stock ROM RF-register reader, physical PTT boundary, or live board adapter
has been established. The diagnostic main ELF remains unflashable.
