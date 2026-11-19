#include "aprs_resident_config.h"
#include "gfx.h"

#ifndef BEACON_TICK
#define BEACON_TICK (*(const volatile uint32_t *)0x1D8B0u)
#endif

static const char *beacon_status(const struct h9_runtime_status *status)
{
    if (!status->worker_ready || !status->polls) return "SCHEDULER UNAVAILABLE";
    if (!status->uart_online) return "GPS LINK WAIT";
    if (!status->beacon.gps.fix) return "WAITING FOR GPS";
    if (!status->beacon.armed) return "GPS FIX";
    return status->beacon.pending ? "GPS FIX  BEACON DUE" : "GPS FIX  SCHEDULING";
}

unsigned module_main(const struct h9_api *loader, unsigned self)
{
    (void)self;
    const struct h9_runtime_api *api = aprs_runtime(loader);
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, GFX_BLACK);
    gfx_fill(0, 0, H9_WIDTH, 19, GFX_BLUE);
    gfx_text(4, 2, "APRS BEACON", 11, 2, GFX_WHITE, GFX_BLUE);
    gfx_text(4, 84, "BENCH MODE  TX DISABLED", 25, 1, GFX_GREY, GFX_BLACK);
    gfx_text(4, 116, "BACK  TOOLBOX", 25, 1, GFX_WHITE, GFX_BLACK);
    gfx_text(4, 48, api ? "WAITING FOR STATUS" : "SCHEDULER UNAVAILABLE", 25, 1, GFX_YELLOW, GFX_BLACK);
    gfx_wait_release();
    uint32_t last = BEACON_TICK - 100u;
    while (h9_key() != H9_KEY_BACK) {
        uint32_t now = BEACON_TICK;
        struct h9_runtime_status status;
        if (api && now - last >= 100u && api->read_status(&status)) {
            gfx_fill(0, 33, H9_WIDTH, 25, GFX_BLACK);
            gfx_text(4, 34, status.settings.armed ? "SCHEDULE ARMED" : "SCHEDULE OFF", 25, 1, GFX_WHITE, GFX_BLACK);
            gfx_text(4, 48, beacon_status(&status), 25, 1,
                     status.beacon.gps.fix ? GFX_GREEN : GFX_YELLOW, GFX_BLACK);
            last = now;
        }
        h9_delay(10);
    }
    gfx_wait_release();
    return 0;
}
