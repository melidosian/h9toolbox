#include "h9_aprs_clock.h"

#include <stdint.h>

#define H9_APRS_RCC_CFGR (*(volatile uint32_t *)(uintptr_t)0x40021004u)
#define H9_APRS_FACTORY_HALFWORD (*(volatile uint16_t *)(uintptr_t)0x1FFF0F04u)
#define H9_APRS_VTOR (*(volatile uint32_t *)(uintptr_t)0xE000ED08u)

uint32_t h9_aprs_clock_cfgr_value(uint32_t previous, uint16_t factory)
{
    return (previous & 0xFFFF0000u) | 0x2000u | (factory & 0x1FFFu);
}

void h9_aprs_clock_init(void)
{
    H9_APRS_RCC_CFGR = h9_aprs_clock_cfgr_value(H9_APRS_RCC_CFGR,
                                                H9_APRS_FACTORY_HALFWORD);
    H9_APRS_VTOR = 0x08002000u;
}
