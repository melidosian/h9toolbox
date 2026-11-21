#ifndef RADIO_APRS_H
#define RADIO_APRS_H
#include "../modules/aprs/aprs_resident_config.h"
#include "radio_settings.h"

enum { BEACON_OFF, BEACON_TIMED, BEACON_PTT, BEACON_SMART, BEACON_TYPE_COUNT };
enum { APRS_BEACON_TYPE = APRS_SETTING_COUNT };
unsigned radio_beacon_type(const struct radio_storage *, const struct ts_settings *);
const char *radio_beacon_label(unsigned);
const char *radio_beacon_save(const struct radio_storage *, const struct h9_api *,
    const struct h9_runtime_api *, struct ts_settings *, unsigned type);
#endif
