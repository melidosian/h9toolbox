#ifndef H9_APRS_SAMPLE_TIMER_H
#define H9_APRS_SAMPLE_TIMER_H

#include <stdbool.h>
#include <stdint.h>

/* Stock APRS 0x08002824 configures timer 0x40012C00 before ADC setup.
 * Its connection to ADC sampling is inferred, not yet board-verified. */
typedef struct {
    volatile uint32_t *rcc;
    volatile uint32_t *timer;
} H9AprsSampleTimer;

void h9_aprs_sample_timer_bind(H9AprsSampleTimer *sample_timer,
                                volatile uint32_t *rcc,
                                volatile uint32_t *timer);
void h9_aprs_sample_timer_bind_target(H9AprsSampleTimer *sample_timer);
bool h9_aprs_sample_timer_start(H9AprsSampleTimer *sample_timer);

#endif
