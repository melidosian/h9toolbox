#ifndef RADIO_ACTIONS_H
#define RADIO_ACTIONS_H
#include "h9mod.h"

enum radio_action_id {
    RADIO_ACT_CHANNELS, RADIO_ACT_INFO, RADIO_ACT_APRS_INFO, RADIO_ACT_MANUAL,
    RADIO_ACT_RESET_VFO, RADIO_ACT_RESET_CHANNELS, RADIO_ACT_RESET_ANALOG,
    RADIO_ACT_RESET_ALL, RADIO_ACT_RESET_APRS,
    RADIO_ACT_BT_INFO, RADIO_ACT_BT_PAIRING, RADIO_ACT_BT_RESET,
    RADIO_ACT_TONE_SCAN, RADIO_ACT_FREQ_SCAN, RADIO_ACT_RANGE_SCAN, RADIO_ACT_SPECTRUM,
    RADIO_ACT_CHANNEL_PREVIEW, RADIO_ACTION_COUNT
};
struct radio_action_info { const char *id, *label, *group, *help; };
extern const struct radio_action_info radio_actions[RADIO_ACTION_COUNT];
int radio_action(const struct h9_api *, unsigned action, uint16_t accent);
#endif
