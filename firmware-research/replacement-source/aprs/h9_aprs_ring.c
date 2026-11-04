#include "h9_aprs_ring.h"

static size_t next_index(const H9AprsRing *ring, size_t index)
{
    return index + 1u == ring->capacity ? 0u : index + 1u;
}

bool h9_aprs_ring_init(H9AprsRing *ring, uint8_t *storage, size_t capacity)
{
    if (ring == NULL || storage == NULL || capacity < 2u) return false;
    ring->storage = storage;
    ring->capacity = capacity;
    ring->head = 0;
    ring->tail = 0;
    return true;
}

bool h9_aprs_ring_empty(const H9AprsRing *ring)
{
    return ring == NULL || ring->capacity < 2u || ring->head == ring->tail;
}

bool h9_aprs_ring_full(const H9AprsRing *ring)
{
    return ring != NULL && ring->capacity >= 2u &&
        next_index(ring, ring->tail) == ring->head;
}

bool h9_aprs_ring_push(H9AprsRing *ring, uint8_t byte)
{
    if (ring == NULL || ring->storage == NULL || ring->capacity < 2u ||
        h9_aprs_ring_full(ring)) return false;
    ring->storage[ring->tail] = byte;
    ring->tail = next_index(ring, ring->tail);
    return true;
}

bool h9_aprs_ring_pop(H9AprsRing *ring, uint8_t *byte)
{
    if (ring == NULL || byte == NULL || h9_aprs_ring_empty(ring)) return false;
    *byte = ring->storage[ring->head];
    ring->head = next_index(ring, ring->head);
    return true;
}
