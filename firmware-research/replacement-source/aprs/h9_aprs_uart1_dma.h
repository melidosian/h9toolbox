#ifndef H9_APRS_UART1_DMA_H
#define H9_APRS_UART1_DMA_H

#include "h9_aprs_uart_receive.h"
#include <stdint.h>

/* Stock APRS v1.0.15: USART1 0x40013800, DMA channel 1 0x40020008.
 * Register pointers are injectable so the transfer/IRQ logic can be tested
 * without touching a radio. Clock, pins, baud, and NVIC are board setup. */
typedef struct {
    volatile uint32_t *usart;
    volatile uint32_t *dma_controller;
    volatile uint32_t *dma_channel;
    H9AprsUartReceive *receive;
} H9AprsUart1Dma;

void h9_aprs_uart1_dma_bind(H9AprsUart1Dma *driver,
                            volatile uint32_t *usart,
                            volatile uint32_t *dma_controller,
                            volatile uint32_t *dma_channel,
                            H9AprsUartReceive *receive);
void h9_aprs_uart1_dma_bind_target(H9AprsUart1Dma *driver,
                                   H9AprsUartReceive *receive);
/* Call after the board has enabled DMA/USART clocks and configured USART1. */
bool h9_aprs_uart1_dma_start(H9AprsUart1Dma *driver);
void h9_aprs_uart1_dma_rearm(void *context, uint8_t *buffer, size_t capacity);
void h9_aprs_uart1_irq(H9AprsUart1Dma *driver);
void h9_aprs_dma1_channel1_irq(H9AprsUart1Dma *driver);

#endif
