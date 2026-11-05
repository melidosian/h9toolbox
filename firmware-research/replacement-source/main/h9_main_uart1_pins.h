#ifndef H9_MAIN_UART1_PINS_H
#define H9_MAIN_UART1_PINS_H

#include <stdbool.h>
#include <stdint.h>

/* Stock app+0x078796 with channel selector 1, used by UART1 serial open.
 * GPIO IDs 0x20/0x21 share the third GPIO bank. */
typedef struct {
    volatile uint32_t *pinmux_1e5100;
    volatile uint32_t *gpio_1e5080;
} H9MainUart1Pins;

void h9_main_uart1_pins_bind_target(H9MainUart1Pins *pins);

/* Does not configure UART clocks or arbitrate GPIO ownership. */
bool h9_main_uart1_pins_configure(H9MainUart1Pins *pins,
                                  uint8_t tx_pin, uint8_t rx_pin);

#endif
