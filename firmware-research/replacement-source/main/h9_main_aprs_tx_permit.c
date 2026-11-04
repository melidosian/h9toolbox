#include "h9_main_aprs_tx_permit.h"
#include "h9_main_aprs_wire.h"
#include "h9_aprs_identity.h"
#include "h9_aprs_tx_permit_protocol.h"

static bool version_equals(const char actual[H9_MAIN_APRS_VERSION_CAPACITY],
                            const char *expected)
{
    size_t index;
    for (index = 0u; index < H9_MAIN_APRS_VERSION_CAPACITY; ++index) {
        if (actual[index] != expected[index]) return false;
        if (expected[index] == '\0') return true;
    }
    return false;
}

size_t h9_main_aprs_tx_permit_encode(const H9MainAprsVersions *versions,
                                      bool enabled, uint8_t *output,
                                      size_t capacity)
{
    const uint8_t payload[2] = {
        H9_APRS_TX_PERMIT_VERSION,
        enabled ? H9_APRS_TX_PERMIT_ENABLE : 0u,
    };
    if (versions == NULL || !versions->has_module ||
        !versions->has_protocol ||
        !version_equals(versions->module, H9_APRS_MODULE_VERSION) ||
        !version_equals(versions->protocol, H9_APRS_PROTOCOL_VERSION))
        return 0u;
    return h9_main_aprs_wire_encode(H9_APRS_TX_PERMIT_COMMAND, payload,
                                     sizeof payload, output, capacity);
}
