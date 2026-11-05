#include "h9_main_uart1_pins.h"

#include <stddef.h>

void h9_main_uart1_pins_bind_target(H9MainUart1Pins *pins)
{
    if (pins == NULL) return;
    pins->pinmux_1e5100 =
        (volatile uint32_t *)(uintptr_t)0x001E5100u;
    pins->gpio_1e5080 =
        (volatile uint32_t *)(uintptr_t)0x001E5080u;
}

bool h9_main_uart1_pins_configure(H9MainUart1Pins *pins,
                                  uint8_t tx_pin, uint8_t rx_pin)
{
    volatile uint32_t *pinmux;
    volatile uint32_t *gpio;
    if (pins == NULL || pins->pinmux_1e5100 == NULL ||
        pins->gpio_1e5080 == NULL || tx_pin != 0x20u || rx_pin != 0x21u)
        return false;

    pinmux = pins->pinmux_1e5100;
    gpio = pins->gpio_1e5080;
    pinmux[9] |= 0x80u;
    pinmux[9] &= ~0x40u;
    pinmux[7] &= ~0x0Cu;
    pinmux[7] |= 0x04u;

    /* The stock pin getter indexes the GPIO-bank table by pin >> 4.
     * Pins 32 and 33 both resolve to 0x001E5080. */
    gpio[2] &= ~0x01u;
    gpio[3] |= 0x01u;
    gpio[2] |= 0x02u;
    gpio[4] |= 0x02u;
    gpio[5] &= ~0x02u;
    gpio[2] &= ~0x02u;
    gpio[3] |= 0x02u;
    return true;
}
