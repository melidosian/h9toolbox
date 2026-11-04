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

H9AudioPlan h9_main_aprs_mute_plan(const H9MainAprsConfig *config,
                                   const H9MainAprsMuteLease *lease,
                                   H9Side received_side,
                                   bool route_verified,
                                   uint32_t now_ms)
{
    H9AudioPlan plan = h9_main_audio_plan(config, received_side, false);
    if (config != NULL && plan.decoder_feed &&
        h9_main_aprs_mute_should_silence(lease,
            config->mute_aprs_audio, route_verified, now_ms)) {
        plan.speaker_audio = false;
        plan.bluetooth_audio = false;
    }
    return plan;
}

bool h9_main_aprs_mute_apply(const H9MainAprsConfig *config,
                             const H9MainAprsMuteLease *lease,
                             uint32_t now_ms,
                             const H9MainAprsMuteOutputs *outputs)
{
    H9AudioPlan plan;
    H9Side received_side;
    bool route_verified;
    bool speaker_requested;
    bool bluetooth_requested;
    bool applied = false;
    bool ended;
    if (config == NULL || outputs == NULL || outputs->begin_route == NULL ||
        outputs->read_route == NULL || outputs->set_speaker == NULL ||
        outputs->set_bluetooth == NULL || outputs->end_route == NULL ||
        !outputs->begin_route(outputs->context)) return false;
    if (outputs->read_route(outputs->context, &received_side,
                            &route_verified, &speaker_requested,
                            &bluetooth_requested)) {
        plan = h9_main_aprs_mute_plan(config, lease, received_side,
                                      route_verified, now_ms);
        /* The policy grants or withholds each output; it never turns on a
         * path that ordinary squelch/audio routing has requested off. */
        if (outputs->set_bluetooth(outputs->context,
                                   bluetooth_requested && plan.bluetooth_audio))
            applied = outputs->set_speaker(outputs->context,
                                            speaker_requested && plan.speaker_audio);
    }
    ended = outputs->end_route(outputs->context);
    return applied && ended;
}
