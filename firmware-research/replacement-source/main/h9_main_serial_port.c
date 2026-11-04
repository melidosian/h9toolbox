#include "h9_main_serial_port.h"

#include <stddef.h>

#if defined(__PI32V2__)
#define H9_MAIN_RAM_CODE __attribute__((section(".ram_code.serial_port"), noinline))
#else
#define H9_MAIN_RAM_CODE
#endif

H9_MAIN_RAM_CODE
static bool port_begin(void *context)
{
    H9MainSerialPort *port = (H9MainSerialPort *)context;
    return h9_main_serial_gate_acquire(&port->gate);
}

H9_MAIN_RAM_CODE
static bool port_end(void *context)
{
    H9MainSerialPort *port = (H9MainSerialPort *)context;
    return h9_main_serial_gate_release(&port->gate);
}

H9_MAIN_RAM_CODE
static bool port_exchange(void *context, uint32_t word, uint8_t *received)
{
    H9MainSerialPort *port = (H9MainSerialPort *)context;
    return h9_main_serial_bus_transfer(&port->bus, word, received);
}

void h9_main_serial_port_bind(H9MainSerialPort *port,
                               volatile uint32_t *bus_registers,
                               volatile uint32_t *gate_control,
                               volatile uint32_t *gate_users,
                               uint32_t poll_limit)
{
    if (port == NULL) return;
    h9_main_serial_bus_bind(&port->bus, bus_registers, poll_limit);
    h9_main_serial_gate_bind(&port->gate, gate_control, gate_users);
    port->commands.control = bus_registers;
    port->commands.begin = port_begin;
    port->commands.end = port_end;
    port->commands.exchange = port_exchange;
    port->commands.context = port;
}

void h9_main_serial_port_bind_target(H9MainSerialPort *port,
                                      uint32_t poll_limit)
{
    if (port == NULL) return;
    h9_main_serial_port_bind(port,
        (volatile uint32_t *)(uintptr_t)0x001E3E08u,
        (volatile uint32_t *)(uintptr_t)0x0010F1A8u,
        (volatile uint32_t *)(uintptr_t)0x0001176Cu,
        poll_limit);
}
