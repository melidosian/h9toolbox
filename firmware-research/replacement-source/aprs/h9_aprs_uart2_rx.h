#ifndef H9_APRS_UART2_RX_H
#define H9_APRS_UART2_RX_H

#include "h9_aprs_controller.h"

#include <stdbool.h>
#include <stdint.h>

/* Stock USART2 uses two 1024-byte DMA buffers. The replacement stores one
 * 1024-byte batch for either the IRQ fallback or the DMA board path. The
 * DMA adapter adds a separate 256-byte spill ring during forwarding. */
#define H9_APRS_UART2_BATCH_CAPACITY 1024u
typedef struct {
    volatile uint32_t *usart;
    volatile uint32_t *nvic_iser;
    uint8_t bytes[H9_APRS_UART2_BATCH_CAPACITY];
    volatile uint16_t length;
    volatile bool ready;
    uint32_t overrun_count;
    uint32_t dropped_count;
} H9AprsUart2Rx;

void h9_aprs_uart2_rx_bind(H9AprsUart2Rx *receiver,
                            volatile uint32_t *usart,
                            volatile uint32_t *nvic_iser);
void h9_aprs_uart2_rx_bind_target(H9AprsUart2Rx *receiver);
bool h9_aprs_uart2_rx_start(H9AprsUart2Rx *receiver);
void h9_aprs_uart2_rx_irq(H9AprsUart2Rx *receiver);
/* Call from the board loop. Returns true only if a chunk was forwarded. */
bool h9_aprs_uart2_rx_poll(H9AprsUart2Rx *receiver,
                            H9AprsController *controller);

#endif
