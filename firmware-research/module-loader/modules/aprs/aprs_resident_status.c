#include "aprs_resident_config.h"
#include "app_theme.h"
#ifdef H9_RESIDENT_TX
#include "aprs_tx.h"
#endif

#ifndef BEACON_TICK
#define BEACON_TICK (*(const volatile uint32_t *)0x1D8B0u)
#endif

static const char *mode_label(const struct ts_settings *settings)
{
    if (H9_HAS_MESSAGES) {
        if (*(const volatile uint8_t *)0xC512u == 1) return "PTT";
        if (*(const volatile uint8_t *)0xC513u == 1) return "TIMED";
        return !settings->armed ? "OFF" : settings->timed ? "TIMED" : "SMART";
    }
    return settings->armed ? "ARMED" : "OFF";
}

static const char *beacon_status(const struct h9_runtime_status *status)
{
    if (!status->worker_ready || !status->polls) return "SCHEDULER UNAVAILABLE";
    if (!status->uart_online) return "GPS LINK WAIT";
#ifdef H9_RESIDENT_TX
    switch (status->beacon.wait) {
    case H9_BS_TX_FAULT: return "TX RESTORE FAULT";
    case H9_BS_RETRY: return "TX RETRY WAIT";
    case H9_BS_BUSY: return "WAITING FOR IDLE RADIO";
    case H9_BS_SIDE: return "WAITING FOR APRS VFO";
    case H9_BS_FREQUENCY: return "CHECK APRS FREQUENCY";
    case H9_BS_SETTING: return "CHECK APRS VFO SETTING";
    case H9_BS_CALLSIGN: return "CHECK APRS CALLSIGN";
    case H9_BS_STOCK_CONTROL: return "CHECK STOCK APRS SETUP";
    case H9_BS_STOCK_POSITION: return "WAITING FOR RADIO GPS";
    case H9_BS_POSITION: return "WAITING FOR GPS";
    default: break;
    }
#endif
    if (!status->beacon.gps.fix) return "WAITING FOR GPS";
    if (!status->beacon.armed) return "GPS FIX";
    return status->beacon.pending ? "GPS FIX  BEACON DUE" : "GPS FIX  SCHEDULING";
}

#ifdef H9_RESIDENT_TX
static void tx_status(const struct h9_runtime_status *status)
{
    if (status->settings.armed) {
        char vfo[15] = { (char)('A' + status->settings.tx_side), ' ' };
        aprs_tx_mhz(vfo + 2, status->settings.tx_frequency);
        gfx_label(UI_LEFT, 38, "APRS VFO", 48, 1, UI_DIM, UI_BG);
        gfx_text(97, 38, vfo, 10, 1, UI_TEXT, UI_BG);
    }
    char digits[11], *out = digits + 10;
    uint32_t count = status->beacon.submitted;
    *out = 0;
    do { *--out = (char)('0' + count % 10u); count /= 10u; } while (count);
    gfx_label(UI_LEFT, 65, "TX SUBMITTED", 72, 1, UI_DIM, UI_BG);
    gfx_text(H9_WIDTH - UI_LEFT - (unsigned)(digits + 10 - out) * 6u, 65, out, 10, 1, UI_TEXT, UI_BG);
}
#endif

unsigned module_main(const struct h9_api *loader, unsigned self)
{
    (void)self;
    const struct h9_runtime_api *api = aprs_runtime(loader);
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("BEACON STATUS");
#ifdef H9_RESIDENT_TX
    gfx_label(UI_LEFT, 87, "TX IN RADIO VIEW", 150, 1, UI_DIM, UI_BG);
#else
    gfx_label(UI_LEFT, 80, "BENCH MODE  TX DISABLED", 150, 1, UI_DIM, UI_BG);
#endif
    ui_footer("BACK  TOOLBOX");
    gfx_label(UI_LEFT, 51, api ? "WAITING FOR STATUS" : "SCHEDULER UNAVAILABLE", 150, 1, UI_ACCENT, UI_BG);
    gfx_wait_release();
    uint32_t last = BEACON_TICK - 100u;
    while (h9_key() != H9_KEY_BACK) {
        uint32_t now = BEACON_TICK;
        struct h9_runtime_status status;
        if (api && now - last >= 100u && api->read_status(&status)) {
            gfx_fill(0, 26, H9_WIDTH, 48, UI_BG);
            gfx_label(UI_LEFT, 27, H9_HAS_MESSAGES ? "BEACON TYPE" : "SCHEDULE",
                     66, 1, UI_DIM, UI_BG);
            gfx_label(127, 27, mode_label(&status.settings), 30, 1, UI_TEXT, UI_BG);
            gfx_label(UI_LEFT, 51, beacon_status(&status), 150, 1,
                     status.beacon.gps.fix && status.beacon.wait < H9_BS_BUSY ? UI_OK : UI_ACCENT, UI_BG);
#ifdef H9_RESIDENT_TX
            tx_status(&status);
#endif
            last = now;
        }
        h9_delay(10);
    }
    gfx_wait_release();
    return 0;
}
