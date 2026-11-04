# APRS ADC interrupt state and flag handling

Source: captured APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
Ghidra's decoded IRQ handler is at `0x08002F8A`; its existing export is
`evidence/stock/aprs-adc-caller.txt`. The two branch callbacks were checked
in the local Ghidra project and against the image bytes:

- `0x08003240`: `70 47` (`BX LR`), followed by alignment bytes.
- `0x08002F80`: `70 47` (`BX LR`), immediately before the ADC data-read helper.

The handler tests `ADC_ISR & ADC_IER`. On bit `0x4` or `0x8`, it sets state
flag `0x200` unless state flag `0x10` is already set. For an `0x8` event with
ADC configuration bits `0xC00` clear and the driver's byte at `+0x19` clear,
it also changes interrupt enables and state depending on ADC control bit 2.
It then runs the receive/TX sample callback and writes `0xC` to `ADC_ISR`.
On bit `0x80`, it sets state flag `0x10000`, calls the empty callback at
`0x08003240`, and writes `0x80` to `ADC_ISR`. On bit `0x10`, it conditionally
sets event flag `2` based on driver word `+0x28` or ADC configuration bit 0,
calls the empty callback at `0x08002F80`, and clears the interrupt flag.

The replacement `h9_aprs_adc_board_irq` ports those state and register
effects, calls the controller once for each sample interrupt, and is bound
to the diagnostic image's IRQ 12 vector. It keeps the stock `+0x38/+0x3C`
flags as fields in its own board wrapper, not as a binary-compatible copy of
the whole vendor driver. Tests cover simultaneous sample/watchdog/sequence
flags and both sample sub-branches. The meaning of all state bits, hardware
write-one-to-clear behavior, interrupt timing, and actual sampled audio are
not yet verified on the radio.
