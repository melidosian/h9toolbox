#ifndef H9_MAIN_UART_RING_H
#define H9_MAIN_UART_RING_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "h9_main_aprs_receive.h"

#define H9_MAIN_UART_RING_CAPACITY 2048u
#define H9_MAIN_UART_DRAIN_LIMIT 1024u

/* The UART interrupt calls push; the poller calls drain. This models the
 * ring snapshot at stock app+0x087218. */
typedef struct {
    uint8_t storage[H9_MAIN_UART_RING_CAPACITY];
    volatile uint16_t write_index;
    volatile uint16_t read_index;
    volatile uint32_t dropped_bytes;
} H9MainUartRing;

typedef void (*H9MainUartFrame)(void *context,
                                const H9MainWireReceiver *receiver);

void h9_main_uart_ring_init(H9MainUartRing *ring);
/* Single-producer ISR handoff. Returns false and preserves unread data if
 * the ring is full. The board must serialize reset with the interrupt. */
bool h9_main_uart_ring_push(H9MainUartRing *ring, uint8_t byte);
/* Copies at most 1,024 pending bytes before advancing the read index, then
 * passes each completed frame to the callback. Returns bytes consumed. */
size_t h9_main_uart_ring_drain(H9MainUartRing *ring,
                               H9MainWireReceiver *receiver,
                               H9MainUartFrame on_frame, void *context);

#endif
