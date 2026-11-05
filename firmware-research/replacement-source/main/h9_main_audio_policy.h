#ifndef H9_MAIN_AUDIO_POLICY_H
#define H9_MAIN_AUDIO_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/* Stock app+0x052102 reads these bytes before deciding whether the speaker
 * request can pass. Names retain RAM addresses because most meanings remain
 * unproven. status_1692c is the result of app+0x01682C. */
typedef struct {
    uint8_t ram_11b16;
    uint8_t ram_12bf4;
    uint8_t ram_c1b2;
    uint8_t ram_c1b4;
    uint8_t ram_c1c3;
    uint8_t ram_c1c2;
    uint8_t ram_c1c1;
    uint8_t ram_12bf6;
    uint8_t ram_11b23;
    uint8_t ram_11b20;
    uint8_t ram_11b4d;
    uint8_t ram_11b24;
    uint8_t ram_11afd;
    uint8_t ram_12b9e;
    uint8_t ram_11b1a;
    uint8_t ram_11b21;
    uint8_t ram_11cc8;
    uint8_t ram_11afe;
    uint8_t ram_11afb;
    uint8_t ram_12bd8;
    uint8_t ram_11b25;
    uint8_t status_1692c;
} H9MainAudioPolicyState;

bool h9_main_audio_policy_evaluate(const H9MainAudioPolicyState *state,
                                    bool requested_on);
/* Adapter for H9MainAudioPolicy in h9_main_audio_arbiter.h. */
bool h9_main_audio_policy_callback(void *context, bool requested_on,
                                    bool *effective_on);

#endif
