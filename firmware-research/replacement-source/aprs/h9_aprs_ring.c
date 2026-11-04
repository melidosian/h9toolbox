#include "h9_aprs_ring.h"

#include <stddef.h>

static size_t next_index(const H9AprsRing *ring, size_t index)
{
    return index + 1u == ring->capacity ? 0u : index + 1u;
}

bool h9_aprs_ring_init(H9AprsRing *ring, uint8_t *storage, size_t capacity)
{
    if (ring == NULL || storage == NULL || capacity < 2u) return false;
    ring->storage = storage;
    ring->capacity = capacity;
    atomic_init(&ring->head, 0u);
    atomic_init(&ring->tail, 0u);
    return true;
}

bool h9_aprs_ring_empty(const H9AprsRing *ring)
{
    return ring == NULL || ring->capacity < 2u ||
        atomic_load_explicit(&ring->head, memory_order_relaxed) ==
        atomic_load_explicit(&ring->tail, memory_order_acquire);
}

bool h9_aprs_ring_full(const H9AprsRing *ring)
{
    return ring != NULL && ring->capacity >= 2u &&
        next_index(ring, atomic_load_explicit(&ring->tail,
                                               memory_order_relaxed)) ==
        atomic_load_explicit(&ring->head, memory_order_acquire);
}

bool h9_aprs_ring_push(H9AprsRing *ring, uint8_t byte)
{
    size_t tail;
    if (ring == NULL || ring->storage == NULL || ring->capacity < 2u ||
        h9_aprs_ring_full(ring)) return false;
    tail = atomic_load_explicit(&ring->tail, memory_order_relaxed);
    ring->storage[tail] = byte;
    atomic_store_explicit(&ring->tail, next_index(ring, tail),
                          memory_order_release);
    return true;
}

bool h9_aprs_ring_pop(H9AprsRing *ring, uint8_t *byte)
{
    size_t head;
    if (ring == NULL || byte == NULL || h9_aprs_ring_empty(ring)) return false;
    head = atomic_load_explicit(&ring->head, memory_order_relaxed);
    *byte = ring->storage[head];
    atomic_store_explicit(&ring->head, next_index(ring, head),
                          memory_order_release);
    return true;
}

void h9_aprs_ring_clear_consumer(H9AprsRing *ring)
{
    size_t tail;
    if (ring == NULL || ring->storage == NULL || ring->capacity < 2u) return;
    tail = atomic_load_explicit(&ring->tail, memory_order_acquire);
    atomic_store_explicit(&ring->head, tail, memory_order_release);
}
