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
to match the planned side. It also requires the selected channel's stored TX
word (in 10 Hz units) to equal the planned frequency in Hz, so a changed
channel frequency cannot pass merely because the working copy changed with it.
Lock and Current modes also require the logical
side to still match. `h9_main_tx_send` now requires this snapshot after its
prepared side/frequency readback and before its only keying callback. A
host test reproduces a B logical side with an A Dual Watch route and confirms
that no packet send is allowed; it also checks a wrong copied frequency pair
and a changed selected-channel TX frequency.

`h9_main_rf_route_prepare` is a new replacement primitive for the unkeyed
board preparation step. It preserves the logical-side guard for Lock/Current,
sets the Dual Watch route byte to the planned A/B side when necessary, and
temporarily changes logical bit 7 for Auto. Its saved record restores the
original selectors after the transaction. A fake-RAM test starts with logical
B and Dual Watch route A, applies B-Lock preparation, and verifies that RF
setup would select B; it also checks Auto restoration and rejection of a
Lock request when the logical side is A. The host TX-service adapter uses the
helper for both B-Lock and B-Auto and restores the original selector bytes
after the send transaction. The `prepare_tx` callback now receives the full
plan, including whether Auto may change logical side. No live board adapter
calls the primitive yet. A live adapter must serialize Dual Watch updates across prepare,
readback, keying, and restore; the helper alone does not make those actions
atomic.

This guard operates on firmware state, not a measurement of emitted RF.
The double read can detect a selector change during the copy, but it is not
an atomic lock against a later Dual Watch update; a live adapter must
serialize state through the final keying decision.

The replacement TX service now requires `begin_route`/`end_route` callbacks.
It acquires the reservation before reading the logical side and releases it
only after TX stop and receive-state restoration, including failed setup or
readback. A failed acquire rejects the packet before preparation. The host
test checks that every TX callback runs while reserved and that early reject,
successful send, and cleanup failure release it. This is a replacement
transaction contract, not behavior established in stock firmware. It does
not itself identify the radio's scheduler or provide the live lock adapter.
The source now repeats position, frequency-policy, synth, and route readbacks
after the APRS permit acknowledgment, immediately before its keying callback.
Host tests change each input during permit arm and confirm no packet send.
This narrows the asynchronous wait window; a live route reservation is still
needed through keying and cleanup.
No stock ROM RF-register reader, physical PTT boundary, or live board adapter
has been established. The diagnostic main ELF remains unflashable.
