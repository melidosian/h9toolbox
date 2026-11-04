#include "h9_main_serial_gate.h"

#include <stddef.h>

#if defined(__PI32V2__)
#define H9_MAIN_RAM_CODE __attribute__((section(".ram_code.serial_gate"), noinline))
#define H9_MAIN_CLI() __asm__ volatile("cli" ::: "memory")
#define H9_MAIN_CSYNC() __asm__ volatile("csync" ::: "memory")
#define H9_MAIN_STI() __asm__ volatile("sti" ::: "memory")
#else
#define H9_MAIN_RAM_CODE
#define H9_MAIN_CLI() ((void)0)
#define H9_MAIN_CSYNC() ((void)0)
#define H9_MAIN_STI() ((void)0)
#endif

void h9_main_serial_gate_bind(H9MainSerialGate *gate,
                               volatile uint32_t *control,
                               volatile uint32_t *users)
{
    if (gate == NULL) return;
    gate->control = control;
    gate->users = users;
}

void h9_main_serial_gate_bind_target(H9MainSerialGate *gate)
{
    h9_main_serial_gate_bind(gate,
        (volatile uint32_t *)(uintptr_t)0x0010F1A8u,
        (volatile uint32_t *)(uintptr_t)0x0001176Cu);
}

H9_MAIN_RAM_CODE
bool h9_main_serial_gate_acquire(H9MainSerialGate *gate)
{
    if (gate == NULL || gate->control == NULL || gate->users == NULL)
        return false;
    H9_MAIN_CLI();
    if (*gate->users == UINT32_MAX) {
        H9_MAIN_STI();
        return false;
    }
    *gate->control = 6u;
    ++*gate->users;
    H9_MAIN_CSYNC();
    H9_MAIN_STI();
    return true;
}

H9_MAIN_RAM_CODE
bool h9_main_serial_gate_release(H9MainSerialGate *gate)
{
    uint32_t users;
    if (gate == NULL || gate->control == NULL || gate->users == NULL ||
        *gate->users == 0u) return false;
    users = *gate->users - 1u;
    *gate->users = users;
    if (users == 0u) *gate->control = 0u;
    return true;
}
