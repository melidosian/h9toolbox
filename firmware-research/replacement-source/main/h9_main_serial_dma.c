#include "h9_main_serial_dma.h"

#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#define H9_DMA_READ_BARRIER() _ReadWriteBarrier()
#elif defined(__GNUC__) || defined(__clang__)
#define H9_DMA_READ_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
#error "A compiler memory barrier is required for the DMA ring handoff"
#endif

size_t h9_main_serial_dma_read(void *context, uint8_t *destination,
                               size_t request)
{
    H9MainSerialDmaRing *ring = (H9MainSerialDmaRing *)context;
    uint32_t capacity;
    uint32_t available;
    uint32_t position;
    size_t count;
    size_t first;

    if (ring == NULL || (destination == NULL && request != 0u)) return 0u;
    capacity = ring->capacity;
    if (ring->storage == NULL || capacity == 0u ||
        (capacity & (capacity - 1u)) != 0u) return 0u;
    H9_DMA_READ_BARRIER();
    available = ring->produced - ring->consumed;
    count = request < (size_t)available ? request : (size_t)available;
    if (count > (size_t)capacity) count = (size_t)capacity;
    position = ring->consumed & (capacity - 1u);
    first = count < (size_t)(capacity - position) ?
        count : (size_t)(capacity - position);
    if (first != 0u) memcpy(destination, ring->storage + position, first);
    if (count != first) memcpy(destination + first, ring->storage,
                               count - first);
    ring->consumed += (uint32_t)count;
    return count;
}
