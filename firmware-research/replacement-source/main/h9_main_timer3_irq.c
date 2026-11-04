#include "h9_main_timer3_irq.h"

#include <stddef.h>

#if !defined(__PI32V2__)
/* Host tests link the timer binder; the target ISR is direct Pi32 assembly. */
void h9_main_timer3_isr(void) { }
#endif

bool h9_main_timer3_irq_handle(const H9MainTimer3Irq *irq)
{
    if (irq == NULL || irq->tick_count_c0a4 == NULL ||
        irq->timer_control_10f0ec == NULL ||
        irq->clock_count_1d8b0 == NULL ||
        irq->compare_d5e8 == NULL || irq->compare_d5bc == NULL ||
        irq->poll_02001d2e == NULL || irq->clock_enter_02001634 == NULL ||
        irq->clock_sync == NULL || irq->check_02001e2c == NULL ||
        irq->clock_leave_02001650 == NULL ||
        irq->fault_swi0 == NULL) return false;

    ++*irq->tick_count_c0a4;
    *irq->timer_control_10f0ec |= 0x40u;
    ++*irq->clock_count_1d8b0;
    if (irq->poll_02001d2e(irq->context) == 0u) return true;
    irq->clock_enter_02001634(irq->context);
    irq->clock_sync(irq->context);
    if (irq->check_02001e2c(irq->context) != 0u) {
        if (*irq->compare_d5e8 != *irq->compare_d5bc) {
            irq->fault_swi0(irq->context);
            return false;
        }
    }
    irq->clock_leave_02001650(irq->context);
    return true;
}
