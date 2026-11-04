#ifndef H9_APRS_UART1_BOARD_H
#define H9_APRS_UART1_BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* Stock APRS v1.0.15: RCC 0x40021000, USART1 GPIO 0x50000400,
 * USART1 0x40013800, NVIC ISER 0xE000E100. The system clock itself
 * must already be established by the board clock initializer. */
typedef struct {
    volatile uint32_t *rcc;
    volatile uint32_t *gpio;
    volatile uint32_t *usart;
    volatile uint32_t *nvic_iser;
} H9AprsUart1Board;

void h9_aprs_uart1_board_bind(H9AprsUart1Board *board,
                              volatile uint32_t *rcc,
                              volatile uint32_t *gpio,
                              volatile uint32_t *usart,
                              volatile uint32_t *nvic_iser);
void h9_aprs_uart1_board_bind_target(H9AprsUart1Board *board);
uint32_t h9_aprs_uart1_peripheral_clock(uint32_t system_clock_hz,
                                        uint32_t rcc_prescaler);
uint32_t h9_aprs_uart_brr(uint32_t peripheral_clock_hz, uint32_t baud);
uint32_t h9_aprs_uart1_brr(uint32_t peripheral_clock_hz);
/* Set clocks, pins 6/7, and 115200 8N1 USART1. Call before DMA start. */
bool h9_aprs_uart1_board_configure(H9AprsUart1Board *board,
                                   uint32_t system_clock_hz);
/* Enable IRQ 9/27 after DMA flags have been cleared and receive is armed. */
void h9_aprs_uart1_board_enable_irqs(H9AprsUart1Board *board);

#endif
