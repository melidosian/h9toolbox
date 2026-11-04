#ifndef H9_APRS_UART2_BOARD_H
#define H9_APRS_UART2_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Stock 0x08002974 selects USART2 at 0x40004400, 9600 baud. Its raw
 * command-0x11/4 output is gated by GPIOB bit 5 before reaching this port. */
typedef struct {
    volatile uint32_t *rcc;
    volatile uint32_t *gpio_c;
    volatile uint32_t *usart;
    uint32_t poll_limit;
    uint32_t bytes_completed;
    uint32_t timeouts;
    bool faulted;
} H9AprsUart2Board;

void h9_aprs_uart2_board_bind(H9AprsUart2Board *board,
                               volatile uint32_t *rcc,
                               volatile uint32_t *gpio_c,
                               volatile uint32_t *usart,
                               uint32_t poll_limit);
void h9_aprs_uart2_board_bind_target(H9AprsUart2Board *board,
                                      uint32_t poll_limit);
bool h9_aprs_uart2_board_configure(H9AprsUart2Board *board,
                                    uint32_t system_clock_hz);
/* Return completed bytes; latch fault after a bounded TXE/TC timeout. */
size_t h9_aprs_uart2_board_write(H9AprsUart2Board *board,
                                  const uint8_t *data, size_t length);

#endif
