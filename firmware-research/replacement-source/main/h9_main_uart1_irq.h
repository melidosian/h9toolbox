#ifndef H9_MAIN_UART1_IRQ_H
#define H9_MAIN_UART1_IRQ_H

#include <stdint.h>

#include "h9_main_serial_dma.h"

/* UART1 is the stock TD-H9 APRS serial route for pins 0x20/0x21 when free.
 * registers points at MMIO 0x001E2100 on target; tests use fake words. */
typedef struct {
    volatile uint32_t *registers;
    H9MainSerialDmaRing *receive;
    uint32_t receive_chunk_bytes;
    void (*acknowledge_tx_dma)(void *context);
    void (*acknowledge_rx_dma)(void *context);
    void (*event)(void *context, unsigned event_number);
    void *context;
} H9MainUart1Irq;

/* Stock app+0x0C5EC8 events: 1=TX complete, 2=full RX chunk,
 * 3=partial RX chunk. The acknowledge callbacks must provide the stock
 * ROM DMA operation before this is bound to the actual interrupt vector. */
void h9_main_uart1_irq(H9MainUart1Irq *irq);

#endif
