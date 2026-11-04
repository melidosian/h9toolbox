#ifndef H9_APRS_STATUS_H
#define H9_APRS_STATUS_H

#include <stdint.h>

#define H9_APRS_STATUS_BYTES 48u

/* Raw source fields in the status reply at stock 0x0800586C. Their semantic
 * meanings are still being traced, so callers must supply actual board state
 * instead of treating zero-initialized values as a valid status report. */
typedef struct {
    uint32_t words[10];
    uint32_t current_tick;
    uint16_t serial_halfword;
    uint8_t word_10_low;
    uint8_t serial_byte_a;
    uint8_t serial_byte_b;
} H9AprsStatusSnapshot;

void h9_aprs_status_encode(const H9AprsStatusSnapshot *snapshot,
                            uint8_t output[H9_APRS_STATUS_BYTES]);

#endif
