# APRS burst activity between replacement processors

The stock APRS controller samples receive audio from PA1 and sends decoded
packets to the main radio over USART1. The replacement source adds a separate
activity signal so the main radio can eventually suppress an APRS burst on
both audible outputs while the APRS controller continues decoding. This is a
new protocol between **both replacement firmwares**, not a stock command.

The APRS controller sends framed command `0x52` with two payload bytes:
`{version=1, active=0|1}`. It reads the ADC demodulator's published
`preamble_seen` state in the main loop, never sends from the ADC
interrupt, sends an event on each activity transition, and renews an active
event every 250 ms. A failed UART send leaves its state uncommitted so the
next loop can retry. The main APRS session accepts only the exact versioned
payload and calls `burst_activity`.

The demodulator qualifies `preamble_seen` after six HDLC flags and clears it
after 97 ADC samples without a transition. Its separate `in_frame` state may
remain set after a packet while RF is idle, so it is not used as a mute
signal. This can let the first part of a preamble through before qualification;
the mute timing still needs measurement on hardware.

The main-side `H9MainAprsMuteLease` uses the receiving board's millisecond
clock. An active event is valid for 750 ms; an inactive event clears it
immediately. `h9_main_aprs_mute_should_silence` additionally requires the
user's mute option and a board-confirmed APRS receive route. The composed
`h9_main_aprs_mute_plan` preserves decoder feed while clearing both speaker
and Bluetooth audio decisions during a verified APRS burst. Its
`route_verified` input must come from the live receive-audio route, not the
selected display line or a TX setup snapshot. A missing clear or broken UART
therefore does not silence the radio indefinitely. **Neither physical output
gate is connected.** The APRS receive route, UI setting, persistence, and
hardware audio taps still need to be implemented and measured. Even after
those are connected, some preamble audio may precede decoder recognition.
See [the receive-audio route trace](../h9-firmware-research/evidence/stock/main-aprs-receive-audio-route.md)
for why the known Dual Watch and display-side bytes cannot verify this route.

The host test sends actual framed activity from the APRS controller to the
main session, checks renewal and inactive transitions, rejects an unknown
version, and checks the composed speaker/Bluetooth plan, route/option guards,
and lease expiry. Both diagnostic target links pass. The linked images are
not bootable or flashable firmware.

The separate AFSK integration test generates an AX.25 packet from its own
Bell 202 source, feeds the replacement ADC/demodulator, verifies a decoded
packet and active `0x52` report after the flag preamble, then feeds ADC silence
and verifies an inactive `0x52` report. It covers both 9,600 samples/s and
the recovered timer rate of approximately 9,524 samples/s, including one
DC-offset/noise case. It does not establish the live radio's ADC level,
speaker/Bluetooth gate, or channel route.
