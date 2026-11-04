#include "h9_aprs_wave_output.h"

#include <stddef.h>

#define H9_APRS_RCC_BASE 0x40021000u
#define H9_APRS_GPIO_A_BASE 0x50000000u
#define H9_APRS_GPIO_ODR_WORD (0x14u / 4u)
#define H9_APRS_RCC_GPIO_ENABLE_WORD (0x34u / 4u)
#define H9_APRS_GPIO_MODER_WORD 0u
#define H9_APRS_GPIO_OTYPER_WORD 1u
#define H9_APRS_GPIO_OSPEEDR_WORD 2u
#define H9_APRS_GPIO_PUPDR_WORD 3u
#define H9_APRS_GPIO_CLEAR_WORD (0x28u / 4u)

void h9_aprs_wave_output_bind(H9AprsWaveOutput *output,
                               volatile uint32_t *rcc,
                               volatile uint32_t *gpio_a)
{
    if (output == NULL) return;
    output->rcc = rcc;
    output->gpio_a = gpio_a;
}

void h9_aprs_wave_output_bind_target(H9AprsWaveOutput *output)
{
    h9_aprs_wave_output_bind(output,
        (volatile uint32_t *)(uintptr_t)H9_APRS_RCC_BASE,
        (volatile uint32_t *)(uintptr_t)H9_APRS_GPIO_A_BASE);
}

void h9_aprs_wave_output_configure(H9AprsWaveOutput *output)
{
    unsigned pin;
    if (output == NULL || output->rcc == NULL || output->gpio_a == NULL)
        return;
    output->rcc[H9_APRS_RCC_GPIO_ENABLE_WORD] |= 1u;
    /* Stock 0x080056EC also configures PA0 as a low-speed push-pull
     * output with no pull, then clears it. Its external role is unknown. */
    output->gpio_a[H9_APRS_GPIO_MODER_WORD] =
        (output->gpio_a[H9_APRS_GPIO_MODER_WORD] & ~3u) | 1u;
    output->gpio_a[H9_APRS_GPIO_OTYPER_WORD] &= ~1u;
    output->gpio_a[H9_APRS_GPIO_OSPEEDR_WORD] &= ~3u;
    output->gpio_a[H9_APRS_GPIO_PUPDR_WORD] &= ~3u;
    output->gpio_a[H9_APRS_GPIO_CLEAR_WORD] = 1u;
    for (pin = 4u; pin <= 7u; ++pin) {
        uint32_t pair_mask = 3u << (pin * 2u);
        output->gpio_a[H9_APRS_GPIO_MODER_WORD] =
            (output->gpio_a[H9_APRS_GPIO_MODER_WORD] & ~pair_mask) |
            (1u << (pin * 2u));
        output->gpio_a[H9_APRS_GPIO_OTYPER_WORD] &= ~(1u << pin);
        output->gpio_a[H9_APRS_GPIO_OSPEEDR_WORD] =
            (output->gpio_a[H9_APRS_GPIO_OSPEEDR_WORD] & ~pair_mask) |
            (3u << (pin * 2u));
        output->gpio_a[H9_APRS_GPIO_PUPDR_WORD] =
            (output->gpio_a[H9_APRS_GPIO_PUPDR_WORD] & ~pair_mask) |
            (1u << (pin * 2u));
    }
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
