#include "h9_main_irq_registration.h"

#include <stddef.h>

static const uint8_t irq_map[64] = {
    0, 7, 0, 1, 5, 3, 5, 3, 1, 3, 0, 3, 2, 0, 0, 0,
    3, 3, 3, 3, 0, 0, 0, 0, 1, 1, 2, 2, 2, 0, 2, 1,
    1, 1, 3, 3, 3, 0, 2, 2, 0, 0, 0, 0, 0, 1, 0, 4,
    1, 1, 1, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

bool h9_main_irq_register(const H9MainIrqRegistration *registration,
                          uint32_t irq, uint32_t priority,
                          void (*handler)(void))
{
    uint32_t shift, mask, field, value;
    volatile uint32_t *control;
    if (registration == NULL || irq >= 64u || handler == NULL ||
        registration->vector_words_2bf00 == NULL ||
        registration->control_words_10f100 == NULL ||
        registration->enter_02001634 == NULL ||
        registration->leave_02001650 == NULL) return false;

    registration->vector_words_2bf00[irq] = (uint32_t)(uintptr_t)handler;
    shift = (irq & 7u) * 4u;
    mask = 0xFu << shift;
    control = registration->control_words_10f100 + (irq >> 3u);
    registration->enter_02001634(registration->context, irq >> 3u,
                                   priority, shift);
    value = *control & ~mask;
    field = (((uint32_t)irq_map[irq] << 1u) & 0xEu) | 1u;
    field <<= shift;
    value |= field;
    *control = value;
    registration->leave_02001650(registration->context, value,
                                   field, shift);
    return true;
}

static void target_enter(void *context, uint32_t bank,
                         uint32_t priority, uint32_t shift)
{
    typedef void (*RomEnter)(uint32_t, uint32_t, uint32_t);
    (void)context;
    ((RomEnter)(uintptr_t)0x02001634u)(bank, priority, shift);
}

static void target_leave(void *context, uint32_t value,
                         uint32_t field, uint32_t shift)
{
    typedef void (*RomLeave)(uint32_t, uint32_t, uint32_t);
    (void)context;
    ((RomLeave)(uintptr_t)0x02001650u)(value, field, shift);
}

void h9_main_irq_registration_bind_target(H9MainIrqRegistration *registration)
{
    if (registration == NULL) return;
    registration->context = NULL;
    registration->vector_words_2bf00 =
        (volatile uint32_t *)(uintptr_t)0x0002BF00u;
    registration->control_words_10f100 =
        (volatile uint32_t *)(uintptr_t)0x0010F100u;
    registration->enter_02001634 = target_enter;
    registration->leave_02001650 = target_leave;
}
