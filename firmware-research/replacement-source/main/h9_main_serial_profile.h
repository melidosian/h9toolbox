#ifndef H9_MAIN_SERIAL_PROFILE_H
#define H9_MAIN_SERIAL_PROFILE_H

#include <stdint.h>

/* Stock app+0x08843C constructs this 32-byte argument before calling the
 * serial-open routine at app+0x0C57BC. Pointer fields are 32-bit Pi32 values. */
typedef struct {
    uint8_t tx_pin;
    uint8_t rx_pin;
    uint8_t reserved_02[2];
    uint32_t storage_address;
    uint32_t storage_size;
    uint32_t read_limit;
    uint32_t field_10;
    uint32_t event_callback_address;
    uint32_t callback_context_address;
    uint32_t baud;
} H9MainSerialOpenRecord;

typedef struct {
    uint32_t mmio_base;
    uint8_t interrupt_number;
} H9MainSerialRoute;

/* Builds the stock TD-H9 APRS serial profile using caller-owned storage and
 * an authored event callback. No register or ROM operation is performed. */
void h9_main_aprs_serial_profile(H9MainSerialOpenRecord *record,
                                 uint32_t storage_address,
                                 uint32_t event_callback_address);

/* Returns 0..2 for a pin pair present in the stock UART pin tables, or -1.
 * The hardware-open routine also checks whether that UART is already in use. */
int h9_main_serial_route_pins(uint8_t tx_pin, uint8_t rx_pin);

/* Returns the observed UART register base and interrupt for channel 0..2. */
int h9_main_serial_route(int channel, H9MainSerialRoute *route);

#endif
