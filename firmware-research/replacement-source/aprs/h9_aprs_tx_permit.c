#include "h9_aprs_tx_permit.h"
#include "h9_aprs_tx_permit_protocol.h"

#include <stddef.h>

void h9_aprs_tx_permit_set(H9AprsTxPermit *permit, bool enabled,
                            uint32_t now_ms)
{
    if (permit == NULL) return;
    permit->enabled = enabled;
    permit->renewed_ms = now_ms;
}

bool h9_aprs_tx_permit_active(const H9AprsTxPermit *permit,
                              uint32_t now_ms)
{
    return permit != NULL && permit->enabled &&
           (uint32_t)(now_ms - permit->renewed_ms) <
               H9_APRS_TX_PERMIT_LEASE_MS;
}
