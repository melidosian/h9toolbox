#ifndef H9_APRS_SYSTICK_H
#define H9_APRS_SYSTICK_H

#include <stdbool.h>
#include <stdint.h>

/* Stock 0x08003890 reads a 32-bit tick; the vector-15 handler at
 * 0x08004984 adds the configured step. Register setup follows 0x08003EE8. */
typedef struct {
    volatile uint32_t *registers;
    volatile uint32_t ticks;
    uint32_t step_ms;
} H9AprsSysTick;

void h9_aprs_systick_bind(H9AprsSysTick *timer,
                           volatile uint32_t *registers);
void h9_aprs_systick_bind_target(H9AprsSysTick *timer);
bool h9_aprs_systick_start(H9AprsSysTick *timer,
                            uint32_t system_clock_hz, uint32_t step_ms);
void h9_aprs_systick_irq(H9AprsSysTick *timer);
uint32_t h9_aprs_systick_now(const H9AprsSysTick *timer);

#endif
