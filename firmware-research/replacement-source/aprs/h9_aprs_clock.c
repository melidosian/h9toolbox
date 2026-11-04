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

uint32_t h9_aprs_clock_system_hz(uint32_t rcc_control,
                                 uint32_t rcc_trim,
                                 uint32_t rcc_select)
{
    static const uint32_t source_hz[5] = {
        4000000u, 8000000u, 16000000u, 22120000u, 24000000u
    };
    uint32_t source = rcc_select & 0x38u;
    uint32_t index;
    if (source == 0x08u) return 24000000u;
    if (source == 0x18u) return 32768u;
    if (source != 0u) return 0;
    index = (rcc_trim >> 13) & 7u;
    if (index > 4u) index = 0;
    return source_hz[index] >> ((rcc_control >> 11) & 7u);
}

uint32_t h9_aprs_clock_select_internal(volatile uint32_t *rcc,
                                       uint16_t factory,
                                       uint32_t poll_limit)
{
    uint32_t poll;
    if (rcc == 0 || poll_limit == 0u) return 0;
    /* Stock 0x08003BF8 selects trim index 4 (24 MHz) and waits for
     * RCC control bit 10 before switching to source 0. */
    rcc[1] = (rcc[1] & 0xFFFF0000u) | 0x8000u |
             ((uint32_t)factory & 0x1FFFu);
    for (poll = 0; poll < poll_limit; ++poll)
        if ((rcc[0] & 0x400u) != 0u) break;
    if (poll == poll_limit) return 0;
    rcc[0] &= ~0x3800u;
    /* Stock 0x080039A0 selects source 0 and zero bus prescalers. */
    rcc[2] &= ~(0x7u | 0xF00u | 0x7000u);
    for (poll = 0; poll < poll_limit; ++poll)
        if ((rcc[2] & 0x38u) == 0u) break;
    if (poll == poll_limit) return 0;
    return h9_aprs_clock_system_hz(rcc[0], rcc[1], rcc[2]);
}

uint32_t h9_aprs_clock_select_internal_target(uint32_t poll_limit)
{
    volatile uint32_t *rcc = (volatile uint32_t *)(uintptr_t)0x40021000u;
    return h9_aprs_clock_select_internal(rcc, H9_APRS_FACTORY_HALFWORD,
                                         poll_limit);
}
