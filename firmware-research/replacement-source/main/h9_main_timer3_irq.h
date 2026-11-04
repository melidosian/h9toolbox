#ifndef H9_MAIN_TIMER3_IRQ_H
#define H9_MAIN_TIMER3_IRQ_H

#include <stdbool.h>
#include <stdint.h>

/* Stock handler app+0x03126C..0x0312D2, excluding its Pi32 interrupt
 * save/restore wrapper. ROM calls are injected until a target ISR wrapper
 * and interrupt registration can be linked and checked. */
typedef struct {
    void *context;
    volatile uint32_t *tick_count_c0a4;
    volatile uint32_t *timer_control_10f0ec;
    volatile uint32_t *clock_count_1d8b0;
    volatile uint32_t *compare_d5e8;
    volatile uint32_t *compare_d5bc;
    uint32_t (*poll_02001d2e)(void *context);
    void (*clock_enter_02001634)(void *context);
    void (*clock_sync)(void *context);
    uint32_t (*check_02001e2c)(void *context);
    void (*clock_leave_02001650)(void *context);
    void (*fault_swi0)(void *context);
} H9MainTimer3Irq;

bool h9_main_timer3_irq_handle(const H9MainTimer3Irq *irq);
void h9_main_timer3_isr(void);

#endif
