/* The resident build uses the grouped list and keypad flow of Limehawk's
 * TD-H3 menu. The H9 module names and journaled APRS settings are its rows. */
#include "app_theme.h"
#include "../aprs/aprs_resident_config.h"

enum { APRS, APRS_SETTINGS, TOOLS, APPS, DIAGNOSTICS, SETUP, GROUPS, APRS_PAGE };
enum { MAX_APPS = 64, MAX_ITEMS = MAX_APPS + APRS_SETTING_COUNT + 1, HEADING = 0x80 };
enum { ROW_Y = UI_TITLE_H, ROW_H = 14, VISIBLE = 6, ROW_WIDTH = 155, NO_DIGIT = 10 };
enum { DIGIT_WAIT = 100, SAVE_WAIT = 200, IDLE_WAIT = 2000 }; /* 100 Hz tick */
#ifndef MENU_TICK
#define MENU_TICK (*(const volatile uint32_t *)0x1D8B0u)
#endif
#ifndef MENU_SELECTION
/* Final word of the protected area cleared by ml_aprs_boot. The resident
 * build has retired the legacy APRS overlay, which used the area below it. */
#define MENU_SELECTION (*(volatile uint32_t *)0x29EFCu)
#endif

static const char *const groups[] = { "APRS", "APRS SETTINGS", "RADIO TOOLS", "APPS", "DIAGNOSTICS", "SETUP" };
static const char *const settings[] = { "SCHEDULE", "BEACON MODE", "INTERVAL", "SYMBOL", "APRS MUTE" };
static const char *const setting_help[] = {
    "Arm the beacon scheduler", "Smart or timed beacon", "Time between beacons",
    "Symbol sent in a beacon", "Mute packet audio on B"
};

struct tool_label { const char *name, *label, *help; uint8_t group; };
static const struct tool_label tools[] = {
    { "APRS", "STATIONS", "Stations, map and messages", APRS },
    { "APRS BEACON", "BEACON STATUS", "GPS and beacon status", APRS },
    { "APRS MAP", "MAP", "Plot heard stations", APRS_PAGE },
    { "APRS MSG", "MESSAGES", "Read APRS messages", APRS_PAGE },
    { "APRS SEND", "SEND MESSAGE", "Compose an APRS message", APRS_PAGE },
    { "GPS", "GPS", 0, TOOLS },
    { "SAT PASS", "SAT PASS", 0, TOOLS },
    { "SPECTRUM", "SPECTRUM", 0, TOOLS },
    { "ABOUT", "ABOUT", "Firmware information", SETUP }
};

struct menu_option { uint8_t group, index, row; };
struct menu_list {
    struct menu_option options[MAX_ITEMS];
    uint8_t rows[MAX_ITEMS + GROUPS];
    unsigned count, row_count;
};

struct menu_state {
    struct ts_settings saved, draft;
    const char *notice;
    uint32_t last_edit;
    unsigned selected, digit, editing, available, dirty;
};

static void read_name(const struct h9_api *api, unsigned index, char name[H9_NAME_BYTES + 1])
{
    api->read(name, api->bundle + H9_BUNDLE_HEADER + H9_BUNDLE_ENTRY * index, H9_NAME_BYTES);
    name[H9_NAME_BYTES] = 0;
}

static const char *after_prefix(const char *name, const char *prefix)
{
    while (*prefix && *name == *prefix) { ++name; ++prefix; }
    return *prefix ? 0 : name;
}

static const struct tool_label *named_tool(const char *name)
{
    for (unsigned i = 0; i < sizeof tools / sizeof *tools; ++i) {
        const char *tail = after_prefix(name, tools[i].name);
        if (tail && !*tail) return &tools[i];
    }
    return 0;
}

static unsigned text_length(const char *s)
{
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}

static unsigned app_group(const char *name, const struct tool_label *tool)
{
    if (tool) return tool->group;
    unsigned n = text_length(name);
    if (after_prefix(name, "APRS ") || after_prefix(name, "AUDIO ") ||
        (n > 6 && after_prefix(name + n - 6, " PROBE"))) return DIAGNOSTICS;
    return APPS;
}

static void add_option(struct menu_list *list, unsigned group, unsigned index)
{
    list->rows[list->row_count] = (uint8_t)list->count;
    list->options[list->count++] = (struct menu_option){ group, index, list->row_count++ };
}

