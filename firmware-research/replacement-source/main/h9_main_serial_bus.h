#ifndef H9_MAIN_SERIAL_BUS_H
#define H9_MAIN_SERIAL_BUS_H

#include <stdbool.h>
#include <stdint.h>

/* Stock RAM address 0x40 performs a synchronous byte exchange through
 * controller registers at 0x001E3E08. The connected device is not proven. */
typedef struct {
    volatile uint32_t *registers;
    uint32_t poll_limit;
} H9MainSerialBus;

void h9_main_serial_bus_bind(H9MainSerialBus *bus,
                              volatile uint32_t *registers,
                              uint32_t poll_limit);
void h9_main_serial_bus_bind_target(H9MainSerialBus *bus,
                                     uint32_t poll_limit);
/* The target implementation is placed in relocated RAM code. A bounded
 * poll replaces the stock routine's unbounded busy wait. */
bool h9_main_serial_bus_transfer(H9MainSerialBus *bus,
                                  uint32_t transmit_word,
                                  uint8_t *received_byte);

#endif
