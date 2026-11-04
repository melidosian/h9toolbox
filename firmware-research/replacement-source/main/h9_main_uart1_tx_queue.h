#ifndef H9_MAIN_UART1_TX_QUEUE_H
#define H9_MAIN_UART1_TX_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "h9_main_uart1_tx.h"

/* The board supplies writable buffers and their 32-bit Pi32 DMA addresses.
 * Each slot remains owned by the queue through UART event 1. */
typedef struct {
    uint8_t *storage;
    uint32_t dma_address;
    size_t capacity;
    size_t length;
} H9MainUart1TxSlot;

typedef struct {
    H9MainUart1Tx *port;
    H9MainUart1TxSlot *slots;
    size_t slot_count;
    size_t head;
    size_t count;
    bool active;
    volatile uint8_t completed;
    uint32_t rejected;
    uint32_t start_failures;
} H9MainUart1TxQueue;

int h9_main_uart1_tx_queue_init(H9MainUart1TxQueue *queue,
                                 H9MainUart1Tx *port,
                                 H9MainUart1TxSlot *slots,
                                 size_t slot_count);
bool h9_main_uart1_tx_queue_enqueue(H9MainUart1TxQueue *queue,
                                     const uint8_t *bytes, size_t length);
/* Main-loop operation; releases a completed slot and starts the next one. */
void h9_main_uart1_tx_queue_pump(H9MainUart1TxQueue *queue);
/* IRQ event callback; only event 1 marks the in-flight DMA transfer done. */
void h9_main_uart1_tx_queue_event(void *context, unsigned event_number);
/* Fits H9MainAprsSessionCallbacks.serial_write. Rejections are counted. */
void h9_main_uart1_tx_queue_write(void *context, const uint8_t *bytes,
                                   size_t length);

#endif
