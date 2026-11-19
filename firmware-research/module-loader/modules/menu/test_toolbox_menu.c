#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "h9mod.h"

static uint32_t tick, menu_selection;
#define MENU_TICK tick
#define MENU_SELECTION menu_selection
#include "toolbox_menu.c"

#define BEACON_TICK tick
#include "../aprs/aprs_resident_status.c"

static const char names[][H9_NAME_BYTES] = {
    "MENU", "ABOUT", "APRS", ".APRS LIST", "GPS", "APRS BEACON", "CUSTOM APP", "APRS RAW",
    "APRS MAP", "APRS MSG", "APRS SEND"
};
static unsigned saves, committed_at, fail_save, saw_edit, saw_settings, saw_failure, saw_status, usb_updates;
static unsigned omit_station_list;
static struct ts_settings stored;

static void read_bundle(void *dst, uint32_t address, uint32_t length)
{
    assert(address >= H9_BUNDLE_HEADER && length == H9_NAME_BYTES);
    unsigned index = (address - H9_BUNDLE_HEADER) / H9_BUNDLE_ENTRY;
    assert(index < sizeof names / sizeof *names);
    if (omit_station_list && index == 3) memset(dst, 0, length);
    else memcpy(dst, names[index], length);
}

static unsigned runtime_status(struct h9_runtime_status *status)
{
    *status = (struct h9_runtime_status){
        .settings = stored, .worker_ready = 1, .polls = 1, .uart_online = 1
    };
    status->beacon.armed = status->beacon.pending = status->beacon.gps.fix = 1;
    return 1;
}

const char *aprs_setting_value(enum aprs_setting setting, const struct ts_settings *config, char minutes[8])
{
    (void)minutes;
    return setting == APRS_MUTE && config->mute ? "ON" : "OFF";
}

const char *aprs_setting_adjust(const struct h9_runtime_api *api, enum aprs_setting setting,
                               struct ts_settings *config, int direction)
{
    assert(api && setting == APRS_MUTE && direction == 1);
    config->mute ^= 1;
    return 0;
}

int aprs_settings_save(const struct h9_api *loader, const struct h9_runtime_api *api,
                       const struct ts_settings *config, struct ts_settings *saved)
{
    assert(loader && api);
    ++saves;
    if (fail_save) return 0;
    *saved = stored = *config;
    committed_at = tick;
    return 1;
}

static const struct h9_runtime_api service = { .abi = H9_RUNTIME_ABI, .read_status = runtime_status };
static const struct h9_runtime_prefix prefix = { .api = &service };
static const struct h9_api loader = { .version = 5, .read = read_bundle, .resident = &prefix };

struct press { unsigned at, key; };
static const struct press *keys;

unsigned h9_key(void)
{
    for (const struct press *p = keys; p->at; ++p)
        if (tick >= p->at && tick < p->at + 12) return p->key;
    return H9_KEY_NONE;
}

void h9_delay(unsigned units)
{
    tick += units;
    assert(tick < 2200);
}

void h9_usb_loader(void) { ++usb_updates; }
void gfx_wait_release(void) { while (h9_key()) h9_delay(2); }

void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    (void)color;
    assert(x + w <= H9_WIDTH && y + h <= H9_HEIGHT);
}

unsigned gfx_text(unsigned x, unsigned y, const char *text, unsigned max, unsigned scale, uint16_t fg, uint16_t bg)
{
    (void)fg;
    unsigned n = 0;
    while (n < max && text[n]) ++n;
    assert(x + n * 6 * scale <= H9_WIDTH && y + 8 * scale <= H9_HEIGHT);
    assert(strcmp(text, ".APRS LIST"));
    saw_edit |= !strcmp(text, "APRS MUTE") && bg == UI_EDIT;
    saw_settings |= !strcmp(text, "APRS SETTINGS");
    saw_failure |= !strcmp(text, "SETTINGS SAVE FAILED");
    saw_status |= !strcmp(text, "GPS FIX  BEACON DUE");
    return x + n * 6 * scale;
}

static unsigned run_menu(const struct press *sequence)
{
    keys = sequence;
    tick = 0;
    return toolbox_menu(&loader, sizeof names / sizeof *names);
}

int main(void)
{
    struct menu_list list = { .count = 0 };
    build_list(&list, &loader, sizeof names / sizeof *names);
    assert(list.count == 12 && list.options[0].index == 2 && list.options[1].index == 5);
    assert(list.options[9].group == DIAGNOSTICS && list.options[9].index == 7);
    omit_station_list = 1;
    list.count = list.row_count = 0;
    build_list(&list, &loader, sizeof names / sizeof *names);
    assert(list.count == 15 && list.options[2].group == APRS && list.options[2].index == 8);
    assert(list.options[3].group == APRS && list.options[3].index == 9);
    assert(list.options[4].group == APRS && list.options[4].index == 10);
    omit_station_list = 0;

    const struct press about[] = {{ 16, H9_KEY_1 }, { 36, H9_KEY_1 }, { 56, H9_KEY_MENU }, { 0, 0 }};
    assert(run_menu(about) == 1 && menu_selection == 10);
    const struct press again[] = {{ 16, H9_KEY_MENU }, { 0, 0 }};
    assert(run_menu(again) == 1);
    const struct press usb[] = {{ 16, H9_KEY_1 }, { 36, H9_KEY_2 }, { 56, H9_KEY_MENU }, { 0, 0 }};
    assert(run_menu(usb) == 0 && usb_updates == 1);

    enum { KEY_7 = 9 };
    const struct press mute[] = {
        { 16, KEY_7 }, { 36, H9_KEY_MENU }, { 56, H9_KEY_UP }, { 76, H9_KEY_BACK }, { 96, H9_KEY_BACK }, { 0, 0 }
    };
    assert(run_menu(mute) == 0 && saves == 1 && stored.mute && saw_edit && saw_settings && committed_at < 96);
    const struct press idle_edit[] = {
        { 16, H9_KEY_MENU }, { 36, H9_KEY_UP }, { 300, H9_KEY_BACK }, { 330, H9_KEY_BACK }, { 0, 0 }
    };
    assert(run_menu(idle_edit) == 0 && saves == 2 && !stored.mute && committed_at > 235 && committed_at < 300);
    fail_save = 1;
    assert(run_menu(mute) == 0 && saves == 3 && !stored.mute && saw_failure);

    const struct press idle[] = {{ 0, 0 }};
    assert(run_menu(idle) == 0 && tick == IDLE_WAIT);
    const struct press beacon[] = {{ 30, H9_KEY_MENU }, { 60, H9_KEY_UP }, { 90, H9_KEY_BACK }, { 0, 0 }};
    keys = beacon;
    tick = 0;
    assert(module_main(&loader, 5) == 0 && saw_status && saves == 3);
    puts("Toolbox grouping, digits, menu return, APRS saves, and read-only beacon status passed.");
}
