#include "h9_main_audio_arbiter.h"

#include <stddef.h>

bool h9_main_audio_gate_plan(uint8_t current_state, uint8_t source,
                             bool requested_on, H9MainAudioPolicy policy,
                             void *policy_context, H9MainAudioGatePlan *plan)
{
    bool effective_on;
    if (plan == NULL) return false;
    plan->next_state = current_state;
    plan->apply_speaker = false;
    plan->speaker_on = requested_on;

    /* The stock helper returns before changing the hold bit when the GPIO
     * state already matches the request. */
    if (((current_state & 1u) != 0u) == requested_on) return true;
    if (source != 13u && !requested_on && (current_state & 4u) != 0u)
        return true;

    if (source == 13u) {
        plan->next_state = (uint8_t)((current_state & 0xFBu) |
                                     (requested_on ? 4u : 0u));
    }
    if (source == 1u || source == 8u || source == 11u ||
        source == 12u || source == 13u) {
        effective_on = requested_on;
    } else {
        if (policy == NULL || !policy(policy_context, requested_on,
                                      &effective_on)) return false;
    }

    /* Stock does not drive GPIO when the policy's effective state disagrees
     * with the requested state. Its separate side effect for a call state
     * is deliberately not represented by this speaker-only plan. */
    if (effective_on == requested_on) plan->apply_speaker = true;
    return true;
}
