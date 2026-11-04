#ifndef H9_MAIN_AUDIO_ARBITER_H
#define H9_MAIN_AUDIO_ARBITER_H

#include <stdbool.h>
#include <stdint.h>

/* Stock app+0x05224C calls app+0x052102 for ordinary sources. The latter's
 * policy depends on radio, call, and Bluetooth state still being mapped. */
typedef bool (*H9MainAudioPolicy)(void *context, bool requested_on,
                                  bool *effective_on);

typedef struct {
    uint8_t next_state;
    bool apply_speaker;
    bool speaker_on;
} H9MainAudioGatePlan;

/* Port of the stock gate's early request/hold decision. Source 13 owns bit 2
 * of RAM 0x11B4D. Sources 1, 8, 11, and 12 use direct requests; other sources
 * delegate to the still-incomplete app+0x052102 policy. The caller must apply
 * the speaker action and then mirror bit 0, as app+0x052228 does. */
bool h9_main_audio_gate_plan(uint8_t current_state, uint8_t source,
                             bool requested_on, H9MainAudioPolicy policy,
                             void *policy_context, H9MainAudioGatePlan *plan);

#endif
