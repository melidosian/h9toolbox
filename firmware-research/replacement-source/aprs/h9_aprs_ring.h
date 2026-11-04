#ifndef H9_APRS_RING_H
#define H9_APRS_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>

/* Queue layout and one-empty-slot rule follow stock 0x080053E0..0x08005470.
 * Atomic indices are a replacement synchronization policy for ISR/main use. */
typedef struct {
    uint8_t *storage;
    size_t capacity;
    /* Single consumer owns head; single producer owns tail. The ADC ISR
     * and board loop use this ring without a long interrupt-disabled copy. */
    _Atomic size_t head;
    _Atomic size_t tail;
} H9AprsRing;

bool h9_aprs_ring_init(H9AprsRing *ring, uint8_t *storage, size_t capacity);
bool h9_aprs_ring_empty(const H9AprsRing *ring);
bool h9_aprs_ring_full(const H9AprsRing *ring);
bool h9_aprs_ring_push(H9AprsRing *ring, uint8_t byte);
bool h9_aprs_ring_pop(H9AprsRing *ring, uint8_t *byte);
/* Only the consumer may discard queued bytes. New producer bytes appended
 * after the observed tail remain available. */
void h9_aprs_ring_clear_consumer(H9AprsRing *ring);

#endif
