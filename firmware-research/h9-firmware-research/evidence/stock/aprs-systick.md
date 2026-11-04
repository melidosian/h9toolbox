# APRS controller tick and SysTick vector

Source: APRS v1.0.15 image, SHA-256
`1dbbc10425f186f22a4d82d73f139f1e3cc3388931b73eb8ed18b6e3087ccd33`.
The stock vector at slot 15 (flash offset `0x3C`) is `0x08004985`, the Thumb
entry at `0x08004984`. It calls `0x0800389C`, which adds the word at RAM
`0x200000CC` to the counter at `0x200000C4`. The getter at `0x08003890`
returns that counter. The status reply at `0x0800586C` uses it for the
two-byte elapsed field.

The setup path at `0x08005DE8` calls `0x080038B4`. Its helper at
`0x08003EE8` writes the period minus one to SysTick LOAD
(`0xE000E014`), clears VAL (`0xE000E018`), and writes `7` to CTRL
(`0xE000E010`), enabling the processor-clock timer and interrupt. The
configuration derives its period from the selected system clock and the
configured tick step. The exact stock initial step value was not recovered
from its startup data, so the replacement board entry chooses a 1 ms step
after selecting the observed 24 MHz internal clock.

`replacement-source/aprs/h9_aprs_systick.*` ports the bounded register
setup, counter, and vector-15 handler. Host tests check a 24 MHz, 1 ms
reload of `23999`, another valid step, and tick increments. The diagnostic
ARM link checks vector slot 15 by symbol as well as the existing DMA, ADC,
and USART slots. This adds a working timebase to the replacement source,
but no radio boot or timing measurement has validated it.
