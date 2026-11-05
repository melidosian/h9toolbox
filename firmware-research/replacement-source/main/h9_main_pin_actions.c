#include "h9_main_pin_actions.h"

#include <stddef.h>

static int supports_pin(void *context, uint32_t pin)
{
    H9MainPinActions *actions = (H9MainPinActions *)context;
    return actions != NULL && actions->sync_1e0200 != NULL &&
        pin < 56u && pin != 38u && actions->banks[pin >> 4u] != NULL;
}

static void clear_bank_bit(volatile uint32_t *word, uint32_t mask)
{
#if defined(__PI32V2__)
    uint32_t keep = ~mask;
    __asm__ volatile ("[%0+0] &= %1" ::
                      "r"(word), "r"(keep) : "memory");
#else
    *word &= ~mask;
#endif
}

static void set_bank_bit(volatile uint32_t *word, uint32_t mask)
{
#if defined(__PI32V2__)
    __asm__ volatile ("[%0+0] |= %1" ::
                      "r"(word), "r"(mask) : "memory");
#else
    *word |= mask;
#endif
}

static volatile uint32_t *pin_word(H9MainPinActions *actions,
                                    uint32_t pin, unsigned word_index)
{
    return actions->banks[pin >> 4u] + word_index;
}

static void action_0176(void *context, uint32_t pin, uint32_t value)
{
    H9MainPinActions *actions = (H9MainPinActions *)context;
    uint32_t mask = 1u << (pin & 15u);
    (void)value;
    (void)*actions->sync_1e0200;
    clear_bank_bit(pin_word(actions, pin, 3u), mask);
}

static void action_01f6(void *context, uint32_t pin, uint32_t value)
{
    H9MainPinActions *actions = (H9MainPinActions *)context;
    uint32_t mask = 1u << (pin & 15u);
    (void)*actions->sync_1e0200;
    if (value) set_bank_bit(pin_word(actions, pin, 2u), mask);
    else clear_bank_bit(pin_word(actions, pin, 2u), mask);
}

static void action_0284(void *context, uint32_t pin, uint32_t value)
{
    H9MainPinActions *actions = (H9MainPinActions *)context;
    uint32_t mask = 1u << (pin & 15u);
    if (value) set_bank_bit(pin_word(actions, pin, 4u), mask);
    else clear_bank_bit(pin_word(actions, pin, 4u), mask);
}

static void action_02dc(void *context, uint32_t pin, uint32_t value)
{
    H9MainPinActions *actions = (H9MainPinActions *)context;
    uint32_t mask = 1u << (pin & 15u);
    if (value) set_bank_bit(pin_word(actions, pin, 5u), mask);
    else clear_bank_bit(pin_word(actions, pin, 5u), mask);
}

void h9_main_pin_actions_bind_target(H9MainPinActions *actions)
{
    unsigned index;
    if (actions == NULL) return;
    for (index = 0u; index < 4u; ++index)
        actions->banks[index] =
            (volatile uint32_t *)(uintptr_t)(0x001E5000u + 0x40u * index);
    actions->sync_1e0200 =
        (volatile uint32_t *)(uintptr_t)0x001E0200u;
}

void h9_main_pin_actions_attach(H9MainPinRegistry *registry,
                                 H9MainPinActions *actions)
{
    if (registry == NULL || actions == NULL) return;
    registry->context = actions;
    registry->action_0176 = action_0176;
    registry->action_01f6 = action_01f6;
    registry->action_0284 = action_0284;
    registry->action_02dc = action_02dc;
    registry->supports_pin = supports_pin;
}
