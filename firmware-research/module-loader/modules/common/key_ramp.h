#ifndef H9_KEY_RAMP_H
#define H9_KEY_RAMP_H
#include "h9mod.h"

/* The 100 Hz clock gives a 500 ms initial delay and 160/80/40 ms repeats.
 * Zero-initialize for a released keypad,
 * or seed sample and stable with the key held when a screen opens. */
struct h9_key_ramp {
    uint32_t pressed_at, next_repeat;
    uint8_t sample, stable, armed;
};

#if defined(H9_RADIO_SETTINGS) && !defined(H9_KEY_RAMP_IMPLEMENTATION)
/* Far menu screens share one copy. Separately installed apps use their own. */
unsigned h9_key_ramp(struct h9_key_ramp *input, unsigned key, uint32_t now, unsigned scroll);
#else
#ifndef H9_KEY_RAMP_IMPLEMENTATION
static inline
#endif
unsigned h9_key_ramp(struct h9_key_ramp *input, unsigned key, uint32_t now, unsigned scroll)
{
    if (key != input->sample) {
        input->sample = key;
        return H9_KEY_NONE;
    }
    if (key != input->stable) {
        input->stable = key;
        input->armed = key == H9_KEY_UP || key == H9_KEY_DOWN;
        input->pressed_at = now;
        input->next_repeat = now + 50;
        return key;
    }
    if (!scroll || !input->armed || (int32_t)(now - input->next_repeat) < 0) return H9_KEY_NONE;
    uint32_t age = now - input->pressed_at;
    input->next_repeat = now + (age < 100 ? 16 : age < 180 ? 8 : 4);
    return key;
}
#endif
#endif
