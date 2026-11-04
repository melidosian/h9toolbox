#include "h9_main_uart_ring.h"

#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#define H9_COMPILER_BARRIER() _ReadWriteBarrier()
#elif defined(__GNUC__) || defined(__clang__)
#define H9_COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
#error "A compiler memory barrier is required for the UART ISR handoff"
#endif

void h9_main_uart_ring_init(H9MainUartRing *ring)
{
    if (ring == NULL) return;
    memset(ring, 0, sizeof *ring);
}

bool h9_main_uart_ring_push(H9MainUartRing *ring, uint8_t byte)
{
    uint16_t write_index;
    uint16_t next;
    if (ring == NULL) return false;
    write_index = ring->write_index;
    if (write_index >= H9_MAIN_UART_RING_CAPACITY) return false;
    next = (uint16_t)((write_index + 1u) &
                      (H9_MAIN_UART_RING_CAPACITY - 1u));
    if (next == ring->read_index) {
        ring->dropped_bytes++;
        return false;
    }
    ring->storage[write_index] = byte;
    H9_COMPILER_BARRIER();
    ring->write_index = next;
    return true;
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
    H9_COMPILER_BARRIER();
    if (read_index >= H9_MAIN_UART_RING_CAPACITY ||
        write_index >= H9_MAIN_UART_RING_CAPACITY) return 0;
    count = (write_index - read_index) & (H9_MAIN_UART_RING_CAPACITY - 1u);
    if (count > H9_MAIN_UART_DRAIN_LIMIT) count = H9_MAIN_UART_DRAIN_LIMIT;
    for (index = 0; index < count; ++index)
        batch[index] = ring->storage[(read_index + index) &
                                     (H9_MAIN_UART_RING_CAPACITY - 1u)];
    H9_COMPILER_BARRIER();
    ring->read_index = (uint16_t)((read_index + count) &
                                  (H9_MAIN_UART_RING_CAPACITY - 1u));
    for (index = 0; index < count; ++index) {
        if (h9_main_wire_feed(receiver, batch[index]) == H9_MAIN_WIRE_FRAME &&
            on_frame != NULL)
            on_frame(context, receiver);
    }
    return count;
}
