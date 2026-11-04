#include "h9_main_aprs_aux_control.h"
#include "h9_main_aprs_wire.h"

bool h9_main_aprs_aux_set_enabled(bool enabled, bool serial_open,
                                   H9MainAprsAuxWrite write,
                                   void *context)
{
    uint8_t frame[16];
    const uint8_t payload = enabled ? 1u : 2u;
    size_t length;
    if (!serial_open || write == NULL) return false;
    length = h9_main_aprs_wire_encode(0x11u, &payload, 1u,
                                      frame, sizeof frame);
    return length != 0u && write(context, frame, length);
}
