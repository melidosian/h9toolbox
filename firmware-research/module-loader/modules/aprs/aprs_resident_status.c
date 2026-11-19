#include "aprs_resident_config.h"
#include "app_theme.h"

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
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("BEACON STATUS");
    gfx_text(UI_LEFT, 80, "BENCH MODE  TX DISABLED", 25, 1, UI_DIM, UI_BG);
    ui_footer("BACK  TOOLBOX");
    gfx_text(UI_LEFT, 51, api ? "WAITING FOR STATUS" : "SCHEDULER UNAVAILABLE", 25, 1, UI_ACCENT, UI_BG);
    gfx_wait_release();
    uint32_t last = BEACON_TICK - 100u;
    while (h9_key() != H9_KEY_BACK) {
        uint32_t now = BEACON_TICK;
        struct h9_runtime_status status;
        if (api && now - last >= 100u && api->read_status(&status)) {
            gfx_fill(0, 26, H9_WIDTH, 35, UI_BG);
            gfx_text(UI_LEFT, 27, "SCHEDULE", 8, 1, UI_DIM, UI_BG);
            gfx_text(status.settings.armed ? 127 : 139, 27, status.settings.armed ? "ARMED" : "OFF",
                     5, 1, UI_TEXT, UI_BG);
            gfx_text(UI_LEFT, 51, beacon_status(&status), 25, 1,
                     status.beacon.gps.fix ? UI_OK : UI_ACCENT, UI_BG);
            last = now;
        }
        h9_delay(10);
    }
    gfx_wait_release();
    return 0;
}
