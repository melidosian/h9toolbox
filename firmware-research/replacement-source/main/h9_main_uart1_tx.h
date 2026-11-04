#ifndef H9_MAIN_UART1_TX_H
#define H9_MAIN_UART1_TX_H

#include <stdint.h>

/* Stock UART1 DMA TX uses UART MMIO 0x001E2100 and descriptor 0x001E3B0.
 * The two ROM DMA operations are supplied by the board layer. */
typedef struct {
    volatile uint32_t *registers;
    void (*prepare_dma)(void *context, uint32_t descriptor_address);
    void (*enable_dma)(void *context, uint32_t descriptor_address,
                       uint32_t option);
    void *context;
} H9MainUart1Tx;

/* Port of stock app+0x0C5E94. source_address is a 32-bit Pi32 DMA address;
 * the caller owns the buffer until UART event 1 reports completion. Returns
 * zero if the required board hooks are absent, before touching registers. */
int h9_main_uart1_dma_start(H9MainUart1Tx *tx, uint32_t source_address,
                             uint32_t length);

#endif
