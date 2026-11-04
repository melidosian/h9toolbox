#ifndef H9_MAIN_APRS_UART1_LINK_H
#define H9_MAIN_APRS_UART1_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "h9_main_aprs_session.h"
#include "h9_main_serial_dma.h"
#include "h9_main_uart1_irq.h"
#include "h9_main_uart1_tx_queue.h"

/* Board-owned ROM DMA operations. They must be mapped before a real IRQ or
 * serial open is enabled. This adapter does not configure clocks or pins. */
typedef struct {
    void (*prepare_tx)(void *context, uint32_t descriptor_address);
    void (*enable_tx)(void *context, uint32_t descriptor_address,
                      uint32_t option);
    void (*acknowledge_tx)(void *context);
    void (*acknowledge_rx)(void *context);
    void *context;
} H9MainAprsUart1DmaHooks;

typedef struct {
    H9MainAprsSession session;
    H9MainSerialDmaRing receive;
    H9MainUart1Tx transmit;
    H9MainUart1TxQueue transmit_queue;
    H9MainUart1Irq irq;
    H9MainAprsUart1DmaHooks dma;
    H9MainAprsSessionCallbacks application;
    void *application_context;
} H9MainAprsUart1Link;

/* Receive storage is the board's DMA ring. Slots carry actual Pi32 DMA
 * addresses and retain outgoing frame bytes until the TX-complete interrupt.
 * The application callbacks receive application_context, not link. */
int h9_main_aprs_uart1_link_init(
    H9MainAprsUart1Link *link, volatile uint32_t *uart_registers,
    const uint8_t *receive_storage, uint32_t receive_capacity,
    uint32_t receive_chunk_bytes, H9MainUart1TxSlot *transmit_slots,
    size_t transmit_slot_count, const H9MainAprsUart1DmaHooks *dma,
    const H9MainAprsSessionCallbacks *application,
    void *application_context);

/* Call from the main loop and the UART1 vector respectively. */
size_t h9_main_aprs_uart1_link_poll(H9MainAprsUart1Link *link);
void h9_main_aprs_uart1_link_irq(H9MainAprsUart1Link *link);

#endif