static void build_list(struct menu_list *list, const struct h9_api *api, unsigned count)
{
    struct menu_option apps[MAX_APPS];
    unsigned shown = 0, launcher = 0, station_list = 0;
    for (unsigned i = 1; i < count; ++i) {
        char name[H9_NAME_BYTES + 1];
        read_name(api, i, name);
        if (name[0] == '.') {
            const char *tail = after_prefix(name, ".APRS LIST");
            if (tail && !*tail) station_list = 1;
        } else if (name[0] && shown < MAX_APPS) {
            const struct tool_label *tool = named_tool(name);
            if (tool == tools) launcher = 1;   /* the APRS entry opens .APRS LIST */
            apps[shown++] = (struct menu_option){ app_group(name, tool), i, 0 };
        }
    }
    for (unsigned group = 0; group < GROUPS; ++group) {
        unsigned heading = list->row_count;
        list->rows[list->row_count++] = (uint8_t)(HEADING | group);
        if (group == APRS_SETTINGS) {
            for (unsigned i = 0; i < APRS_SETTING_COUNT; ++i) add_option(list, group, i);
        } else {
            for (unsigned i = 0; i < shown; ++i) {
                unsigned app = apps[i].group;
                if (app == group || (group == APRS && app == APRS_PAGE && !(launcher && station_list)))
                    add_option(list, group, apps[i].index);
            }
            if (group == SETUP) add_option(list, group, 0);   /* USB UPDATE */
        }
        if (list->row_count == heading + 1) list->row_count = heading;
    }
}

static const char *draw_row(const struct h9_api *api, const struct menu_list *list,
                            const struct menu_state *state, unsigned row, unsigned y)
{
    if (row >= list->row_count) {
        gfx_fill(0, y, ROW_WIDTH, ROW_H, UI_BG);
        return 0;
    }
    unsigned item = list->rows[row];
    if (item & HEADING) {
        gfx_fill(0, y, ROW_WIDTH, ROW_H, UI_BG);
        unsigned x = gfx_text(3, y + 3, groups[item & ~HEADING], 20, 1, UI_DIM, UI_BG);
        gfx_text(x + 6, y + 3, "-------------------------", (ROW_WIDTH - x - 6) / 6, 1, UI_DIM, UI_BG);
        return 0;
    }
    struct menu_option option = list->options[item];
    char name[H9_NAME_BYTES + 1], minutes[8];
    const char *label = "USB UPDATE", *value = ">", *help = "Open USB firmware updater";
    if (option.group == APRS_SETTINGS) {
        label = settings[option.index];
        value = aprs_setting_value(option.index, state->available ? &state->draft : 0, minutes);
        help = setting_help[option.index];
    } else if (option.index) {
        read_name(api, option.index, name);
        const struct tool_label *tool = named_tool(name);
        label = tool ? tool->label : name;
        help = tool ? tool->help : 0;
    }
    unsigned selected = item == state->selected;
    uint16_t bg = selected ? (state->editing ? UI_EDIT : UI_ACCENT) : UI_BG;
    gfx_fill(0, y, ROW_WIDTH, ROW_H, bg);
    gfx_text(3, y + 3, label, H9_NAME_BYTES, 1, selected ? UI_BG : UI_TEXT, bg);
    unsigned n = text_length(value);
    gfx_text(ROW_WIDTH - 3 - n * 6, y + 3, value, n, 1, selected ? UI_BG : UI_ACCENT, bg);
    return selected ? help : 0;
}

static void draw_menu(const struct h9_api *api, const struct menu_list *list, const struct menu_state *state)
{
    struct menu_option option = list->options[state->selected];
    unsigned item = state->selected + 1;
    char count[] = { '0' + item / 10, '0' + item % 10, '/',
                     '0' + list->count / 10, '0' + list->count % 10, 0 };
    if (state->digit != NO_DIGIT) { count[0] = '0' + state->digit; count[1] = '-'; }
    gfx_fill(0, 0, H9_WIDTH, ROW_Y, UI_BG);
    gfx_text(UI_LEFT, UI_TITLE_Y, groups[option.group], 20, 1, UI_ACCENT, UI_BG);
    gfx_text(H9_WIDTH - 33, UI_TITLE_Y, count, 5, 1, UI_ACCENT, UI_BG);

    unsigned top = option.row < VISIBLE / 2 ? 0 : option.row - VISIBLE / 2;
    unsigned limit = list->row_count > VISIBLE ? list->row_count - VISIBLE : 0;
    if (top > limit) top = limit;
    const char *help = 0;
    for (unsigned i = 0; i < VISIBLE; ++i) {
        const char *row_help = draw_row(api, list, state, top + i, ROW_Y + i * ROW_H);
        if (row_help) help = row_help;   /* headings and other rows never return help */
    }
    unsigned track = VISIBLE * ROW_H;
    gfx_fill(ROW_WIDTH + 1, ROW_Y, 2, track, UI_DIM);
    gfx_fill(ROW_WIDTH + 1, ROW_Y + (track - 5) * state->selected / (list->count - 1), 2, 5, UI_ACCENT);
    gfx_fill(0, ROW_Y + VISIBLE * ROW_H, H9_WIDTH, H9_HEIGHT - ROW_Y - VISIBLE * ROW_H, UI_BG);
    if (state->notice || help)
        gfx_text(3, 104, state->notice ? state->notice : help, 26, 1,
                 state->notice ? UI_ACCENT : UI_HINT, UI_BG);
    gfx_text(UI_LEFT, UI_FOOT_Y, state->editing ? "MENU DONE   UP/DN CHANGE" : "MENU PICK   BACK RADIO", UI_CELLS, 1,
             UI_TEXT, UI_BG);
}

