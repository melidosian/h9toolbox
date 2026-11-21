#include "radio_aprs.h"

/* Native VM stores PTT and Timed as two adjacent one-byte flags. */
enum { BEACON_FLAGS = 0x30EE };

unsigned radio_beacon_type(const struct radio_storage *storage, const struct ts_settings *cfg)
{
    uint8_t flags[2];
    if (storage->read(BEACON_FLAGS, flags, sizeof flags)) return BEACON_TYPE_COUNT;
    if (flags[0] == 1) return BEACON_PTT;
    if (flags[1] == 1) return BEACON_TIMED;
    if (cfg && cfg->armed) return cfg->timed ? BEACON_TIMED : BEACON_SMART;
    return BEACON_OFF;
}

const char *radio_beacon_label(unsigned type)
{
    static const char *const labels[] = { "OFF", "TIMED", "PTT", "SMART", "READ ERROR" };
    return labels[type < BEACON_TYPE_COUNT ? type : BEACON_TYPE_COUNT];
}

static int store_profile(const struct radio_storage *storage, const uint8_t *profile, unsigned size)
{
    uint8_t check[4];
    if (storage->write(BEACON_FLAGS, profile, size) ||
        storage->read(BEACON_FLAGS, check, size)) return 0;
    for (unsigned i = 0; i < size; ++i)
        if (check[i] != profile[i]) return 0;
    return 1;
}

const char *radio_beacon_save(const struct radio_storage *storage, const struct h9_api *loader,
    const struct h9_runtime_api *api, struct ts_settings *saved, unsigned type)
{
    uint8_t prior[4];
    if (type >= BEACON_TYPE_COUNT || storage->read(BEACON_FLAGS, prior, sizeof prior))
        return "SETTINGS READ FAILED";

    struct ts_settings previous = *saved, next = previous;
    next.timed = 0;
    next.armed = type == BEACON_SMART;
    if (next.armed) {
        struct h9_runtime_status status;
        if (!api || !api->read_status(&status) || !status.worker_ready) return "SCHEDULER UNAVAILABLE";
        if (!aprs_select_tx(&next)) return "CHECK APRS TX CHANNEL";
    }
    /* Carry the old resident Timed interval into the native profile when
     * that legacy mode is explicitly saved as Timed. */
    unsigned size = 2;
    uint8_t profile[4] = { type == BEACON_PTT, type == BEACON_TIMED, prior[2], prior[3] };
    if (type == BEACON_TIMED && previous.armed && previous.timed && !prior[0] && !prior[1]) {
        unsigned seconds = previous.minutes * 60u;
        profile[2] = seconds;
        profile[3] = seconds >> 8;
        size = 4;
    }

    /* Arm Smart only after both native triggers are disabled. Disarm the
     * resident worker before enabling either native trigger. */
    if (!next.armed && saved->armed && !aprs_settings_save(loader, api, &next, saved))
        return "SETTINGS SAVE FAILED";
    unsigned changed = size == 4 || profile[0] != prior[0] || profile[1] != prior[1];
    if ((!changed || store_profile(storage, profile, size)) &&
        (!next.armed || aprs_settings_save(loader, api, &next, saved))) return 0;

    if (changed && !store_profile(storage, prior, size)) return "CHECK BEACON TYPE";
    if (previous.armed && !saved->armed && !aprs_settings_save(loader, api, &previous, saved))
        return "CHECK BEACON TYPE";
    return "SETTINGS SAVE FAILED";
}
