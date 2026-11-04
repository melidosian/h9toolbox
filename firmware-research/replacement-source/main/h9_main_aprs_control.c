#include "h9_main_aprs_control.h"

#include <string.h>

uint16_t h9_main_control_crc(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;
    size_t index;
    unsigned bit;
    if (data == NULL) return 0;
    for (index = 0; index < length; ++index) {
        crc ^= (uint16_t)data[index] << 8;
        for (bit = 0; bit < 8u; ++bit)
            crc = (uint16_t)((crc << 1) ^
                             ((crc & 0x8000u) != 0u ? 0x1021u : 0u));
    }
    return crc;
}

int h9_main_control_from_frame(const H9MainWireReceiver *receiver,
                               H9MainControlAction *action)
{
    size_t body_length;
    uint16_t received_crc;
    uint16_t code;
    if (action == NULL) return 0;
    *action = H9_MAIN_CONTROL_NONE;
    if (receiver == NULL || !receiver->frame_ready ||
        receiver->command != 0x51u || receiver->length < 4u ||
        receiver->length > H9_MAIN_APRS_RECEIVE_CAPACITY) return 0;
    body_length = receiver->length - 2u;
    received_crc = (uint16_t)((uint16_t)receiver->payload[body_length] << 8) |
                   receiver->payload[body_length + 1u];
    if (h9_main_control_crc(receiver->payload, body_length) != received_crc)
        return 0;
    code = (uint16_t)((uint16_t)receiver->payload[0] << 8) |
           receiver->payload[1];
    switch (code) {
    case H9_MAIN_CONTROL_06_01:
    case H9_MAIN_CONTROL_06_02:
    case H9_MAIN_CONTROL_06_03:
    case H9_MAIN_CONTROL_06_04:
    case H9_MAIN_CONTROL_06_05:
    case H9_MAIN_CONTROL_06_06:
    case H9_MAIN_CONTROL_07_01:
    case H9_MAIN_CONTROL_07_03:
        *action = (H9MainControlAction)code;
        return 1;
    default:
        return 0;
    }
}

bool h9_main_control_volume_init(H9MainControlVolumeState *state,
                                  uint8_t initial_level)
{
    if (state == NULL || initial_level > 30u) return false;
    memset(state, 0, sizeof *state);
    state->level = initial_level;
    return true;
}

bool h9_main_control_volume_apply(H9MainControlVolumeState *state,
                                   H9MainControlAction action,
                                   H9MainControlVolumeEffects *effects)
{
    int adjustment = 0;
    int next;
    bool held = false;
    if (state == NULL || effects == NULL || state->level > 30u) return false;
    memset(effects, 0, sizeof *effects);
    switch (action) {
    case H9_MAIN_CONTROL_06_01: adjustment = 1; break;
    case H9_MAIN_CONTROL_06_03: adjustment = -1; break;
    case H9_MAIN_CONTROL_06_05:
        adjustment = 1;
        state->held_adjustment = 1u;
        held = true;
        break;
    case H9_MAIN_CONTROL_06_06:
        adjustment = -1;
        state->held_adjustment = 2u;
        held = true;
        break;
    case H9_MAIN_CONTROL_06_02:
    case H9_MAIN_CONTROL_06_04:
        effects->cancel_timer = state->timer_armed;
        state->timer_armed = false;
        state->held_adjustment = 0u;
        return true;
    default:
        return false;
    }
    next = (int)state->level + adjustment;
    if (next < 0) next = 0;
    if (next > 30) next = 30;
    effects->level_changed = (uint8_t)next != state->level;
    effects->boundary_feedback = !effects->level_changed &&
                                 !state->timer_armed;
    state->level = (uint8_t)next;
    state->volume_display_pending = true;
    if (held) {
        effects->cancel_timer = state->timer_armed;
        effects->start_timer_units = 200u;
        state->timer_armed = true;
    } else if (!effects->level_changed && state->timer_armed) {
        effects->start_timer_units = 150u;
    }
    return true;
}
