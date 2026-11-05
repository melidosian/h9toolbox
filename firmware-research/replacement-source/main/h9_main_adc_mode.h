#ifndef H9_MAIN_ADC_MODE_H
#define H9_MAIN_ADC_MODE_H

#include <stdbool.h>
#include <stdint.h>

/* The stock selector at app+0x04AAA0 has several branches. These are the
 * two modes used by app_core's 16+10 sample capture; both take case zero. */
typedef struct {
    volatile uint16_t *ready_11b98;
    volatile uint32_t *register_1e19ec;
    volatile uint32_t *register_1e0044;
    volatile uint32_t *register_1e1964;
    volatile uint32_t *register_1e1910;
    volatile uint32_t *adc_1e3100;
    void *context;
    void (*rom_configure)(void *context, uint32_t group,
                          uint32_t selector, uint32_t mask,
                          uint32_t value);
} H9MainAdcMode;

void h9_main_adc_mode_bind_target(H9MainAdcMode *selection);
bool h9_main_adc_mode_select(H9MainAdcMode *selection, uint32_t mode,
                              uint16_t *previous_ready);

#endif
