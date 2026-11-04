#ifndef H9_APRS_UART2_DMA_H
#define H9_APRS_UART2_DMA_H

#include "h9_aprs_uart2_rx.h"

#include <stdbool.h>
#include <stdint.h>

/* USART2 at 0x40004400, DMA channel at 0x40020030, and request selector
 * at 0x4001001C. Stock uses two 1024-byte buffers. This replacement uses
 * one DMA buffer plus a 256-byte RX interrupt spill ring during forwarding. */
typedef struct {
    volatile uint32_t *usart;
    volatile uint32_t *dma_controller;
    volatile uint32_t *dma_channel;
    volatile uint32_t *dma_request;
    volatile uint32_t *rcc;
    volatile uint32_t *nvic_iser;
    H9AprsUart2Rx *receive;
    uint8_t spill[256];
    volatile uint8_t spill_head;
    volatile uint8_t spill_tail;
    volatile bool spill_active;
    uint32_t transfer_errors;
    uint32_t invalid_counts;
    uint32_t spill_drops;
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
/* Stop receive DMA and its USART interrupts only if no batch/spill is ready.
 * The check and shutdown are atomic with respect to device interrupts. */
bool h9_aprs_uart2_dma_stop(H9AprsUart2Dma *driver);
void h9_aprs_uart2_dma_usart_irq(H9AprsUart2Dma *driver);
void h9_aprs_uart2_dma_channel_irq(H9AprsUart2Dma *driver);
/* Forwards the completed DMA batch and any bytes collected meanwhile by
 * the RX interrupt, then rearms DMA. */
bool h9_aprs_uart2_dma_poll(H9AprsUart2Dma *driver,
                             H9AprsController *controller);

#endif
