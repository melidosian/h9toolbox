#include "h9_main_uart1_tx.h"

#include <stddef.h>

#define H9_MAIN_UART1_TX_DMA_DESCRIPTOR 0x001E3B0u

int h9_main_uart1_dma_start(H9MainUart1Tx *tx, uint32_t source_address,
                             uint32_t length)
{
    volatile uint32_t *uart;
    if (tx == NULL || tx->registers == NULL) return 0;
    if (length == 0u) return 1;
    if (tx->prepare_dma == NULL || tx->enable_dma == NULL) return 0;
    uart = tx->registers;
    tx->prepare_dma(tx->context, H9_MAIN_UART1_TX_DMA_DESCRIPTOR);
    uart[0] |= 0x2000u;
    uart[0] |= 0x0004u;
    uart[5] = source_address;
    uart[6] = length;
    tx->enable_dma(tx->context, H9_MAIN_UART1_TX_DMA_DESCRIPTOR, 0u);
    uart[0] &= ~0x0004u;
    return 1;
}
