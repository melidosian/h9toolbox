#include "h9_main_serial_profile.h"

#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(H9MainSerialOpenRecord) == 32u,
               "Pi32 serial-open record must be 32 bytes");
_Static_assert(offsetof(H9MainSerialOpenRecord, event_callback_address) == 20u,
               "Pi32 serial event callback offset differs");
_Static_assert(offsetof(H9MainSerialOpenRecord, baud) == 28u,
               "Pi32 serial baud offset differs");

/* These are the two 11-byte stock tables copied to RAM 0xDA60 and 0xDA6C.
 * The same table index maps to a UART channel by index / 4. */
static const uint8_t tx_pin_table[11] = {
    0x05u, 0x14u, 0x15u, 0x0Bu, 0x10u, 0x20u,
    0x00u, 0x3Du, 0x02u, 0x09u, 0x19u
};
static const uint8_t rx_pin_table[11] = {
    0x06u, 0x16u, 0x15u, 0x0Cu, 0x11u, 0x21u,
    0x01u, 0x3Eu, 0x03u, 0x0Au, 0x1Au
};

void h9_main_aprs_serial_profile(H9MainSerialOpenRecord *record,
                                 uint32_t storage_address,
                                 uint32_t event_callback_address)
{
    if (record == NULL) return;
    memset(record, 0, sizeof *record);
    record->tx_pin = 0x20u;
    record->rx_pin = 0x21u;
    record->storage_address = storage_address;
    record->storage_size = 2048u;
    record->read_limit = 1024u;
    record->field_10 = 10u;
    record->event_callback_address = event_callback_address;
    record->baud = 19200u;
}

int h9_main_serial_route_pins(uint8_t tx_pin, uint8_t rx_pin)
{
    size_t index;
    for (index = 0u; index < sizeof tx_pin_table; ++index) {
        if (tx_pin_table[index] == tx_pin && rx_pin_table[index] == rx_pin)
            return (int)(index / 4u);
    }
    return -1;
}

int h9_main_serial_route(int channel, H9MainSerialRoute *route)
{
    if (route == NULL || channel < 0 || channel > 2) return 0;
    route->mmio_base = 0x001E2000u + (uint32_t)channel * 0x100u;
    route->interrupt_number = (uint8_t)(0x12u + (unsigned)channel);
    return 1;
}
