#include "h9_main_timer3_irq.h"

#include <stddef.h>

#if !defined(__PI32V2__)
/* Host tests link the binder; the real interrupt shell is Pi32 assembly. */
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

static uint32_t target_poll(void *context)
{
    typedef uint32_t (*RomPoll)(void);
    (void)context;
    return ((RomPoll)(uintptr_t)0x02001D2Eu)();
}

static void target_clock_enter(void *context)
{
    typedef void (*RomClock)(void);
    (void)context;
    ((RomClock)(uintptr_t)0x02001634u)();
}

static void target_clock_sync(void *context)
{
    (void)context;
#if defined(__PI32V2__)
    __asm__ volatile("csync" ::: "memory");
#endif
}

static uint32_t target_clock_check(void *context)
{
    typedef uint32_t (*RomCheck)(void);
    (void)context;
    return ((RomCheck)(uintptr_t)0x02001E2Cu)();
}

static void target_clock_leave(void *context)
{
    typedef void (*RomClock)(void);
    (void)context;
    ((RomClock)(uintptr_t)0x02001650u)();
}

static void target_fault(void *context)
{
    /* The target body issues the stock SWI after this callback returns. */
    (void)context;
}

void h9_main_timer3_irq_bind_target(H9MainTimer3Irq *irq)
{
    if (irq == NULL) return;
    irq->context = NULL;
    irq->tick_count_c0a4 = (volatile uint32_t *)(uintptr_t)0x0000C0A4u;
    irq->timer_control_10f0ec =
        (volatile uint32_t *)(uintptr_t)0x0010F0ECu;
    irq->clock_count_1d8b0 =
        (volatile uint32_t *)(uintptr_t)0x0001D8B0u;
    irq->compare_d5e8 = (volatile uint32_t *)(uintptr_t)0x0000D5E8u;
    irq->compare_d5bc = (volatile uint32_t *)(uintptr_t)0x0000D5BCu;
    irq->poll_02001d2e = target_poll;
    irq->clock_enter_02001634 = target_clock_enter;
    irq->clock_sync = target_clock_sync;
    irq->check_02001e2c = target_clock_check;
    irq->clock_leave_02001650 = target_clock_leave;
    irq->fault_swi0 = target_fault;
}

void h9_main_timer3_irq_target_body(void)
{
    H9MainTimer3Irq irq;
    h9_main_timer3_irq_bind_target(&irq);
    if (!h9_main_timer3_irq_handle(&irq)) {
#if defined(__PI32V2__)
        __asm__ volatile("swi 0" ::: "memory");
#endif
        for (;;) { }
    }
}
