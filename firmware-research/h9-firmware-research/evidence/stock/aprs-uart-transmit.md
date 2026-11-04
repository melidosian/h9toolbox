# APRS-controller UART transmit path

Source: pinned `APRS-1.0.15.bin`, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Addresses use image base `0x08002000`.

`0x08004894` frames outgoing serial messages with `0xC0` delimiters and
`0xDB` escaping, sending each byte via `0x08004870`. The latter follows the
USART1 descriptor installed by `0x08005F20` and calls `0x0800476C` with
one byte and a timeout value of 10. `0x0800476C` waits for status bit `0x80`
(transmit data register empty), writes the byte to USART offset `+4`, and
waits for status bit `0x40` (transmission complete). Its timeout helper at
`0x08004EF2` uses the stock millisecond tick and returns an error when a
flag does not arrive.

The replacement's `h9_aprs_uart1_tx.*` performs the same register waits and
byte write against injectable USART registers. It uses a caller-supplied
bounded poll count because the millisecond tick has not been ported. On a
timeout it latches a fault and suppresses later fragments. The controller
checks this fault after streaming a frame so its public send operation can
report failure. The diagnostic board entry binds the real USART1 address
and supplies this callback for command replies and forwarded packets.

Host fake-register tests exercise a serial version reply and both timeout
flags. No physical UART waveform or radio-to-radio exchange has been tested;
the diagnostic image still lacks ADC/DAC and APRS RF hardware integration.
