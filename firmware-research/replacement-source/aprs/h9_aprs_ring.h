#ifndef H9_APRS_RING_H
#define H9_APRS_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Reconstructs the byte queue at 0x080053E0, 0x08005406, 0x0800543E,
   and 0x08005470. One slot remains unused to distinguish full from empty. */
typedef struct {
    uint8_t *storage;
    size_t capacity;
    size_t head;
    size_t tail;
} H9AprsRing;

bool h9_aprs_ring_init(H9AprsRing *ring, uint8_t *storage, size_t capacity);
bool h9_aprs_ring_empty(const H9AprsRing *ring);
bool h9_aprs_ring_full(const H9AprsRing *ring);
bool h9_aprs_ring_push(H9AprsRing *ring, uint8_t byte);
bool h9_aprs_ring_pop(H9AprsRing *ring, uint8_t *byte);

#endif
