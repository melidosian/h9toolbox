#include "h9_main_audio_policy.h"

#include <stddef.h>

bool h9_main_audio_policy_evaluate(const H9MainAudioPolicyState *state,
                                    bool requested_on)
{
    uint8_t mode;
    if (state == NULL || state->ram_11b16 == 0u ||
        state->ram_12bf4 != 0u) return false;

    if (state->ram_c1b2 == 2u) {
        mode = state->ram_c1b4;
        if (mode >= 2u && mode <= 4u) return false;
        if (mode == 1u && state->ram_c1c3 == 1u) return false;
        if (mode == 5u || mode == 6u) {
            if (state->ram_c1c2 == 1u && state->ram_12bf6 == 1u)
                return false;
            if (state->ram_11b23 == 1u && state->ram_c1c1 == 1u &&
                state->ram_12bf6 == 0u) return false;
        }
        if (state->ram_12bf6 != 1u && state->ram_11b20 == 0u &&
            (state->ram_11b4d & 4u) != 0u) return requested_on;
        if (state->status_1692c == 0x2Eu) return true;
        return state->ram_11b24 != 1u;
    }

    if (state->ram_11afd == 1u && state->ram_11b20 != 0u) return true;
    if (state->ram_12b9e == 0u) {
        if (state->ram_11b1a == 0u ||
            state->ram_11b21 != 0u ||
            (state->ram_11cc8 & 1u) != 0u) return true;
    } else if (state->ram_11cc8 != 0u) {
        return true;
    }
    if (state->ram_11afe == 1u ||
        (state->ram_11afb == 1u && state->ram_11b20 != 0u) ||
        (state->ram_11b4d & 5u) != 0u) return true;
    if (state->ram_11b20 == 0u) return requested_on;
    if (state->ram_12bd8 == 1u && state->ram_11b25 == 0u)
        return false;
    return state->ram_11b24 != 1u;
}

bool h9_main_audio_policy_callback(void *context, bool requested_on,
                                    bool *effective_on)
{
    if (context == NULL || effective_on == NULL) return false;
    *effective_on = h9_main_audio_policy_evaluate(
        (const H9MainAudioPolicyState *)context, requested_on);
    return true;
}
