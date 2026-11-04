#include "h9_main_uart_ring.h"

#include <string.h>

void h9_main_uart_ring_init(H9MainUartRing *ring)
{
    if (ring == NULL) return;
    memset(ring, 0, sizeof *ring);
}

size_t h9_main_uart_ring_drain(H9MainUartRing *ring,
                               H9MainWireReceiver *receiver,
                               H9MainUartFrame on_frame, void *context)
{
    uint8_t batch[H9_MAIN_UART_DRAIN_LIMIT];
    uint16_t read_index;
    uint16_t write_index;
    size_t count;
    size_t index;
    if (ring == NULL || receiver == NULL) return 0;
    read_index = ring->read_index;
    write_index = ring->write_index;
    if (read_index >= H9_MAIN_UART_RING_CAPACITY ||
        write_index >= H9_MAIN_UART_RING_CAPACITY) return 0;
    count = (write_index - read_index) & (H9_MAIN_UART_RING_CAPACITY - 1u);
    if (count > H9_MAIN_UART_DRAIN_LIMIT) count = H9_MAIN_UART_DRAIN_LIMIT;
    for (index = 0; index < count; ++index)
        batch[index] = ring->storage[(read_index + index) &
                                     (H9_MAIN_UART_RING_CAPACITY - 1u)];
    ring->read_index = (uint16_t)((read_index + count) &
                                  (H9_MAIN_UART_RING_CAPACITY - 1u));
    for (index = 0; index < count; ++index) {
        if (h9_main_wire_feed(receiver, batch[index]) == H9_MAIN_WIRE_FRAME &&
            on_frame != NULL)
            on_frame(context, receiver);
    }
    return count;
}
