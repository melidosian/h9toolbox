#include "h9_aprs_sample_timer.h"

#include <stddef.h>

#define H9_APRS_RCC_BASE 0x40021000u
#define H9_APRS_TIMER_BASE 0x40012C00u
#define H9_APRS_RCC_TIMER_ENABLE_WORD (0x40u / 4u)
#define H9_APRS_TIMER_CR1 0u
#define H9_APRS_TIMER_CR2 1u
#define H9_APRS_TIMER_SMCR 2u
#define H9_APRS_TIMER_EGR 5u
#define H9_APRS_TIMER_PSC 10u
#define H9_APRS_TIMER_ARR 11u
#define H9_APRS_TIMER_RCR 12u

void h9_aprs_sample_timer_bind(H9AprsSampleTimer *sample_timer,
                                volatile uint32_t *rcc,
                                volatile uint32_t *timer)
{
    if (sample_timer == NULL) return;
    sample_timer->rcc = rcc;
    sample_timer->timer = timer;
}

void h9_aprs_sample_timer_bind_target(H9AprsSampleTimer *sample_timer)
{
    h9_aprs_sample_timer_bind(sample_timer,
        (volatile uint32_t *)(uintptr_t)H9_APRS_RCC_BASE,
        (volatile uint32_t *)(uintptr_t)H9_APRS_TIMER_BASE);
}

bool h9_aprs_sample_timer_start(H9AprsSampleTimer *sample_timer)
{
    volatile uint32_t *timer;
    if (sample_timer == NULL || sample_timer->rcc == NULL ||
        sample_timer->timer == NULL) return false;
    timer = sample_timer->timer;
    sample_timer->rcc[H9_APRS_RCC_TIMER_ENABLE_WORD] |= 0x800u;
    /* 0x080049C0 with the state built at 0x08002824. */
    timer[H9_APRS_TIMER_CR1] = (timer[H9_APRS_TIMER_CR1] & ~0x3F0u) | 0x80u;
    timer[H9_APRS_TIMER_ARR] = 0xD1u;
    timer[H9_APRS_TIMER_PSC] = 0xBu;
    timer[H9_APRS_TIMER_RCR] = 0u;
    timer[H9_APRS_TIMER_EGR] = 1u;
    /* 0x08003F20 writes the observed CR2 trigger field. */
    timer[H9_APRS_TIMER_CR2] = (timer[H9_APRS_TIMER_CR2] & ~0x70u) | 0x20u;
    timer[H9_APRS_TIMER_SMCR] &= ~0x80u;
    /* 0x08003FD2 starts the timer. */
    timer[H9_APRS_TIMER_CR1] |= 1u;
    return true;
}
