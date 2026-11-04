#include "h9_main_beacon.h"
#include <stddef.h>

bool h9_main_beacon_position_ready(const H9BeaconPositionStatus *status)
{
    if (status == NULL) return false;
    return status->position_source != 1u ||
           (status->gps_fix_valid &&
            (status->latitude_raw != 0u || status->longitude_raw != 0u));
}

bool h9_main_beacon_caller_allows_tick(const H9BeaconCallerState *state)
{
    return state != NULL && state->state_11ab0 <= 1u &&
           state->state_11ac6 == 3u && state->flag_11afb == 0u &&
           state->flag_11afe == 0u;
}

H9BeaconDecision h9_main_beacon_caller_tick(
    H9BeaconScheduler *scheduler, const H9BeaconCallerState *state,
    uint8_t timed_beacon_setting, uint16_t interval_setting,
    uint8_t state_11b1e, uint8_t ptt_priority)
{
    H9BeaconDecision skipped = { false, false };
    if (!h9_main_beacon_caller_allows_tick(state)) return skipped;
    return h9_main_beacon_tick(scheduler, timed_beacon_setting,
                               interval_setting, state_11b1e, ptt_priority);
}

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
