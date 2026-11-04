#ifndef H9_MAIN_APRS_UART1_LINK_H
#define H9_MAIN_APRS_UART1_LINK_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "h9_main_aprs_session.h"
#include "h9_main_ax25.h"
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
    uint8_t tx_frame[2u * H9_MAIN_AX25_MAX_PACKET + 10u];
    uint32_t tx_started_ms;
    uint32_t tx_timeout_ms;
    uint16_t tx_next_token;
    uint16_t tx_active_token;
    bool tx_packet_queued;
    bool tx_permit_acknowledged;
    bool tx_drained;
} H9MainAprsUart1Link;

typedef enum {
    H9_MAIN_APRS_TX_IDLE,
    H9_MAIN_APRS_TX_PERMIT_PENDING,
    H9_MAIN_APRS_TX_READY_TO_KEY,
    H9_MAIN_APRS_TX_WAITING,
    H9_MAIN_APRS_TX_DRAINED,
    H9_MAIN_APRS_TX_TIMED_OUT
} H9MainAprsTxStatus;

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

/* Main-loop TX operations for the replacement pair. Arm queues a version-
 * gated permit before the board keys RF. The board must wait until status is
 * READY_TO_KEY (the MCU acknowledged the permit), then key RF and call send.
 * Send queues command 0x00, and status succeeds only for the matching
 * final-sample event.
 * The board must stop PTT on timeout, then clear the permit and restore RX. */
uint16_t h9_main_aprs_uart1_link_tx_arm(H9MainAprsUart1Link *link,
                                         uint32_t now_ms,
                                         uint32_t timeout_ms);
bool h9_main_aprs_uart1_link_tx_send(H9MainAprsUart1Link *link,
                                      const uint8_t *packet, size_t length,
                                      uint32_t now_ms);
H9MainAprsTxStatus h9_main_aprs_uart1_link_tx_status(
    const H9MainAprsUart1Link *link, uint32_t now_ms);
bool h9_main_aprs_uart1_link_tx_clear(H9MainAprsUart1Link *link);

#endif
