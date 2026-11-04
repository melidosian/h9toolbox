#include "h9_main_rf_output.h"

#include <stddef.h>

static void update_bits(volatile uint32_t *register_address,
                        uint32_t mask, bool enabled)
{
#if defined(__PI32V2__)
    /* Stock uses the Pi32 memory bit operations rather than a C load/store
     * pair. Keep that single-instruction update when compiled for the radio. */
    if (enabled)
        __asm__ volatile ("[%0+0] |= %1" ::
                          "r"(register_address), "r"(mask) : "memory");
    else {
        uint32_t keep = ~mask;
        __asm__ volatile ("[%0+0] &= %1" ::
                          "r"(register_address), "r"(keep) : "memory");
    }
#else
    uint32_t value = *register_address;
    if (enabled) value |= mask;
    else value &= ~mask;
    *register_address = value;
#endif
}

void h9_main_rf_output_bind_target(H9MainRfOutput *output)
{
    if (output == NULL) return;
    output->gpio_data = (volatile uint32_t *)(uintptr_t)0x001E5000u;
    output->sync_register = (volatile uint32_t *)(uintptr_t)0x001E0200u;
    output->pwm_registers = (volatile uint32_t *)(uintptr_t)0x001E0600u;
}

bool h9_main_rf_output_set_gpio(H9MainRfOutput *output,
                                H9MainRfGpioLine line, bool enabled)
{
    if (output == NULL || output->gpio_data == NULL ||
        output->sync_register == NULL ||
        (line != H9_MAIN_RF_GPIO_BIT0 && line != H9_MAIN_RF_GPIO_BIT8))
        return false;
    /* The stock routine reads 0x001E0200 immediately before the GPIO RMW.
     * Its peripheral side effect is unknown. */
    (void)*output->sync_register;
    update_bits(output->gpio_data, (uint32_t)line, enabled);
    return true;
}

bool h9_main_rf_output_set_pwm(H9MainRfOutput *output, bool enabled,
                               uint8_t duty)
{
    uint32_t period;
    if (output == NULL || output->pwm_registers == NULL) return false;
    period = output->pwm_registers[2];
    output->pwm_registers[3] = ((uint32_t)duty * period) / 255u;
    update_bits(output->pwm_registers, 0x100u, enabled);
    return true;
}
