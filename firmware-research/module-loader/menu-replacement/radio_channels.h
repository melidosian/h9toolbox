#ifndef RADIO_CHANNELS_H
#define RADIO_CHANNELS_H
#include "radio_settings.h"
enum { RADIO_CHANNEL_FIELDS = 15, RADIO_CHANNEL_TARGETS = 201 };
/* Targets 1..199 are memories; 200/201 are VFO A/B. Fields are generated at
 * runtime because their offsets and scan/used bit masks depend on target. */
static inline unsigned radio_vfo_offset(unsigned vfo)
{
    return 0xcb0 + 4 * vfo;
}
int radio_channel_field(unsigned target, unsigned field, struct radio_setting *setting);
int radio_channel_copy(const struct radio_storage *storage, unsigned channel, unsigned vfo);
int radio_channel_delete(const struct radio_storage *storage, unsigned channel);
#endif
