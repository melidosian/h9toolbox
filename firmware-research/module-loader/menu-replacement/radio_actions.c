#include "app_theme.h"
#include "radio_actions.h"
#include "radio_bluetooth.h"
#include "radio_scan.h"
#include "radio_system.h"
#include "radio_theme.h"


static int scan(void *mode) { return radio_scan(*(unsigned *)mode); }
static int bt_reset(void *unused) { (void)unused; return radio_bt_reset(); }
static int confirm_bt_reset(const struct h9_api *loader)
{
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("RESET BLUETOOTH?");
    gfx_label(3, 35, "REMOVE PAIRED DEVICES", 156, 1, UI_TEXT, UI_BG);
    gfx_label(3, 52, "RESTORE BT DEFAULTS", 156, 1, UI_HINT, UI_BG);
    ui_footer("MENU CONFIRM  BACK CANCEL");
    gfx_wait_release();
    for (;;) {
        unsigned key = gfx_wait_key();
        if (key == H9_KEY_BACK) return RADIO_OK;
        if (key == H9_KEY_MENU) return radio_with_rf(loader, bt_reset, 0);
    }
}
int radio_action(const struct h9_api *loader, unsigned action, uint16_t accent)
{
    if (action == RADIO_ACT_CHANNEL_PREVIEW) return radio_channel_preview();
    if (action == RADIO_ACT_CHANNELS) return radio_channel_editor(&radio_stock_storage, accent);
    if (action >= RADIO_ACT_INFO && action <= RADIO_ACT_RESET_APRS)
        return radio_system(loader, action - RADIO_ACT_INFO);
    if (action == RADIO_ACT_BT_INFO) return radio_bt_info();
    if (action == RADIO_ACT_BT_PAIRING) return radio_bt_pairing();
    if (action == RADIO_ACT_BT_RESET) return confirm_bt_reset(loader);
    if (action >= RADIO_ACT_TONE_SCAN && action <= RADIO_ACT_SPECTRUM) {
        unsigned mode = action - RADIO_ACT_TONE_SCAN;
        return radio_with_rf(loader, scan, &mode);
    }
    return RADIO_INVALID;
}
