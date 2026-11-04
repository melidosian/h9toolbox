#ifndef H9_APRS_UART2_DMA_H
#define H9_APRS_UART2_DMA_H

#include "h9_aprs_uart2_rx.h"

#include <stdbool.h>
#include <stdint.h>

/* USART2 at 0x40004400, DMA channel at 0x40020030, and request selector
 * at 0x4001001C. Stock uses two buffers; this path pauses its single
 * buffer during forwarding. */
typedef struct {
    volatile uint32_t *usart;
    volatile uint32_t *dma_controller;
    volatile uint32_t *dma_channel;
    volatile uint32_t *dma_request;
    volatile uint32_t *rcc;
    volatile uint32_t *nvic_iser;
    H9AprsUart2Rx *receive;
    uint32_t transfer_errors;
    uint32_t invalid_counts;
} H9AprsUart2Dma;

void h9_aprs_uart2_dma_bind(H9AprsUart2Dma *driver,
                             volatile uint32_t *usart,
                             volatile uint32_t *dma_controller,
                             volatile uint32_t *dma_channel,
                             volatile uint32_t *dma_request,
                             volatile uint32_t *rcc,
                             volatile uint32_t *nvic_iser,
                             H9AprsUart2Rx *receive);
void h9_aprs_uart2_dma_bind_target(H9AprsUart2Dma *driver,
                                    H9AprsUart2Rx *receive);
bool h9_aprs_uart2_dma_start(H9AprsUart2Dma *driver);
void h9_aprs_uart2_dma_usart_irq(H9AprsUart2Dma *driver);
void h9_aprs_uart2_dma_channel_irq(H9AprsUart2Dma *driver);
/* Processes one completed batch, then rearms the single DMA buffer. */
bool h9_aprs_uart2_dma_poll(H9AprsUart2Dma *driver,
                             H9AprsController *controller);

#endif
