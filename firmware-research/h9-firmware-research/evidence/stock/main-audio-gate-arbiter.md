# Main receive-audio gate arbiter

Source: pinned TD-H9 main v1.0.38 `app.bin`, SHA-256
`11a07bc0b7f23ba809451256004f26e31547f1c251bbbc5f02ef905c968f9cd3`.
The independent Pi32v2 r3 decode is saved in `audio-gate-control.asm`.
`link_main_check.py` pins the gate's guard/selection bytes at
app+`0x05224C..0x0522A6` to this stock image.

The stock function at app+`0x05224C` reads byte `0x11B4D`, whose bit 0 is
mirrored by the speaker GPIO writer at app+`0x052228`. If bit 0 already
matches the requested value, it returns. For an OFF request from a source
other than `13`, bit 2 of that byte blocks the request. Source `13` instead
sets or clears bit 2 to match its requested value. Sources `1`, `8`, `11`,
`12`, and `13` use the requested value directly. Other sources call the
larger policy function at app+`0x052102` before deciding whether to drive
the speaker GPIO. If that policy's effective value differs from the request,
this function skips the GPIO write.

The replacement `h9_main_audio_arbiter.*` ports this early decision and
delegates the unmapped policy through a callback. It returns a proposed
speaker action and next hold state; it does not run the stock function's
call-state side effect, configure audio hardware, or gate Bluetooth output.
The observed bench probe of the separate GPIO writer silenced only the
built-in speaker. Thus this arbiter is useful for avoiding a lost speaker
mute request, but it is not yet the requested dual-output APRS mute.

Ghidra's pseudocode renders the bit-2 guard as a comparison to
`0xFFFFFFFF`; the independent instructions at app+`0x052262..0x052272`
instead set a temporary value to `!requested`, AND it with extracted bit 2,
and return when the result is nonzero. This interpretation is reflected in
the source and host tests.

The policy function called for other sources at app+`0x052102..0x052228`
hashes to SHA-256
`dcdeae3eef2c26ed4c9a250da76ef6c205a1590bfb14fd0a7a4c14dace9135c1`.
Its corrected Pi32 instruction listing checks RAM `0x11B16` and
`0x12BF4` first, then branches by `0xC1B2` and `0xC1B4`. The
replacement `h9_main_audio_policy.*` ports those tests and the remaining
raw RAM-field branches into a pure decision callback for the arbiter.
The result of app+`0x01682C` is now supplied by
`h9_main_audio_status.*`. Stock app+`0x01677C` scans two 28-byte records
at RAM `0x1D3DC` and `0x1D3F8`. A record qualifies when byte `+0x12`
has bit `0x20`, halfword `+6` is nonzero, and halfword `+8` has neither
`0x2080` bit. If exactly one qualifies, app+`0x01682C` returns its byte
`+0xE`; if none qualifies, it returns `0x2B`. When both qualify, the
selector calls app+`0x016728`, which compares six bytes against a key
at `0x1D43C` and accepts the first matching record with bit `0x20` at
`+0x12`. The replacement exposes that unported comparator as a callback
and returns no record if ambiguity cannot be resolved. Selector `1`
instead returns the first active record with a `0x2080` flag. The entire
app+`0x016728..0x01683C` slice is pinned to the stock image, and the
host test composes the returned status with the speaker policy.

The records' owner, status-byte meaning, live RAM snapshot, speaker
application, and Bluetooth output hook remain unidentified. This policy
is an ordinary speaker-routing decision, not a dual-output APRS mute.
