#include "h9_aprs_uart2_maintenance.h"

#include <stddef.h>

void h9_aprs_uart2_maintenance_init(H9AprsUart2Maintenance *state)
{
    if (state == NULL) return;
    state->loop_passes = 0u;
    state->last_attempt_tick = 0u;
}

bool h9_aprs_uart2_maintenance_due(H9AprsUart2Maintenance *state,
                                    uint32_t now_tick, bool faulted,
                                    bool receive_idle)
{
    if (state == NULL) return false;
    if (++state->loop_passes < 1000u) return false;
    state->loop_passes = 0u;
    if (!faulted || !receive_idle ||
        (uint32_t)(now_tick - state->last_attempt_tick) <= 2000u)
        return false;
    state->last_attempt_tick = now_tick;
    return true;
}
