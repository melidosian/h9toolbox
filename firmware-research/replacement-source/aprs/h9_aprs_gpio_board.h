#ifndef H9_APRS_GPIO_BOARD_H
#define H9_APRS_GPIO_BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* Stock APRS 0x08002964, 0x080029D0, 0x080029E4 use GPIOB 0x50000400
 * bit 5. Its physical purpose on the TD-H9 board is not established. */
typedef struct {
    volatile uint32_t *rcc;
    volatile uint32_t *gpio_b;
} H9AprsGpioBoard;

void h9_aprs_gpio_board_bind(H9AprsGpioBoard *board,
                             volatile uint32_t *rcc,
                             volatile uint32_t *gpio_b);
void h9_aprs_gpio_board_bind_target(H9AprsGpioBoard *board);
bool h9_aprs_gpio_board_configure(H9AprsGpioBoard *board);
void h9_aprs_gpio_board_write(H9AprsGpioBoard *board, bool high);
bool h9_aprs_gpio_board_read(const H9AprsGpioBoard *board);

#endif
