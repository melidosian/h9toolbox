#ifndef H9_MAIN_IRQ_REGISTRATION_H
#define H9_MAIN_IRQ_REGISTRATION_H

#include <stdbool.h>
#include <stdint.h>

/* Stock app+0x02465A..0x0246B0 uses the 64-byte mapping table at
 * app+0x0A2A22. The caller supplies the interrupt function address. */
typedef struct {
    void *context;
    volatile uint32_t *vector_words_2bf00;
    volatile uint32_t *control_words_10f100;
    void (*enter_02001634)(void *context, uint32_t bank,
                           uint32_t priority, uint32_t bit_shift);
    void (*leave_02001650)(void *context, uint32_t control_value,
                           uint32_t field, uint32_t bit_shift);
} H9MainIrqRegistration;

bool h9_main_irq_register(const H9MainIrqRegistration *registration,
                          uint32_t irq, uint32_t priority,
                          void (*handler)(void));
void h9_main_irq_registration_bind_target(H9MainIrqRegistration *registration);

#endif
