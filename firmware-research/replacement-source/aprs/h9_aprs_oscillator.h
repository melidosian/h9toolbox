#ifndef H9_APRS_OSCILLATOR_H
#define H9_APRS_OSCILLATOR_H

#include <stdbool.h>
#include <stdint.h>

/* The waveform tail of 0x08002468 uses a 9-bit phase accumulator and
   selects 0x40 or 0x75 phase units per ADC callback. */
typedef struct {
    uint16_t phase;
    uint16_t step;
} H9AprsOscillator;

void h9_aprs_oscillator_init(H9AprsOscillator *oscillator);
bool h9_aprs_oscillator_set_step(H9AprsOscillator *oscillator, uint16_t step);
uint8_t h9_aprs_oscillator_next(H9AprsOscillator *oscillator);

#endif
