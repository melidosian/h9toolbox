#include "h9_main_aprs_mute.h"
#include "h9_aprs_burst_protocol.h"

#include <stddef.h>

void h9_main_aprs_mute_event(H9MainAprsMuteLease *lease,
                              bool active, uint32_t now_ms)
{
    if (lease == NULL) return;
    lease->active = active;
    if (active) lease->last_active_ms = now_ms;
}

bool h9_main_aprs_mute_should_silence(const H9MainAprsMuteLease *lease,
                                      bool mute_option_enabled,
                                      bool aprs_receive_route_active,
                                      uint32_t now_ms)
{
    return lease != NULL && mute_option_enabled &&
           aprs_receive_route_active && lease->active &&
           (uint32_t)(now_ms - lease->last_active_ms) <
               H9_APRS_BURST_LEASE_MS;
}
