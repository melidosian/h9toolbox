#ifndef H9_MAIN_CORE_SAMPLES_H
#define H9_MAIN_CORE_SAMPLES_H

#include <stdbool.h>
#include <stdint.h>

enum { H9_MAIN_CORE_SAMPLE_COUNT = 16,
       H9_MAIN_CORE_AVERAGE_COUNT = 10 };

/* Stock app+0x060CAC..0x060D16, inside the direct app_core setup call.
 * The ADC mode selector at app+0x04AAA0 is still unported and is injected. */
typedef struct {
    volatile uint32_t *adc_1e3100;
    volatile uint16_t *samples_12338;
    volatile uint16_t *average_11b96;
    volatile uint16_t *ready_11b98;
    void *context;
    void (*initialize)(void *context); /* stock app+0x04AA78 */
    bool (*select_mode)(void *context, uint32_t mode); /* app+0x04AAA0 */
    uint32_t poll_limit;
} H9MainCoreSamples;

void h9_main_core_samples_bind_target(H9MainCoreSamples *samples,
                                       uint32_t poll_limit);
bool h9_main_core_samples_run(H9MainCoreSamples *samples);

#endif
