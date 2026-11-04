# Replacement APRS transmit permit

The stock APRS controller accepts serial command `0x00` as an AX.25 packet
request. Its send gate watches demodulator software state, but the images
have not identified a physical RF-ready or PTT input on the APRS MCU.
The GPIOB5 controlled by stock command `0x11` gates raw USART2 traffic and
must not be treated as RF authorization.

The replacement pair uses command `0x53` with payload `{1, enabled}`.
`enabled=1` starts or renews a 10-second APRS-controller lease; `enabled=0`
clears it. The APRS controller accepts only the exact two-byte versioned
payload. Its board loop allows packet submission and waveform service only
while the lease is active, and cancels pending or active TX after expiration.
The ADC ISR acknowledges cancellation before the packet buffer is reused.

The main encoder refuses to build this command unless it has received both
module version `H9R0.01` and protocol version `V1.0.0`. This matters because
stock APRS firmware reduces unknown command `0x53` to setting `0x03`; a
replacement main must not send the permit to an older APRS image.

The main TX service now requests a permit after its planned-side, frequency,
and Dual Watch route readbacks pass, then calls its sole RF-keying callback.
Cleanup requests PTT stop, clears the permit, restores receive routing, and
releases the route reservation, even if packet sending fails. A failed permit
request cannot reach the keying callback. A failed clear makes the transaction
report cleanup failure; the APRS lease is a second bound on that condition.

This is replacement protocol and source-level transaction code, not observed
stock behavior. The main radio board adapter still has to prove the actual
programmed frequency, assert/deassert PTT, send both permit frames with
completion/error handling, wait for waveform completion, and serialize Dual
Watch state. Neither diagnostic ELF is a flashable firmware image. The
permit alone is not a physical RF measurement or evidence that CHB-Lock
emits on B.
