#ifndef H9_MAIN_SPEAKER_GATE_H
#define H9_MAIN_SPEAKER_GATE_H

#include <stdbool.h>
#include <stdint.h>

/* Stock app+0x052228 writes GPIO pin 3 and mirrors its requested state in
 * RAM 0x00011B4D bit 0. The physical probe confirmed this controls the
 * built-in speaker, while Bluetooth audio bypasses it. */
typedef struct {
    volatile uint32_t *gpio_data;
    volatile uint32_t *sync_register;
    uint8_t *audio_state;
} H9MainSpeakerGate;

void h9_main_speaker_gate_bind_target(H9MainSpeakerGate *gate);
bool h9_main_speaker_gate_set(H9MainSpeakerGate *gate, bool enabled);

#endif
