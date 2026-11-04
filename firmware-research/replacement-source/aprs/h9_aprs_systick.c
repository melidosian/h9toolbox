#include "h9_aprs_systick.h"

#include <stddef.h>

enum { SYSTICK_CTRL, SYSTICK_LOAD, SYSTICK_VAL };

void h9_aprs_systick_bind(H9AprsSysTick *timer,
                           volatile uint32_t *registers)
{
    if (timer == NULL) return;
    timer->registers = registers;
    timer->ticks = 0u;
    timer->step_ms = 0u;
}

void h9_aprs_systick_bind_target(H9AprsSysTick *timer)
{
    h9_aprs_systick_bind(timer,
        (volatile uint32_t *)(uintptr_t)0xE000E010u);
}

bool h9_aprs_systick_start(H9AprsSysTick *timer,
                            uint32_t system_clock_hz, uint32_t step_ms)
{
    uint32_t periods_per_second;
    uint32_t cycles_per_period;
    if (timer == NULL || timer->registers == NULL ||
        system_clock_hz == 0u || step_ms == 0u ||
        step_ms > 1000u || 1000u % step_ms != 0u) return false;
    periods_per_second = 1000u / step_ms;
    cycles_per_period = system_clock_hz / periods_per_second;
    if (cycles_per_period == 0u || cycles_per_period > 0x1000000u)
        return false;
    timer->ticks = 0u;
    timer->step_ms = step_ms;
    timer->registers[SYSTICK_LOAD] = cycles_per_period - 1u;
    timer->registers[SYSTICK_VAL] = 0u;
    timer->registers[SYSTICK_CTRL] = 7u;
    return true;
}

void h9_aprs_systick_irq(H9AprsSysTick *timer)
{
    if (timer == NULL) return;
    timer->ticks += timer->step_ms;
}

uint32_t h9_aprs_systick_now(const H9AprsSysTick *timer)
{
    return timer != NULL ? timer->ticks : 0u;
}
