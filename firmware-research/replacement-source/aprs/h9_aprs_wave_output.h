#ifndef H9_APRS_WAVE_OUTPUT_H
#define H9_APRS_WAVE_OUTPUT_H

#include <stdint.h>

/* The stock sample callback writes the reversed waveform nibble to
 * GPIOA ODR at 0x50000014. The controller owns the nibble conversion. */
typedef struct {
    volatile uint32_t *rcc;
    volatile uint32_t *gpio_a;
} H9AprsWaveOutput;

void h9_aprs_wave_output_bind(H9AprsWaveOutput *output,
                               volatile uint32_t *rcc,
                               volatile uint32_t *gpio_a);
void h9_aprs_wave_output_bind_target(H9AprsWaveOutput *output);
/* Stock 0x080022C0 configures GPIOA pins 4..7 for the waveform nibble. */
void h9_aprs_wave_output_configure(H9AprsWaveOutput *output);
uint8_t h9_aprs_wave_output_read(void *context);
void h9_aprs_wave_output_write(void *context, uint8_t value);

#endif
