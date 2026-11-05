#include "h9_main_core_samples.h"

#include <stddef.h>

static void target_initialize(void *context)
{
    typedef void (*RomConfigure)(uint32_t, uint32_t, uint32_t, uint32_t);
    RomConfigure configure = (RomConfigure)(uintptr_t)0x020000E2u;
    (void)context;
    configure(4u, 4u, 1u, 0u);
    configure(4u, 0u, 1u, 1u);
}

void h9_main_core_samples_bind_target(H9MainCoreSamples *samples,
                                       uint32_t poll_limit)
{
    if (samples == NULL) return;
    samples->adc_1e3100 = (volatile uint32_t *)(uintptr_t)0x001E3100u;
    samples->samples_12338 = (volatile uint16_t *)(uintptr_t)0x00012338u;
    samples->average_11b96 = (volatile uint16_t *)(uintptr_t)0x00011B96u;
    samples->ready_11b98 = (volatile uint16_t *)(uintptr_t)0x00011B98u;
    samples->context = NULL;
    samples->initialize = target_initialize;
    samples->select_mode = NULL; /* app+0x04AAA0 is not ported yet */
    samples->poll_limit = poll_limit;
}

static bool take_sample(H9MainCoreSamples *samples, uint16_t *value)
{
    uint32_t poll;
    for (poll = 0u; poll < samples->poll_limit; ++poll) {
        /* Vendor Pi32 objdump decodes stock 50 E8 FD 39 as wait while
         * bit 7 is clear. Ghidra's 0x1C mask is inconsistent with bytes. */
        if ((samples->adc_1e3100[0] & 0x80u) != 0u) break;
    }
    if (poll == samples->poll_limit) return false;
    *value = (uint16_t)samples->adc_1e3100[1];
    return true;
}

static void acknowledge_sample(volatile uint32_t *control)
{
#if defined(__PI32V2__)
    uint32_t mask = 0x40u;
    __asm__ volatile ("[%0+0] |= %1" ::
                      "r"(control), "r"(mask) : "memory");
#else
    *control |= 0x40u;
#endif
}

bool h9_main_core_samples_run(H9MainCoreSamples *samples)
{
    unsigned index;
    uint16_t value, sum = 0u;
    if (samples == NULL || samples->adc_1e3100 == NULL ||
        samples->samples_12338 == NULL || samples->average_11b96 == NULL ||
        samples->ready_11b98 == NULL || samples->initialize == NULL ||
        samples->select_mode == NULL || samples->poll_limit == 0u)
        return false;

    samples->adc_1e3100[0] = 0u;
    samples->adc_1e3100[0] = 0u;
    samples->initialize(samples->context);
    samples->select_mode(samples->context, 0x50000Fu);
    for (index = 0u; index < H9_MAIN_CORE_SAMPLE_COUNT; ++index) {
        if (!take_sample(samples, &value)) return false;
        samples->samples_12338[index] = value;
        acknowledge_sample(samples->adc_1e3100);
    }
    *samples->average_11b96 = 0u;
    samples->select_mode(samples->context, 0xFu);
    for (index = 0u; index < H9_MAIN_CORE_AVERAGE_COUNT; ++index) {
        if (!take_sample(samples, &value)) return false;
        sum = (uint16_t)(sum + value);
        *samples->average_11b96 = sum;
        acknowledge_sample(samples->adc_1e3100);
    }
    *samples->average_11b96 = (uint16_t)(sum / 10u);
    *samples->ready_11b98 = 1u;
    return true;
}