static int read_settings(const struct h9_runtime_api *api, struct menu_state *state)
{
    struct h9_runtime_status status;
    if (!api || !api->read_status(&status)) return 0;
    state->saved = status.settings;
    state->draft = state->saved;
    return 1;
}

static void save_pending(const struct h9_api *loader, const struct h9_runtime_api *api, struct menu_state *state)
{
    if (!state->dirty) return;
    if (!aprs_settings_save(loader, api, &state->draft, &state->saved)) {
        state->draft = state->saved;
        state->notice = "SETTINGS SAVE FAILED";
    }
    state->dirty = 0;
}

static void select_number(struct menu_state *state, unsigned count, unsigned number)
{
    state->digit = NO_DIGIT;
    if (number && number <= count) state->selected = number - 1;
    else state->notice = "NO SUCH ITEM";
}

unsigned toolbox_menu(const struct h9_api *loader, unsigned count)
{
    struct menu_list list;
    list.count = list.row_count = 0;
    struct menu_state state = { .digit = NO_DIGIT };
    const struct h9_runtime_api *api = aprs_runtime(loader);
    build_list(&list, loader, count);
    unsigned saved_row = MENU_SELECTION;
    if (saved_row < list.count) state.selected = saved_row;
    state.available = read_settings(api, &state);
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    draw_menu(loader, &list, &state);
    gfx_wait_release();
    uint32_t last_key = MENU_TICK;
    unsigned sample = H9_KEY_NONE, stable = H9_KEY_NONE;
    for (;;) {
        h9_delay(2);
        uint32_t now = MENU_TICK;
        int redraw = 0;
        if (state.dirty && now - state.last_edit >= SAVE_WAIT) {
            save_pending(loader, api, &state);
            redraw = 1;
        }
        if (state.digit != NO_DIGIT && now - last_key >= DIGIT_WAIT) {
            select_number(&state, list.count, state.digit);
            redraw = 1;
        }
        if (now - last_key >= IDLE_WAIT) return 0;

        unsigned key = h9_key();
        if (key == sample && key != stable) {
            stable = key;
            if (key != H9_KEY_NONE) {
                last_key = now;
                redraw = 1;
                state.notice = 0;
                if (key >= 2 && key <= 11) {   /* digit d = d + 2 */
                    unsigned digit = key - 2;
                    save_pending(loader, api, &state);
                    state.editing = 0;
                    if (state.digit != NO_DIGIT) select_number(&state, list.count, 10 * state.digit + digit);
                    else if (digit > list.count / 10) select_number(&state, list.count, digit);
                    else state.digit = digit;
                } else {
                    state.digit = NO_DIGIT;
                    struct menu_option option = list.options[state.selected];
                    if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
                        int direction = key == H9_KEY_UP ? 1 : -1;
                        if (state.editing) {
                            state.notice = aprs_setting_adjust(api, option.index, &state.draft, direction);
                            if (!state.notice) {
                                state.dirty = 1;
                                state.last_edit = now;
                            }
                        } else if (direction > 0) {
                            state.selected = state.selected ? state.selected - 1 : list.count - 1;
                        } else {
                            state.selected = (state.selected + 1) % list.count;
                        }
                    } else if (state.editing && (key == H9_KEY_MENU || key == H9_KEY_BACK)) {
                        save_pending(loader, api, &state);
                        state.editing = 0;
                    } else if (key == H9_KEY_BACK) {
                        return 0;
                    } else if (key == H9_KEY_MENU && option.group == APRS_SETTINGS) {
                        if (!state.available) state.available = read_settings(api, &state);
                        if (!state.available) state.notice = "SETTINGS UNAVAILABLE";
                        else if (option.index == APRS_INTERVAL && !state.draft.timed) state.notice = "TIMED MODE ONLY";
                        else state.editing = 1;
                    } else if (key == H9_KEY_MENU) {
                        if (option.index) return option.index;
                        h9_usb_loader();
                        return 0;
                    }
                }
            }
        }
        sample = key;
        if (redraw) {
            MENU_SELECTION = state.selected;
            draw_menu(loader, &list, &state);
        }
    }
}
