#include "h9_aprs_wave_output.h"

#include <stddef.h>

#define H9_APRS_GPIO_A_BASE 0x50000000u
#define H9_APRS_GPIO_ODR_WORD (0x14u / 4u)

void h9_aprs_wave_output_bind(H9AprsWaveOutput *output,
                               volatile uint32_t *gpio_a)
{
    if (output != NULL) output->gpio_a = gpio_a;
}

void h9_aprs_wave_output_bind_target(H9AprsWaveOutput *output)
{
    h9_aprs_wave_output_bind(output,
        (volatile uint32_t *)(uintptr_t)H9_APRS_GPIO_A_BASE);
}

uint8_t h9_aprs_wave_output_read(void *context)
{
    H9AprsWaveOutput *output = (H9AprsWaveOutput *)context;
    if (output == NULL || output->gpio_a == NULL) return 0u;
    return (uint8_t)output->gpio_a[H9_APRS_GPIO_ODR_WORD];
}

void h9_aprs_wave_output_write(void *context, uint8_t value)
{
    H9AprsWaveOutput *output = (H9AprsWaveOutput *)context;
    if (output != NULL && output->gpio_a != NULL)
        output->gpio_a[H9_APRS_GPIO_ODR_WORD] = value;
}
