#include "h9_main_speaker_gate.h"

#include <stddef.h>

void h9_main_speaker_gate_bind_target(H9MainSpeakerGate *gate)
{
    if (gate == NULL) return;
    gate->gpio_data = (volatile uint32_t *)(uintptr_t)0x001E5000u;
    gate->sync_register = (volatile uint32_t *)(uintptr_t)0x001E0200u;
    gate->audio_state = (uint8_t *)(uintptr_t)0x00011B4Du;
}

bool h9_main_speaker_gate_set(H9MainSpeakerGate *gate, bool enabled)
{
    uint32_t mask = 1u << 3;
    if (gate == NULL || gate->gpio_data == NULL ||
        gate->sync_register == NULL || gate->audio_state == NULL)
        return false;
    /* The stock pin writer samples this register before its GPIO update. */
    (void)*gate->sync_register;
#if defined(__PI32V2__)
    if (enabled)
        __asm__ volatile ("[%0+0] |= %1" ::
                          "r"(gate->gpio_data), "r"(mask) : "memory");
    else {
        uint32_t keep = ~mask;
        __asm__ volatile ("[%0+0] &= %1" ::
                          "r"(gate->gpio_data), "r"(keep) : "memory");
    }
#else
    uint32_t value = *gate->gpio_data;
    if (enabled) value |= mask;
    else value &= ~mask;
    *gate->gpio_data = value;
#endif
    *gate->audio_state = (uint8_t)((*gate->audio_state & 0xFEu) |
                                   (enabled ? 1u : 0u));
    return true;
}
