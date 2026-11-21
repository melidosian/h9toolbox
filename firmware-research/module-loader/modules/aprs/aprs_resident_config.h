#ifndef H9_APRS_RESIDENT_CONFIG_H
#define H9_APRS_RESIDENT_CONFIG_H

#include "h9mod.h"
#include "../../resident-service/runtime.h"

int aprs_select_tx(struct ts_settings *);
enum aprs_setting { APRS_SCHEDULE, APRS_MODE, APRS_INTERVAL, APRS_SYMBOL, APRS_MUTE, APRS_SETTING_COUNT };

static inline const struct h9_runtime_api *aprs_runtime(const struct h9_api *loader)
{
    const struct h9_runtime_api *api = loader->version >= 4 && loader->resident ? loader->resident->api : 0;
    return api && api->abi == H9_RUNTIME_ABI ? api : 0;
}

const char *aprs_setting_value(enum aprs_setting setting, const struct ts_settings *config, char minutes[8]);
const char *aprs_setting_adjust(const struct h9_runtime_api *api, enum aprs_setting setting,
                               struct ts_settings *config, int direction);
int aprs_settings_save(const struct h9_api *loader, const struct h9_runtime_api *api,
                       const struct ts_settings *config, struct ts_settings *saved);

#endif
