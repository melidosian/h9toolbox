#include "h9_main_serial_bus.h"

#include <stddef.h>

void h9_main_serial_bus_bind(H9MainSerialBus *bus,
                              volatile uint32_t *registers,
                              uint32_t poll_limit)
{
    if (bus == NULL) return;
    bus->registers = registers;
    bus->poll_limit = poll_limit;
}

void h9_main_serial_bus_bind_target(H9MainSerialBus *bus,
                                     uint32_t poll_limit)
{
    h9_main_serial_bus_bind(bus,
        (volatile uint32_t *)(uintptr_t)0x001E3E08u, poll_limit);
}

#if defined(__PI32V2__)
__attribute__((section(".ram_code.serial_bus"), noinline))
#endif
bool h9_main_serial_bus_transfer(H9MainSerialBus *bus,
                                  uint32_t transmit_word,
                                  uint8_t *received_byte)
{
    volatile uint32_t *registers;
    uint32_t poll;
    if (bus == NULL || bus->registers == NULL ||
        bus->poll_limit == 0u || received_byte == NULL) return false;
    registers = bus->registers;
    registers[1] = transmit_word;
    registers[0] |= 0x10u;
    for (poll = 0u; poll < bus->poll_limit; ++poll) {
        if ((registers[0] & 2u) == 0u) {
            *received_byte = (uint8_t)registers[1];
            return true;
        }
    }
    return false;
}

#if defined(__PI32V2__)
__attribute__((section(".ram_code.serial_bus_adapter"), noinline))
#endif
bool h9_main_serial_bus_exchange(void *context, uint32_t transmit_word,
                                  uint8_t *received_byte)
{
    return h9_main_serial_bus_transfer((H9MainSerialBus *)context,
                                       transmit_word, received_byte);
}
