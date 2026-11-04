#include "h9_main_beacon.h"
#include <stddef.h>

H9BeaconDecision h9_main_beacon_tick(H9BeaconScheduler *scheduler,
                                     uint8_t timed_beacon_setting,
                                     uint16_t interval_setting,
                                     uint8_t state_11b1e,
                                     uint8_t ptt_priority)
{
    H9BeaconDecision result = { false, false };
    uint32_t threshold;
    if (scheduler == NULL) return result;
    if (timed_beacon_setting != 1u) {
        scheduler->counter = 0;
        return result;
    }
    if (interval_setting == 0) return result;
    scheduler->counter += 1u;
    threshold = (uint32_t)interval_setting * 100u;
    if (scheduler->counter < threshold) return result;
    scheduler->counter = 0;
    if (state_11b1e == 1u && ptt_priority == 0u) return result;
    result.call_tx_selector = true;
    result.restore_receive_if_selector_succeeds = state_11b1e != 1u;
    return result;
}
