# APRS controller serial transmit paths

Source: captured APRS v1.0.15 image, SHA-256 `1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`. Addresses below come from the ARM decompilation in `evidence/aprs-command-handlers.txt`, `evidence/aprs-tx-send-gate.txt`, and `evidence/aprs-tx-wave-scheduler.txt`. This is static evidence, not a transmitted RF capture.

Serial command `0x00` reaches the send gate at `0x08005760`, then packet encoder `0x0800534C` and the AFSK waveform queue. The replacement `h9_aprs_tx_runtime` models this route, but only accepts a packet while its logical permit is active. The replacement-only serial command `0x53` supplies a 10-second lease in the diagnostic board source; there is no verified physical RF-ready signal or main board adapter to send this command.

The stock parser at `0x08005B8C` accepts serial command `0x0F` and writes the
last received payload byte's nonzero state to RAM `0x2000010E` (initialized
to zero at `0x08005800`). When enabled, the send gate at `0x08005760`
waits until the packet encoder's flag at object offset `+0x32E` is set, then
sends serial command `0x0F` with one payload byte `01`. The encoder at
`0x0800534C` sets that flag after placing its closing `0x7E` in the waveform
queue. This is an **encoded/queued** status; it does not establish that the
radio emitted RF or finished playing the queued samples. The replacement
controller now stores the command setting and can send the status when the
TX service emits its one-time queued event. The diagnostic board entry keeps
RF transmit unverified, so that event has not been observed on hardware.

Serial command `0x11`, subcommand `4`, takes a separate route through `0x08005A94` and `0x08002A4C`. The latter reads GPIOB bit 5 through `0x08002964`; when the read equals 1, it calls `0x08002C94`, which reaches the synchronous UART writer at `0x0800476C`. The literal at `0x08002CA8` points to descriptor `0x20000D94`; initialization at `0x08002974` sets its peripheral base to USART2 `0x40004400` (`0x080029C4`) and its baud to `0x2580` (9,600). `0x08004484` enables USART2 and GPIOC clocks and selects GPIOC pins 0/1 with alternate function 4; `0x080029F8` configures GPIOB bit 5 as an output and initially clears it. The separately framed command reply path uses USART1 `0x40013800` at 115,200 baud. Thus `0x11/4` emits the payload bytes raw over USART2, not to the AFSK waveform queue or USART1 reply stream. The replacement dispatcher gates its distinct auxiliary UART callback on the GPIO read, and the diagnostic board entry now configures GPIOB bit 5 and USART2 before binding the raw writer. The external device attached to USART2 and electrical meaning of GPIOB bit 5 remain unverified.
