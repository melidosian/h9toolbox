# APRS burst activity between replacement processors

The stock APRS controller samples receive audio from PA1 and sends decoded
packets to the main radio over USART1. The replacement source adds a separate
activity signal so the main radio can eventually suppress an APRS burst on
both audible outputs while the APRS controller continues decoding. This is a
new protocol between **both replacement firmwares**, not a stock command.

The APRS controller sends framed command `0x52` with two payload bytes:
`{version=1, active=0|1}`. It reads the ADC demodulator's published
`in_frame`/`preamble_seen` state in the main loop, never sends from the ADC
interrupt, sends an event on each activity transition, and renews an active
event every 250 ms. A failed UART send leaves its state uncommitted so the
next loop can retry. The main APRS session accepts only the exact versioned
payload and calls `burst_activity`.

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
