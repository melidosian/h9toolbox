# Replacement APRS transmit permit

The stock APRS controller accepts serial command `0x00` as an AX.25 packet
request. Its send gate watches demodulator software state, but the images
have not identified a physical RF-ready or PTT input on the APRS MCU.
The GPIOB5 controlled by stock command `0x11` gates raw USART2 traffic and
must not be treated as RF authorization.

The replacement pair uses command `0x53` with payload
`{version=1, enabled, token_lo, token_hi}`. A nonzero 16-bit token identifies
one transmit transaction. `enabled=1` starts or renews a 10-second
APRS-controller lease; `enabled=0` clears only a matching token. The APRS
controller accepts only the exact four-byte versioned
payload. Its board loop allows packet submission and waveform service only
while the lease is active, and cancels pending or active TX after expiration.
The ADC ISR acknowledges cancellation before the packet buffer is reused.
After applying a well-formed permit command, the APRS MCU replies with
`0x55 {version=1, enabled, token_lo, token_hi}`. The main UART link will not
queue a packet or report `READY_TO_KEY` until it receives the matching enable
acknowledgment. A missing acknowledgment times out without authorizing RF
keying. A disable acknowledgment reports command processing; a stale-token
disable cannot revoke a newer permit.

The main encoder refuses to build this command unless it has received both
module version `H9R0.01` and protocol version `V1.0.0`. This matters because
stock APRS firmware reduces unknown command `0x53` to setting `0x03`; a
replacement main must not send the permit to an older APRS image.

The APRS board sends replacement command `0x54` with
`{version=1, token_lo, token_hi}` only after the last waveform sample has
drained and the DAC path has returned to idle. The stock `0x0F` reply means
the closing flag entered the waveform queue; it does not mean RF has finished.
The main serial session rejects malformed/zero-token completion frames and
delivers a valid token to the UART1 link. That link matches the token against
its current transaction, forwards the event to the application, and reports
timeout before success if the deadline passes. It queues the version-gated
permit, packet, and clear frames in order. The board adapter must poll for
`READY_TO_KEY` before asserting PTT, then release PTT only for the current
completed transaction. A
canceled packet does not emit a completion frame. Loss of the completion frame
must time out and
stop PTT; the 10-second permit lease independently cancels the waveform.

The main TX service now requests a permit after its planned-side, frequency,
and Dual Watch route readbacks pass, then calls its sole RF-keying callback.
Cleanup requests PTT stop, clears the permit, restores receive routing, and
releases the route reservation, even if packet sending fails. A failed permit
request cannot reach the keying callback. The adapter's permit callback can
use the UART1 link to assign a fresh nonzero token. `send_packet` must queue
the packet, poll for the matching `0x54` event, and time out before reporting
success. A failed clear makes the transaction
report cleanup failure; the APRS lease is a second bound on that condition.

This is replacement protocol and source-level transaction code, not observed
stock behavior. The main radio board adapter still has to prove the actual
programmed frequency, assert/deassert PTT, send both permit frames with
completion/error handling, match the waveform-completion token, and serialize Dual
Watch state. Neither diagnostic ELF is a flashable firmware image. The
permit alone is not a physical RF measurement or evidence that CHB-Lock
emits on B.
