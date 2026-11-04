#ifndef H9_MAIN_CLOCK_TIMING_H
#define H9_MAIN_CLOCK_TIMING_H

#include <stdbool.h>
#include <stdint.h>

/* Stock main app+0x0C51CA, called after the clock source/divider search.
 * The selected calibration record has five 32-bit words. */
typedef struct {
    volatile uint32_t *record_words_da08;
    volatile uint8_t *band_d9d4;
    volatile uint32_t *stage_clock_d9d8;
    volatile uint32_t *timebase_d9dc;
    volatile uint8_t *divider_d9e8;
    volatile uint8_t *divider_d9ec;
    volatile uint8_t *rate_d9f0;
    volatile uint16_t *divider_d9f4;
    volatile uint8_t *divider_d9f8;
    volatile uint32_t *intermediate_da04;
} H9MainClockTiming;

void h9_main_clock_timing_bind_target(H9MainClockTiming *timing);

/* Computes all outputs before mutating target RAM. A zero divisor or a
 * divider outside the representable stock byte range is rejected. */
bool h9_main_clock_timing_calculate(H9MainClockTiming *timing,
                                    uint32_t source_hz,
                                    uint32_t target_hz);

#endif
