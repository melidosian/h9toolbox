#ifndef H9_MAIN_CORE_POST_SAMPLES_H
#define H9_MAIN_CORE_POST_SAMPLES_H

#include <stdbool.h>
#include <stdint.h>

/* Stock app+0x060E16..0x060E96, after the 16+10 ADC samples. Calls into
 * unported ROM/application services are explicit; the caller must supply
 * implementations before this can run on the replacement board. */
typedef uint32_t (*H9MainCorePostInvoke)(void *context, uint32_t target,
                                          uint32_t r0, uint32_t r1,
                                          uint32_t r2, uint32_t r3);

typedef struct {
    volatile uint32_t *module_11bc0;
    volatile uint32_t *list_12f38;
    volatile uint8_t *state_12188;
    H9MainCorePostInvoke invoke;
    void *context;
} H9MainCorePostSamples;

/* Returns false without side effects when a required binding is absent.
 * Target addresses identify the stock behavior to replace, not callable
 * functions in the diagnostic image. */
bool h9_main_core_post_samples_run(const H9MainCorePostSamples *post);

#endif
