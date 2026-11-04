#ifndef H9_MAIN_SERIAL_DMA_H
#define H9_MAIN_SERIAL_DMA_H

#include <stddef.h>
#include <stdint.h>

/* Stock app+0x0C5B4A consumes an underlying serial DMA ring with these four
 * fields. The board IRQ/DMA driver must update produced after writing data. */
typedef struct {
    const uint8_t *storage;
    uint32_t capacity;
    volatile uint32_t produced;
    uint32_t consumed;
} H9MainSerialDmaRing;

/* Matches H9MainAprsSessionCallbacks.serial_read. Returns copied bytes,
 * bounded by request, produced-consumed, and ring capacity. A power-of-two capacity is
 * required by the stock mask-based wrap. */
size_t h9_main_serial_dma_read(void *context, uint8_t *destination,
                               size_t request);

#endif
