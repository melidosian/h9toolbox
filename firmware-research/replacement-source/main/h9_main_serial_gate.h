#ifndef H9_MAIN_SERIAL_GATE_H
#define H9_MAIN_SERIAL_GATE_H

#include <stdbool.h>
#include <stdint.h>

/* Stock RAM 0x1634/0x1650 protects the serial transaction with a
 * register write and a RAM reference count. The peripheral's external
 * electrical role has not been identified. */
typedef struct {
    volatile uint32_t *control;
    volatile uint32_t *users;
} H9MainSerialGate;

void h9_main_serial_gate_bind(H9MainSerialGate *gate,
                               volatile uint32_t *control,
                               volatile uint32_t *users);
void h9_main_serial_gate_bind_target(H9MainSerialGate *gate);
bool h9_main_serial_gate_acquire(H9MainSerialGate *gate);
bool h9_main_serial_gate_release(H9MainSerialGate *gate);

#endif
