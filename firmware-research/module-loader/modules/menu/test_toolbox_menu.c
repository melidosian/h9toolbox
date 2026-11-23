#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static unsigned menu_selection;
#define H9_MENU_POSITION menu_selection
#include "h9mod.h"

static uint32_t tick;
#define MENU_TICK tick
#define MENU_SELECTION menu_selection
#if defined(H9_RADIO_MENU_PREVIEW) || defined(H9_RADIO_SETTINGS)
static max_align_t preview_list_storage[2048 / sizeof(max_align_t) + 1];
#define MENU_LIST_STORAGE ((struct menu_list *)preview_list_storage)
#endif
#include "toolbox_menu.c"

#ifdef H9_RADIO_SETTINGS
static unsigned applies, radio_edits, radio_writes, action_calls, last_action, fail_apply;
static unsigned has_categories, omit_aprs, saw_aprs_unavailable;
static unsigned saw_home, saw_radio, saw_general, saw_aprs, saw_blue, saw_yellow, saw_pink;
static uint16_t last_title_color, last_action_accent;
static uint8_t radio_image[0x6100];
static int radio_read(unsigned offset, void *dst, unsigned n)
{ assert(offset + n <= sizeof radio_image); memcpy(dst, radio_image + offset, n); return 0; }
static int radio_write(unsigned offset, const void *src, unsigned n)
{ assert(offset + n <= sizeof radio_image); memcpy(radio_image + offset, src, n); ++radio_writes; return 0; }
const struct radio_storage radio_stock_storage = { .read = radio_read, .write = radio_write };
int aprs_select_tx(struct ts_settings *next) { next->tx_frequency = 14439000; return 1; }
int radio_editor(const struct radio_storage *storage, const struct radio_setting *setting)
{ assert(storage == &radio_stock_storage && setting->kind == RADIO_TEXT); ++radio_edits; return -1; }
int radio_channel_editor(const struct radio_storage *storage, uint16_t accent) { (void)accent; assert(storage); return 0; }
void h9_reset(void) { assert(!"unified menu must apply without CPU reset"); }
int radio_apply_pending(const struct h9_api *api) { assert(api); ++applies; return fail_apply ? RADIO_BUSY : RADIO_OK; }
int radio_action(const struct h9_api *api, unsigned action, uint16_t accent)
{ assert(api && action < RADIO_ACTION_COUNT); last_action = action; last_action_accent = accent; ++action_calls; return 0; }
#endif

#define BEACON_TICK tick
#include "../aprs/aprs_resident_status.c"

static const char names[][H9_NAME_BYTES] = {
    "MENU", "ABOUT", "APRS", ".APRS LIST", "GPS", "APRS BEACON", "CUSTOM APP", "APRS RAW",
    "APRS MAP", "APRS MSG", "APRS SEND"
#ifdef H9_RADIO_SETTINGS
    , "MAZE CHASE"
#endif
};
static unsigned saves, committed_at, fail_save, saw_edit, saw_settings, saw_failure, saw_status, usb_updates;
#ifdef H9_RADIO_MENU_PREVIEW
static unsigned saw_unwired;
#endif
static unsigned omit_station_list;
static struct ts_settings stored;

