#ifndef H9_MAIN_SERIAL_PORT_H
#define H9_MAIN_SERIAL_PORT_H

#include "h9_main_serial_bus.h"
#include "h9_main_serial_commands.h"
#include "h9_main_serial_gate.h"

/* Source-level wiring of the observed RAM transaction path. No production
 * board entry calls this yet. */
typedef struct {
    H9MainSerialBus bus;
    H9MainSerialGate gate;
    H9MainSerialCommands commands;
} H9MainSerialPort;

void h9_main_serial_port_bind(H9MainSerialPort *port,
                               volatile uint32_t *bus_registers,
                               volatile uint32_t *gate_control,
                               volatile uint32_t *gate_users,
                               uint32_t poll_limit);
void h9_main_serial_port_bind_target(H9MainSerialPort *port,
                                      uint32_t poll_limit);

#endif