static void read_bundle(void *dst, uint32_t address, uint32_t length)
{
#ifdef H9_RADIO_SETTINGS
    unsigned entries = sizeof names / sizeof *names;
    unsigned trailer = H9_BUNDLE_HEADER + H9_BUNDLE_ENTRY * entries + 32;
    if (address == 0 && length == H9_BUNDLE_HEADER) {
        unsigned size = has_categories ? trailer + 8 + entries : 0;
        uint8_t header[H9_BUNDLE_HEADER] = { [10] = entries, [12] = size, [13] = size >> 8 };
        memcpy(dst, header, length);
        return;
    }
    if (has_categories && address == trailer && length == 8) { memcpy(dst, "H9MENU01", length); return; }
    if (has_categories && address >= trailer + 8 && address < trailer + 8 + entries && length == 1) {
        *(uint8_t *)dst = address == trailer + 8 + 6 ? 2 : 0;
        return;
    }
#endif
    assert(address >= H9_BUNDLE_HEADER && length == H9_NAME_BYTES);
    unsigned index = (address - H9_BUNDLE_HEADER) / H9_BUNDLE_ENTRY;
    assert(index < sizeof names / sizeof *names);
#ifdef H9_RADIO_SETTINGS
    if (omit_aprs && index == 2) { memset(dst, 0, length); return; }
#endif
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

struct press { unsigned at, key, duration; };
static const struct press *keys;

unsigned h9_key(void)
{
    for (const struct press *p = keys; p->at; ++p)
        if (tick >= p->at && tick < p->at + (p->duration ? p->duration : 12)) return p->key;
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
#ifdef H9_RADIO_SETTINGS
    saw_aprs_unavailable |= !strcmp(text, "APRS CLIENT UNAVAILABLE");
    if (y == UI_TITLE_Y && x == UI_LEFT) {
        if (!strcmp(text, "H9 TOOLBOX")) saw_home = 1;
        if (!strcmp(text, "RADIO SETTINGS")) saw_radio = 1;
        if (!strcmp(text, "GENERAL")) saw_general = 1;
        if (!strcmp(text, "APRS SETTINGS")) saw_settings = 1;
        if (!strcmp(text, "APRS")) saw_aprs = 1;
        last_title_color = fg;
    }
    if (y >= ROW_Y && y < ROW_Y + VISIBLE * ROW_H && x == 3 && !strcmp(text, "Menu color")) {
        if (bg && bg != UI_BG) {
            if (bg == menu_color(0)) saw_blue = fg == UI_TEXT;
            if (bg == menu_color(3)) saw_yellow = fg == UI_BG;
            if (bg == menu_color(12)) saw_pink = fg == UI_BG;
        }
    }
#else
    (void)fg;
#endif
    unsigned n = 0;
    while (n < max && text[n]) ++n;
    assert(x + n * 6 * scale <= H9_WIDTH && y + 8 * scale <= H9_HEIGHT);
    assert(strcmp(text, ".APRS LIST"));
    saw_edit |= !strcmp(text, "APRS MUTE") && bg == UI_EDIT;
    saw_settings |= !strcmp(text, "APRS SETTINGS");
    saw_failure |= !strcmp(text, "SETTINGS SAVE FAILED");
    saw_status |= !strcmp(text, "GPS FIX  BEACON DUE");
#ifdef H9_RADIO_MENU_PREVIEW
    saw_unwired |= !strcmp(text, "STOCK CONTROL NOT WIRED");
#endif
    return x + n * 6 * scale;
}

static unsigned run_menu(const struct press *sequence)
{
    keys = sequence;
    tick = 0;
    return toolbox_menu(&loader, sizeof names / sizeof *names);
}

#ifdef H9_RADIO_SETTINGS
static unsigned row_position(unsigned page, unsigned row)
{
    return MENU_POSITION_MAGIC | page << 8 | row;
}

static unsigned position_of(const struct menu_list *list, unsigned index, int action)
{
    for (unsigned i = 0; i < list->count; ++i) {
        struct menu_option option = list->options[i];
        if (option.index == index && option.action == action && (action || option.group >= RADIO_FIRST))
            return row_position(option.group, i - list->sections[option.group].first);
    }
    assert(!"menu row missing");
    return 0;
}

static unsigned count_reachable(const struct menu_list *list, unsigned page, unsigned *seen)
{
    unsigned bit = 1u << page;
    assert(!(*seen & bit));
    *seen |= bit;
    struct menu_view view = menu_view(list, page);
    if (page < MENU_GROUPS) return view.count;
    unsigned count = 0;
    for (unsigned i = 0; i < view.count; ++i) count += count_reachable(list, view.children[i], seen);
    return count;
}

static void test_unified(const struct menu_list *list)
{
    unsigned seen = 0;
    assert(count_reachable(list, MENU_HOME, &seen) == list->count);
    struct menu_view home = menu_view(list, MENU_HOME);
    assert(home.count == 8 && home.children[0] == RADIO_FOLDER &&
        home.children[1] == CHANNELS && home.children[2] == APRS_FOLDER);
    assert(menu_view(list, RADIO_FOLDER).count == 6);
    assert(menu_view(list, APRS_FOLDER).count == 5);
    assert(menu_view(list, MESSAGES).count == 4);
    assert(list->sections[GAMES].count == 1 && menu_option(list, GAMES, 0).index == 11);

    const struct press from_home[] = {
        {16, H9_KEY_MENU, 65}, {96, H9_KEY_MENU}, {116, H9_KEY_MENU}, {136, H9_KEY_5},
        {156, H9_KEY_MENU}, {176, H9_KEY_BACK, 65}, {256, H9_KEY_BACK}, {276, H9_KEY_BACK}, {0, 0}
    };
    assert(run_menu(from_home) == 0 && saw_home && saw_radio && saw_general);
    assert(menu_selection == row_position(MENU_HOME, 0) && radio_edits == 0 && applies == 1);
    assert(radio_image[radio_settings[0].offset] == 5 && radio_writes == 1);

    const struct press hold_down[] = {{16, H9_KEY_DOWN, 72}, {0, 0}};
    assert(run_menu(hold_down) == 0 && menu_selection == row_position(MENU_HOME, 3));
    const struct press hold_up[] = {{16, H9_KEY_UP, 72}, {0, 0}};
    assert(run_menu(hold_up) == 0 && menu_selection == row_position(MENU_HOME, 0));
    const struct press reverse[] = {{16, H9_KEY_DOWN, 72}, {88, H9_KEY_UP}, {106, H9_KEY_BACK}, {0, 0}};
    assert(run_menu(reverse) == 0 && menu_selection == row_position(MENU_HOME, 2));
    const struct press short_up[] = {{16, H9_KEY_UP}, {36, H9_KEY_BACK}, {0, 0}};
    assert(run_menu(short_up) == 0 && menu_selection == row_position(MENU_HOME, 1));
    const struct press single_app[] = {{16, H9_KEY_MENU}, {36, H9_KEY_MENU}, {0, 0}};
    menu_selection = row_position(MENU_HOME, 5);
    assert(run_menu(single_app) == 6 && menu_selection == row_position(APPS, 0));
    const struct press again[] = {{16, H9_KEY_MENU}, {0, 0}};
    assert(run_menu(again) == 6 && menu_selection == row_position(APPS, 0));

    const struct press about[] = {{16, H9_KEY_1 + 7}, {36, H9_KEY_MENU}, {56, H9_KEY_MENU}, {0, 0}};
    menu_selection = row_position(MENU_HOME, 0);
    assert(run_menu(about) == 1 && menu_selection == row_position(SETUP, 0));
    const struct press usb[] = {{16, H9_KEY_2}, {36, H9_KEY_MENU}, {0, 0}};
    assert(run_menu(usb) == 0 && usb_updates == 1);

    const struct press mute[] = {
        {16, H9_KEY_MENU}, {36, H9_KEY_UP}, {300, H9_KEY_MENU},
        {330, H9_KEY_BACK}, {350, H9_KEY_BACK}, {370, H9_KEY_BACK}, {0, 0}
    };
    unsigned mute_position = position_of(list, APRS_MUTE, APRS_CONTROL);
    assert((mute_position >> 8 & 255) == APRS_CONFIG_PAGE);
    menu_selection = mute_position;
    assert(run_menu(mute) == 0 && saves == 1 && stored.mute && saw_edit && saw_settings && saw_aprs);
    assert(committed_at >= 300 && menu_selection == row_position(MENU_HOME, 2));
    fail_save = 1;
    menu_selection = mute_position;
    assert(run_menu(mute) == 0 && saves == 2 && stored.mute && saw_failure);
    fail_save = 0;
    const struct press idle[] = {{0, 0}};
    menu_selection = mute_position;
    assert(run_menu(idle) == 0 && tick == IDLE_WAIT);
    assert(menu_selection == mute_position && saves == 2);

    /* The native dispatcher reopens the saved page after an idle exit. */
    saw_settings = 0;
    const struct press reopen[] = {{16, H9_KEY_BACK}, {0, 0}};
    assert(run_menu(reopen) == 0 && saw_settings);
    assert(menu_selection != mute_position && saves == 2);

    const struct press settings_inline[] = {
        {16, H9_KEY_MENU}, {36, H9_KEY_UP, 70}, {116, H9_KEY_MENU},
        {136, H9_KEY_BACK}, {156, H9_KEY_BACK}, {176, H9_KEY_BACK}, {0, 0}
    };
    menu_selection = position_of(list, 0, 0);
    assert(run_menu(settings_inline) == 0 && radio_edits == 0 && applies == 2 && radio_writes == 2);
    assert(radio_image[radio_settings[0].offset] == 6);

    for (unsigned digit = 0; digit < 10; ++digit) {
        const struct press keypad[] = {
            {16, H9_KEY_MENU}, {36, digit + 2}, {56, H9_KEY_MENU},
            {76, H9_KEY_BACK}, {96, H9_KEY_BACK}, {116, H9_KEY_BACK}, {0, 0}
        };
        menu_selection = position_of(list, 0, 0);
        assert(run_menu(keypad) == 0 && radio_edits == 0);
        assert(radio_image[radio_settings[0].offset] == digit);
    }
    assert(radio_writes == 12 && applies == 12);

    unsigned ranger = 0;
    while (strcmp(radio_settings[ranger].id, "rangerHigh")) ++ranger;
    menu_selection = position_of(list, ranger, 0);
    const struct press multidigit[] = {
        {16, H9_KEY_MENU}, {36, 6}, {56, 5}, {76, 10}, {96, H9_KEY_MENU},
        {116, H9_KEY_BACK}, {136, H9_KEY_BACK}, {156, H9_KEY_BACK}, {0, 0}
    };
    assert(run_menu(multidigit) == 0 && radio_edits == 0);
    unsigned offset = radio_settings[ranger].offset;
    assert((radio_image[offset] | radio_image[offset + 1] << 8) == 438 && radio_writes == 13);

    unsigned color_position = position_of(list, RADIO_MENU_COLOR, 0);
    unsigned color_offset = radio_settings[RADIO_MENU_COLOR].offset;
    const struct press color_cancel[] = {
        {16, H9_KEY_MENU}, {36, H9_KEY_3}, {56, H9_KEY_BACK},
        {76, H9_KEY_BACK}, {96, H9_KEY_BACK}, {116, H9_KEY_BACK}, {0, 0}
    };
    menu_selection = color_position;
    assert(run_menu(color_cancel) == 0 && saw_blue && saw_yellow);
    assert(radio_image[color_offset] == 0 && radio_writes == 13 && last_title_color == menu_ink(menu_color(0)));
    const struct press color_save[] = {
        {16, H9_KEY_MENU}, {36, H9_KEY_1}, {56, H9_KEY_2}, {76, H9_KEY_MENU},
        {96, H9_KEY_BACK}, {116, H9_KEY_BACK}, {136, H9_KEY_BACK}, {0, 0}
    };
    menu_selection = color_position;
    assert(run_menu(color_save) == 0 && saw_pink && radio_image[color_offset] == 12 && radio_writes == 14);
    assert(last_title_color == menu_ink(menu_color(12)));
    assert(run_menu(idle) == 0 && last_title_color == menu_ink(menu_color(12)));
    for (unsigned action = 0; action < RADIO_ACTION_COUNT; ++action) {
        menu_selection = action == RADIO_ACT_CHANNELS ? row_position(MENU_HOME, 1) : position_of(list, action, 1);
        const struct press activate[] = {
            {16, H9_KEY_MENU}, {56, H9_KEY_BACK}, {76, H9_KEY_BACK}, {96, H9_KEY_BACK}, {0, 0}
        };
        assert(run_menu(activate) == 0 && last_action == action && action_calls == action + 1);
        assert(last_action_accent == menu_color(12));
    }
    const struct press beacon[] = {{30, H9_KEY_MENU}, {60, H9_KEY_UP}, {90, H9_KEY_BACK}, {0, 0}};
    keys = beacon;
    tick = 0;
    menu_selection = 0;
    assert(module_main(&loader, 5) == 0 && saw_status && saves == 2);
    for (unsigned route = H9_MSG_NEW; route <= H9_MSG_DRAFT; ++route) {
        menu_selection = position_of(list, route, APRS_MESSAGE);
        assert(run_menu(again) == 2 && tick < 36);
        assert(((menu_selection & H9_MSG_ROUTE_MASK) >> H9_MSG_ROUTE_SHIFT) == route);
        assert(((menu_selection >> 8) & 255u) == MESSAGES);
    }
    omit_aprs = 1;
    const struct press unavailable[] = {
        {16,H9_KEY_MENU}, {36,H9_KEY_BACK}, {56,H9_KEY_BACK}, {76,H9_KEY_BACK}, {0,0}
    };
    assert(run_menu(unavailable) == 0 && saw_aprs_unavailable);
    omit_aprs = 0;
    toolbox_open_messages();
    assert(menu_selection == row_position(MESSAGES, 0));

    unsigned beacon_position = position_of(list, APRS_BEACON_TYPE, APRS_CONTROL);
    assert(((beacon_position >> 8) & 255u) == APRS_BEACON_PAGE);
    const struct press select_type[] = {
        {16,H9_KEY_MENU}, {36,H9_KEY_UP}, {56,H9_KEY_MENU},
        {76,H9_KEY_BACK}, {96,H9_KEY_BACK}, {116,H9_KEY_BACK}, {0,0}
    };
    for (unsigned type = BEACON_TIMED; type <= BEACON_SMART; ++type) {
        menu_selection = beacon_position;
        assert(run_menu(select_type) == 0);
        assert(radio_beacon_type(&radio_stock_storage, &stored) == type);
        assert(radio_image[0x30ee] == (type == BEACON_PTT));
        assert(radio_image[0x30ef] == (type == BEACON_TIMED));
        assert(stored.armed == (type == BEACON_SMART));
    }
    menu_selection = beacon_position;
    assert(run_menu(select_type) == 0 && !stored.armed);
    assert(radio_beacon_type(&radio_stock_storage, &stored) == BEACON_OFF);

    stored.armed = stored.timed = 1;
    stored.minutes = 12;
    assert(!radio_beacon_save(&radio_stock_storage, &loader, &service, &stored, BEACON_TIMED));
    assert(!stored.armed && radio_image[0x30ef] == 1);
    assert((radio_image[0x30f0] | radio_image[0x30f1] << 8) == 720);

    fail_save = 1;
    assert(radio_beacon_save(&radio_stock_storage, &loader, &service, &stored, BEACON_SMART));
    assert(!stored.armed && radio_image[0x30ef] == 1);
    assert((radio_image[0x30f0] | radio_image[0x30f1] << 8) == 720);
    fail_save = 0;

    struct menu_list categorized;
    has_categories = 1;
    build_list(&categorized, &loader, sizeof names / sizeof *names);
    assert(categorized.sections[GAMES].count == 2 && !categorized.sections[APPS].count);
}
#endif

int main(void)
{
    struct menu_list list = { .count = 0 };
    build_list(&list, &loader, sizeof names / sizeof *names);
#ifdef H9_RADIO_SETTINGS
    assert(list.count == RADIO_SETTING_COUNT + RADIO_ACTION_COUNT + 14);
    assert(!strcmp(radio_settings[menu_option(&list, RADIO_FIRST, 0).index].id, "squelch"));
#elif defined(H9_RADIO_MENU_PREVIEW)
    assert(list.count == 122 && list.options[12].group == RADIO_FIRST);
    assert(!strcmp(radio_categories[0].items[0], "Squelch Lv"));
    assert(!strcmp(radio_categories[9].items[6], "APRS Reset"));
#else
    assert(list.count == 12);
#endif
    assert(list.options[0].index == 2 && list.options[1].index == 5);
#ifdef H9_RADIO_SETTINGS
    assert(menu_option(&list, DIAGNOSTICS, 0).index == 7);
#else
    assert(list.options[9].group == DIAGNOSTICS && list.options[9].index == 7);
#endif
    omit_station_list = 1;
    build_list(&list, &loader, sizeof names / sizeof *names);
#ifdef H9_RADIO_SETTINGS
    assert(list.count == RADIO_SETTING_COUNT + RADIO_ACTION_COUNT + 17);
#elif defined(H9_RADIO_MENU_PREVIEW)
    assert(list.count == 125);
#else
    assert(list.count == 15);
#endif
    assert(list.options[2].group == APRS && list.options[2].index == 8);
    assert(list.options[3].group == APRS && list.options[3].index == 9);
    assert(list.options[4].group == APRS && list.options[4].index == 10);
    omit_station_list = 0;

#ifdef H9_RADIO_SETTINGS
    build_list(&list, &loader, sizeof names / sizeof *names);
    test_unified(&list);
#else
    enum { ABOUT_SECOND_DIGIT = H9_KEY_1, ABOUT_INDEX = 10, USB_SECOND_DIGIT = H9_KEY_2 };
#define EDIT_SAVE_KEY H9_KEY_BACK

    const struct press about[] = {{ 16, H9_KEY_1 }, { 36, ABOUT_SECOND_DIGIT }, { 56, H9_KEY_MENU }, { 0, 0 }};
    assert(run_menu(about) == 1 && menu_selection == ABOUT_INDEX);
    const struct press again[] = {{ 16, H9_KEY_MENU }, { 0, 0 }};
    assert(run_menu(again) == 1);
    const struct press usb[] = {{ 16, H9_KEY_1 }, { 36, USB_SECOND_DIGIT }, { 56, H9_KEY_MENU }, { 0, 0 }};
    assert(run_menu(usb) == 0 && usb_updates == 1);

    enum { MUTE_DIGIT = 9 };
    const struct press mute[] = {
        { 16, MUTE_DIGIT }, { 36, H9_KEY_MENU }, { 56, H9_KEY_UP }, { 76, EDIT_SAVE_KEY }, { 96, H9_KEY_BACK }, { 0, 0 }
    };
    assert(run_menu(mute) == 0 && saves == 1 && stored.mute && saw_edit && saw_settings && committed_at < 96);
    const struct press idle_edit[] = {
        { 16, H9_KEY_MENU }, { 36, H9_KEY_UP }, { 300, EDIT_SAVE_KEY }, { 330, H9_KEY_BACK }, { 0, 0 }
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
#endif
#ifdef H9_RADIO_MENU_PREVIEW
    menu_selection = 12;
    const struct press radio[] = {{ 16, H9_KEY_MENU }, { 36, H9_KEY_BACK }, { 0, 0 }};
    assert(run_menu(radio) == 0 && saw_unwired && menu_selection == 12);
    const struct press jump[] = {
        { 16, H9_KEY_1 }, { 36, H9_KEY_1 }, { 56, H9_KEY_1 - 1 },
        { 76, H9_KEY_MENU }, { 96, H9_KEY_BACK }, { 0, 0 }
    };
    assert(run_menu(jump) == 0 && menu_selection == 109 && usb_updates == 1);
#endif
    puts("Toolbox grouping, digits, menu return, APRS saves, and read-only beacon status passed.");
}
