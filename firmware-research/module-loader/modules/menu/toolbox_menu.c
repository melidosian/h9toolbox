/* The resident build uses the grouped list and keypad flow of Limehawk's
 * TD-H3 menu. The unified build organizes radio and Toolbox rows in folders. */
#include "app_theme.h"
#include "../aprs/aprs_resident_config.h"
#ifdef H9_RADIO_MENU_PREVIEW
#include "../../menu-replacement/stock_menu_catalog.h"
#endif
#ifdef H9_RADIO_SETTINGS
#include "../../menu-replacement/radio_settings_catalog.h"
#include "../../menu-replacement/radio_actions.h"
#include "../../menu-replacement/radio_system.h"
#include "../../menu-replacement/radio_aprs.h"
#endif

enum {
    APRS, APRS_SETTINGS, TOOLS, APPS, DIAGNOSTICS, SETUP,
#ifdef H9_RADIO_SETTINGS
    CHANNELS, GAMES, MESSAGES,
#endif
    GROUPS, APRS_PAGE
};
#ifdef H9_RADIO_SETTINGS
enum { RADIO_FIRST = GROUPS, RADIO_GROUPS = GROUPS + RADIO_SETTING_GROUP_COUNT };
#define RADIO_ROWS (RADIO_SETTING_COUNT + RADIO_ACTION_COUNT + H9_MSG_DRAFT)
#define MENU_GROUPS RADIO_GROUPS
enum { MENU_HOME = MENU_GROUPS, RADIO_FOLDER, APRS_FOLDER, FOLDER_COUNT };
enum { REPEAT_DELAY = 35, REPEAT_PERIOD = 10 };
enum { MENU_ACTION = 1, APRS_CONTROL, APRS_MESSAGE };
enum { APRS_CONFIG_PAGE = RADIO_FIRST + RADIO_GROUP_APRS_IDENTITY,
       APRS_BEACON_PAGE = RADIO_FIRST + RADIO_GROUP_APRS_BEACON };
#define MENU_POSITION_MAGIC H9_MENU_MAGIC
#elif defined(H9_RADIO_MENU_PREVIEW)
enum { RADIO_FIRST = GROUPS, RADIO_GROUPS = GROUPS + RADIO_CATEGORY_COUNT };
#define RADIO_ROWS RADIO_ITEM_COUNT
#define MENU_GROUPS RADIO_GROUPS
#else
#define RADIO_ROWS 0
#define MENU_GROUPS GROUPS
#endif
enum { MAX_APPS = 64, MAX_ITEMS = MAX_APPS + APRS_SETTING_COUNT + 1 + RADIO_ROWS, HEADING = 0x8000 };
#if defined(H9_RADIO_MENU_PREVIEW) || defined(H9_RADIO_SETTINGS)
enum { ROW_Y = UI_TITLE_H, ROW_H = 14, VISIBLE = 6, ROW_WIDTH = 155, NO_DIGIT = 1000 };
#else
enum { ROW_Y = UI_TITLE_H, ROW_H = 14, VISIBLE = 6, ROW_WIDTH = 155, NO_DIGIT = 10 };
#endif
enum { DIGIT_WAIT = 100, SAVE_WAIT = 200, IDLE_WAIT = 2000 }; /* 100 Hz tick */
#ifndef MENU_TICK
#define MENU_TICK (*(const volatile uint32_t *)0x1D8B0u)
#endif
#ifndef MENU_SELECTION
/* Final word of the protected area cleared by ml_aprs_boot. The resident
 * build has retired the legacy APRS overlay, which used the area below it. */
#define MENU_SELECTION H9_MENU_POSITION
#endif

static const char *const groups[] = {
    "APRS", "APRS SETTINGS", "RADIO TOOLS", "APPS", "DIAGNOSTICS", "SETUP",
#ifdef H9_RADIO_SETTINGS
    "CHANNEL SETTINGS", "GAMES", "MESSAGES",
#endif
};
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

struct menu_option { uint8_t group, index, row, action; };
struct menu_list {
    struct menu_option options[MAX_ITEMS];
    unsigned count;
#ifdef H9_RADIO_SETTINGS
    struct menu_section { uint8_t first, count; } sections[MENU_GROUPS];
    unsigned aprs_launcher;
#else
    uint16_t rows[MAX_ITEMS + MENU_GROUPS];
    unsigned row_count;
#endif
};
#ifdef H9_RADIO_SETTINGS
_Static_assert(MAX_ITEMS <= 255, "menu row index needs widening");
_Static_assert(0x29F20u + sizeof(struct menu_list) <= 0x2B000u, "menu list exceeds its scratch window");
#endif

struct menu_state {
    struct ts_settings saved, draft;
    const char *notice;
    uint32_t last_edit;
    unsigned selected, digit, editing, available, dirty;
#ifdef H9_RADIO_SETTINGS
    unsigned radio_changed, page, beacon_type, beacon_draft;
    uint16_t accent;
    struct radio_edit radio_draft;
    unsigned number, have_number, number_invalid;
#endif
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

#ifdef H9_RADIO_SETTINGS
static unsigned bundle_menu_categories(const struct h9_api *api, unsigned count)
{
    uint8_t header[H9_BUNDLE_HEADER], signature[8];
    api->read(header, api->bundle, sizeof header);
    uint32_t length = header[12] | (uint32_t)header[13] << 8 |
                      (uint32_t)header[14] << 16 | (uint32_t)header[15] << 24;
    if (length < H9_BUNDLE_HEADER + (H9_BUNDLE_ENTRY + 1) * count + sizeof signature ||
        length > 0x1f000u) return 0;
    unsigned address = api->bundle + length - count - sizeof signature;
    api->read(signature, address, sizeof signature);
    for (unsigned i = 0; i < sizeof signature; ++i)
        if (signature[i] != "H9MENU01"[i]) return 0;
    return address + sizeof signature;
}

static unsigned category_group(unsigned category)
{
    static const uint8_t menu_groups[] = { APPS, APPS, GAMES, TOOLS, DIAGNOSTICS, SETUP, APRS };
    return category < sizeof menu_groups ? menu_groups[category] : APPS;
}
#endif

static unsigned app_group(const char *name, const struct tool_label *tool)
{
    if (tool) return tool->group;
#ifdef H9_RADIO_SETTINGS
    const char *maze = after_prefix(name, "MAZE CHASE");
    if (maze && !*maze) return GAMES;  /* bundles without category metadata */
#endif
    unsigned n = text_length(name);
    if (after_prefix(name, "APRS ") || after_prefix(name, "AUDIO ") ||
        (n > 6 && after_prefix(name + n - 6, " PROBE"))) return DIAGNOSTICS;
    return APPS;
}

static void add_option(struct menu_list *list, unsigned group, unsigned index)
{
#ifdef H9_RADIO_SETTINGS
    struct menu_section *section = &list->sections[group];
    if (!section->count) section->first = list->count;
    ++section->count;
    unsigned row = 0;
#else
    list->rows[list->row_count] = (uint16_t)list->count;
    unsigned row = list->row_count++;
#endif
    list->options[list->count++] = (struct menu_option){ group, index, row, 0 };
}

#ifdef H9_RADIO_SETTINGS
static int same_text(const char *a, const char *b)
{ while (*a && *a == *b) { ++a; ++b; } return *a == *b; }
const struct radio_setting *radio_setting_by_id(const char *id)
{
    for (unsigned i = 0; i < RADIO_SETTING_COUNT; ++i)
        if (same_text(id, radio_settings[i].id)) return &radio_settings[i];
    return 0;
}

static const char *const message_labels[] = { "NEW MESSAGE", "INBOX", "SENT", "DRAFTS" };
static const char *const message_help[] = {
    "Compose an APRS message", "Read received messages", "Read sent messages", "Open the APRS draft"
};
_Static_assert(sizeof message_labels / sizeof *message_labels == H9_MSG_DRAFT, "message routes");

static void add_aprs_control(struct menu_list *list, unsigned group, unsigned index)
{
    add_option(list, group, index);
    list->options[list->count - 1].action = APRS_CONTROL;
}
static void add_actions(struct menu_list *list, unsigned group, const char *name)
{
    for (unsigned i = 0; i < RADIO_ACTION_COUNT; ++i) if (same_text(radio_actions[i].group, name)) {
        add_option(list, group, i);
        list->options[list->count - 1].action = MENU_ACTION;
    }
}
#endif

static const char *group_name(unsigned group)
{
#ifdef H9_RADIO_SETTINGS
    if (group >= RADIO_FIRST) return radio_setting_groups[group - RADIO_FIRST];
#endif
#ifdef H9_RADIO_MENU_PREVIEW
    if (group >= RADIO_FIRST) return radio_categories[group - RADIO_FIRST].name;
#endif
    return groups[group];
}

static void build_list(struct menu_list *list, const struct h9_api *api, unsigned count)
{
#ifdef H9_RADIO_SETTINGS
    for (unsigned group = 0; group < MENU_GROUPS; ++group) list->sections[group].count = 0;
#endif
    struct menu_option apps[MAX_APPS];
    unsigned shown = 0, launcher = 0, station_list = 0;
#ifdef H9_RADIO_SETTINGS
    unsigned categories = count ? bundle_menu_categories(api, count) : 0;
    list->aprs_launcher = 0;
#endif
    for (unsigned i = 1; i < count; ++i) {
        char name[H9_NAME_BYTES + 1];
        read_name(api, i, name);
        if (name[0] == '.') {
            const char *tail = after_prefix(name, ".APRS LIST");
            if (tail && !*tail) station_list = 1;
        } else if (name[0]) {
            const struct tool_label *tool = named_tool(name);
            if (tool == tools) {   /* the APRS entry opens .APRS LIST */
                launcher = 1;
#ifdef H9_RADIO_SETTINGS
                list->aprs_launcher = i;
#endif
            }
            if (shown == MAX_APPS) continue;
            unsigned group = app_group(name, tool);
#ifdef H9_RADIO_SETTINGS
            uint8_t category = 0;
            if (categories) api->read(&category, categories + i, 1);
            if (category) group = category_group(category);
#endif
            apps[shown++] = (struct menu_option){ group, i, 0, 0 };
        }
    }
    list->count = 0;
#ifndef H9_RADIO_SETTINGS
    list->row_count = 0;
#endif
    for (unsigned group = 0; group < GROUPS; ++group) {
#ifndef H9_RADIO_SETTINGS
        unsigned heading = list->row_count;
        list->rows[list->row_count++] = (uint16_t)(HEADING | group);
#endif
        if (group == APRS_SETTINGS) {
#ifndef H9_RADIO_SETTINGS
            for (unsigned i = 0; i < APRS_SETTING_COUNT; ++i) add_option(list, group, i);
#endif
        } else {
            for (unsigned i = 0; i < shown; ++i) {
                unsigned app = apps[i].group;
                if (app == group || (group == APRS && app == APRS_PAGE && !(launcher && station_list)))
                    add_option(list, group, apps[i].index);
            }
            if (group == SETUP) add_option(list, group, 0);   /* USB UPDATE */
        }
#ifdef H9_RADIO_SETTINGS
        if (group == MESSAGES) {
            for (unsigned route = H9_MSG_NEW; route <= H9_MSG_DRAFT; ++route) {
                add_option(list, group, route);
                list->options[list->count - 1].action = APRS_MESSAGE;
            }
        }
        add_actions(list, group, groups[group]);
#else
        if (list->row_count == heading + 1) list->row_count = heading;
#endif
    }
#ifdef H9_RADIO_MENU_PREVIEW
    for (unsigned i = 0; i < RADIO_CATEGORY_COUNT; ++i) {
        unsigned group = RADIO_FIRST + i;
        list->rows[list->row_count++] = (uint16_t)(HEADING | group);
        for (unsigned j = 0; j < radio_categories[i].count; ++j)
            add_option(list, group, j);
    }
#endif
#ifdef H9_RADIO_SETTINGS
    for (unsigned i = 0; i < RADIO_SETTING_GROUP_COUNT; ++i) {
        unsigned group = RADIO_FIRST + i;
        if (group == APRS_BEACON_PAGE) add_aprs_control(list, group, APRS_BEACON_TYPE);
        for (unsigned j = 0; j < RADIO_SETTING_COUNT; ++j)
            if (radio_settings[j].group == i) add_option(list, group, j);
        if (group == APRS_CONFIG_PAGE) add_aprs_control(list, group, APRS_MUTE);
        add_actions(list, group, radio_setting_groups[i]);
    }
#endif
}

#ifdef H9_RADIO_SETTINGS
static const struct menu_folder {
    const char *name;
    uint8_t children[8]; /* page IDs plus one, with zero for unused slots */
} folders[] = {
    [MENU_HOME - MENU_GROUPS] = { "H9 TOOLBOX", {
        RADIO_FOLDER + 1, CHANNELS + 1, APRS_FOLDER + 1, TOOLS + 1,
        GAMES + 1, APPS + 1, DIAGNOSTICS + 1, SETUP + 1
    } },
    [RADIO_FOLDER - MENU_GROUPS] = { "RADIO SETTINGS", {
        RADIO_FIRST + RADIO_GROUP_RADIO + 1, RADIO_FIRST + RADIO_GROUP_VFO_SCAN + 1,
        RADIO_FIRST + RADIO_GROUP_DISPLAY_BUTTONS + 1, RADIO_FIRST + RADIO_GROUP_BLUETOOTH + 1,
        RADIO_FIRST + RADIO_GROUP_DTMF + 1,
        RADIO_FIRST + RADIO_GROUP_GNSS + 1
    } },
    [APRS_FOLDER - MENU_GROUPS] = { "APRS", {
        APRS + 1, MESSAGES + 1, APRS_CONFIG_PAGE + 1, APRS_BEACON_PAGE + 1,
        RADIO_FIRST + RADIO_GROUP_APRS_RELAY_ROUTING + 1
    } },
};

static const char *page_name(unsigned page)
{
    if (page >= MENU_GROUPS) return folders[page - MENU_GROUPS].name;
    if (page == APRS) return "STATIONS & STATUS";
    if (page == APRS_CONFIG_PAGE) return "APRS SETTINGS";
    if (page == RADIO_FIRST + RADIO_GROUP_RADIO) return "GENERAL";
    return group_name(page);
}

/* Rebuild only the directory view. Its leaves still index the full catalog. */
struct menu_view {
    unsigned count;
    uint8_t children[sizeof folders[0].children];
};

static struct menu_view menu_view(const struct menu_list *list, unsigned page)
{
    struct menu_view view = { 0 };
    if (page < MENU_GROUPS) view.count = list->sections[page].count;
    else {
        const uint8_t *children = folders[page - MENU_GROUPS].children;
        for (unsigned i = 0; i < sizeof view.children && children[i]; ++i) {
            unsigned child = children[i] - 1;
            if (child >= MENU_GROUPS || list->sections[child].count)
                view.children[view.count++] = child;
        }
    }
    return view;
}

static void menu_open(const struct menu_list *list, struct menu_state *state,
                      struct menu_view *view, unsigned page, unsigned selected)
{
    *view = menu_view(list, page);
    state->page = page;
    state->selected = selected < view->count ? selected : 0;
}

static void menu_parent(const struct menu_list *list, struct menu_state *state, struct menu_view *view)
{
    unsigned child = state->page;
    for (unsigned page = MENU_HOME; page < FOLDER_COUNT; ++page) {
        struct menu_view parent = menu_view(list, page);
        for (unsigned row = 0; row < parent.count; ++row)
            if (parent.children[row] == child) {
                menu_open(list, state, view, page, row);
                return;
            }
    }
}

static unsigned menu_position(const struct menu_state *state)
{
    return MENU_POSITION_MAGIC | (state->page << 8) | state->selected;
}

static struct menu_option menu_option(const struct menu_list *list, unsigned page, unsigned row)
{
    return list->options[list->sections[page].first + row];
}

/* Factory 1.0.38 RGB565 palette at app+0xB2F68, in Menu Color order. */
static const uint16_t radio_menu_colors[] = {
    0x001F, 0xF800, 0x07E0, 0xFFE0, 0xF81F, 0xFD20, 0x7D7C, 0x07FF, 0x8430,
    0x01CF, 0x841F, 0xBC40, 0xF81F, 0xFC07, 0x5458, 0xC618, 0xA651, 0x2B12
};
static uint16_t menu_color(unsigned value)
{
    return value < sizeof radio_menu_colors / sizeof *radio_menu_colors
        ? radio_menu_colors[value] : radio_menu_colors[0];
}

static uint16_t saved_menu_color(void)
{
    struct radio_edit edit;
    return radio_begin(&radio_stock_storage, &radio_settings[RADIO_MENU_COLOR], &edit) == RADIO_OK
        ? menu_color(radio_value(&edit)) : UI_ACCENT;
}

static unsigned luma(uint16_t rgb)
{
    return 26 * (rgb >> 11) + 26 * ((rgb >> 5) & 63) + 10 * (rgb & 31);
}

static uint16_t menu_ink(uint16_t accent)
{
    /* Lighten dark colors for small text on the black LCD background. */
    return luma(accent) < 1050 ? ((accent >> 1) & 0x7BEF) + 0x8410 : accent;
}

static int previewing_color(const struct menu_state *state)
{
    return state->editing && state->radio_draft.setting == &radio_settings[RADIO_MENU_COLOR];
}
#endif

static const char *draw_row(const struct h9_api *api, const struct menu_list *list,
                            const struct menu_state *state,
#ifdef H9_RADIO_SETTINGS
                            const struct menu_view *view, uint16_t accent,
#endif
                            unsigned row, unsigned y)
{
#ifdef H9_RADIO_SETTINGS
    unsigned count = view->count;
#else
    unsigned count = list->row_count;
#endif
    if (row >= count) {
        gfx_fill(0, y, ROW_WIDTH, ROW_H, UI_BG);
        return 0;
    }
#ifdef H9_RADIO_SETTINGS
    unsigned item = row;
    struct menu_option option = { 0 };
    int folder = state->page >= MENU_GROUPS;
    if (!folder) option = menu_option(list, state->page, row);
#else
    unsigned item = list->rows[row];
    if (item & HEADING) {
        gfx_fill(0, y, ROW_WIDTH, ROW_H, UI_BG);
        unsigned x = gfx_text(3, y + 3, group_name(item & ~HEADING), 20, 1, UI_DIM, UI_BG);
        gfx_text(x + 6, y + 3, "-------------------------", (ROW_WIDTH - x - 6) / 6, 1, UI_DIM, UI_BG);
        return 0;
    }
    struct menu_option option = list->options[item];
#endif
    char name[H9_NAME_BYTES + 1], minutes[8];
#ifdef H9_RADIO_SETTINGS
    char radio_buffer[RADIO_MAX_VALUE + 1];
#endif
    const char *label = "USB UPDATE", *value = ">", *help = "Open USB firmware updater";
#ifdef H9_RADIO_SETTINGS
    if (folder) {
        label = page_name(view->children[row]);
        help = 0;
    } else if (option.action == APRS_MESSAGE) {
        label = message_labels[option.index - H9_MSG_NEW];
        help = message_help[option.index - H9_MSG_NEW];
    } else if (option.action == APRS_CONTROL) {
        if (option.index == APRS_BEACON_TYPE) {
            label = "BEACON TYPE";
            help = "TIMED, PTT OR SMART";
            value = radio_beacon_label(state->editing && item == state->selected ? state->beacon_draft : state->beacon_type);
        } else {
            label = settings[APRS_MUTE];
            help = setting_help[APRS_MUTE];
            value = aprs_setting_value(APRS_MUTE, state->available ? &state->draft : 0, minutes);
        }
    } else
#endif
    if (option.action) {
#ifdef H9_RADIO_SETTINGS
        label = radio_actions[option.index].label;
        help = radio_actions[option.index].help;
#endif
    } else if (option.group == APRS_SETTINGS) {
        label = settings[option.index];
        value = aprs_setting_value(option.index, state->available ? &state->draft : 0, minutes);
#ifdef H9_RADIO_SETTINGS
        if (state->editing && item == state->selected && state->have_number && state->number_invalid)
            value = radio_number_label(state->number, radio_buffer);
#endif
        help = setting_help[option.index];
#ifdef H9_RADIO_SETTINGS
    } else if (option.group >= RADIO_FIRST) {
        struct radio_edit edit;
        const struct radio_setting *setting = &radio_settings[option.index];
        label = help = setting->label;
        if (state->editing && item == state->selected)
            value = state->have_number && state->number_invalid ? radio_number_label(state->number, radio_buffer) :
                radio_value_label(&state->radio_draft, radio_buffer);
        else value = radio_begin(&radio_stock_storage, setting, &edit) == RADIO_OK
            ? radio_value_label(&edit, radio_buffer) : "READ ERROR";
#endif
#ifdef H9_RADIO_MENU_PREVIEW
    } else if (option.group >= RADIO_FIRST) {
        label = radio_categories[option.group - RADIO_FIRST].items[option.index];
        help = "STOCK CONTROL NOT WIRED";
#endif
    } else if (option.index) {
        read_name(api, option.index, name);
        const struct tool_label *tool = named_tool(name);
        label = tool ? tool->label : name;
        help = tool ? tool->help : 0;
    }
    unsigned selected = item == state->selected;
#ifdef H9_RADIO_SETTINGS
    uint16_t bg = selected ? (state->editing && !previewing_color(state) ? UI_EDIT : accent) : UI_BG;
    uint16_t label_ink = selected ? (luma(bg) >= 820 ? UI_BG : UI_TEXT) : UI_TEXT;
    uint16_t value_ink = selected ? label_ink : menu_ink(accent);
#else
    uint16_t bg = selected ? (state->editing ? UI_EDIT : UI_ACCENT) : UI_BG;
    uint16_t label_ink = selected ? UI_BG : UI_TEXT;
    uint16_t value_ink = selected ? UI_BG : UI_ACCENT;
#endif
    gfx_fill(0, y, ROW_WIDTH, ROW_H, bg);
    unsigned n = text_length(value);
#ifdef H9_RADIO_SETTINGS
    if (n > 12) n = 12;
    gfx_text(3, y + 3, label, (ROW_WIDTH - 12) / 6 - n, 1, label_ink, bg);
#else
    gfx_text(3, y + 3, label, H9_NAME_BYTES, 1, label_ink, bg);
#endif
    gfx_text(ROW_WIDTH - 3 - n * 6, y + 3, value, n, 1, value_ink, bg);
    return selected ? help : 0;
}

static void draw_menu(const struct h9_api *api, const struct menu_list *list, const struct menu_state *state
#ifdef H9_RADIO_SETTINGS
                      , const struct menu_view *view
#endif
                      )
{
#ifdef H9_RADIO_SETTINGS
    unsigned total = view->count, row = state->selected, rows = total;
    const char *title = page_name(state->page);
    uint16_t accent = previewing_color(state) ? menu_color(radio_value(&state->radio_draft)) : state->accent;
    uint16_t ink = menu_ink(accent);
#else
    struct menu_option option = list->options[state->selected];
    unsigned total = list->count, row = option.row, rows = list->row_count;
    const char *title = group_name(option.group);
    uint16_t accent = UI_ACCENT, ink = UI_ACCENT;
#endif
    unsigned item = state->selected + 1;
#ifdef H9_RADIO_MENU_PREVIEW
    char count[] = { '0' + item / 100, '0' + item / 10 % 10, '0' + item % 10, '/',
                     '0' + total / 100, '0' + total / 10 % 10,
                     '0' + total % 10, 0 };
    if (state->digit != NO_DIGIT) {
        count[0] = '-';
        count[1] = state->digit >= 10 ? '0' + state->digit / 10 : '-';
        count[2] = '0' + state->digit % 10;
    }
#else
    char count[] = { '0' + item / 10, '0' + item % 10, '/',
                     '0' + total / 10, '0' + total % 10, 0 };
    if (state->digit != NO_DIGIT) {
#ifdef H9_RADIO_SETTINGS
        count[0] = state->digit >= 10 ? '0' + state->digit / 10 : '-';
        count[1] = '0' + state->digit % 10;
#else
        count[0] = '0' + state->digit;
        count[1] = '-';
#endif
    }
#endif
    gfx_fill(0, 0, H9_WIDTH, ROW_Y, UI_BG);
    gfx_text(UI_LEFT, UI_TITLE_Y, title, sizeof count > 6 ? 18 : 20, 1, ink, UI_BG);
    gfx_text(H9_WIDTH - sizeof count * 6 + 1, UI_TITLE_Y, count, sizeof count - 1, 1, ink, UI_BG);

    unsigned top = row < VISIBLE / 2 ? 0 : row - VISIBLE / 2;
    unsigned limit = rows > VISIBLE ? rows - VISIBLE : 0;
    if (top > limit) top = limit;
    const char *help = 0;
    for (unsigned i = 0; i < VISIBLE; ++i) {
        const char *row_help = draw_row(api, list, state,
#ifdef H9_RADIO_SETTINGS
                                        view, accent,
#endif
                                        top + i, ROW_Y + i * ROW_H);
        if (row_help) help = row_help;   /* headings and other rows never return help */
    }
    unsigned track = VISIBLE * ROW_H;
    gfx_fill(ROW_WIDTH + 1, ROW_Y, 2, track, UI_DIM);
    unsigned offset = total > 1 ? (track - 5) * state->selected / (total - 1) : 0;
    gfx_fill(ROW_WIDTH + 1, ROW_Y + offset, 2, 5, accent);
    gfx_fill(0, ROW_Y + VISIBLE * ROW_H, H9_WIDTH, H9_HEIGHT - ROW_Y - VISIBLE * ROW_H, UI_BG);
    if (state->notice || help)
        gfx_text(3, 104, state->notice ? state->notice : help, 26, 1,
                 state->notice ? ink : UI_HINT, UI_BG);
    const char *footer = state->editing ? "MENU DONE   UP/DN CHANGE" : "MENU PICK   BACK RADIO";
#ifdef H9_RADIO_SETTINGS
    if (state->editing) {
        struct menu_option option = menu_option(list, state->page, state->selected);
        footer = option.group >= RADIO_FIRST && option.action != APRS_CONTROL
            ? "0-9 VALUE MENU SAVE BACK" : "MENU SAVE  BACK CANCEL";
    } else if (state->page >= MENU_GROUPS)
        footer = state->page == MENU_HOME ? "MENU OPEN   BACK RADIO" : "MENU OPEN   BACK UP";
    else footer = "MENU PICK   BACK UP";
#endif
    gfx_text(UI_LEFT, UI_FOOT_Y, footer, UI_CELLS, 1,
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

#ifdef H9_RADIO_SETTINGS
void toolbox_open_radio_settings(void)
{
    MENU_SELECTION = MENU_POSITION_MAGIC | RADIO_FOLDER << 8;
}

void toolbox_open_messages(void)
{
    MENU_SELECTION = MENU_POSITION_MAGIC | MESSAGES << 8;
}

static int apply_radio(const struct h9_api *loader, struct menu_state *state)
{
    if (!state->radio_changed) return 1;
    int result = radio_apply_pending(loader);
    if (result) { state->notice = result == RADIO_BUSY ? "RADIO BUSY: TRY AGAIN" : "SETTINGS APPLY FAILED"; return 0; }
    state->radio_changed = 0;
    return 1;
}
#endif

unsigned toolbox_menu(const struct h9_api *loader, unsigned count)
{
#if defined(H9_RADIO_MENU_PREVIEW) || defined(H9_RADIO_SETTINGS)
#ifndef MENU_LIST_STORAGE
    /* The far menu's RAM module is only an eight-byte jump. Its unused slot
     * holds the larger list while this menu runs; the next module replaces it. */
#define MENU_LIST_STORAGE ((struct menu_list *)0x29F20u)
#endif
    struct menu_list *list = MENU_LIST_STORAGE;
#else
    struct menu_list stack_list;
    struct menu_list *list = &stack_list;
#endif
    struct menu_state state = { .digit = NO_DIGIT };
    const struct h9_runtime_api *api = aprs_runtime(loader);
    build_list(list, loader, count);
    unsigned saved_row = MENU_SELECTION;
#ifdef H9_RADIO_SETTINGS
    struct menu_view view;
    unsigned saved_page = (saved_row >> 8) & 0xffu;
    if ((saved_row & (0xffff0000u & ~H9_MSG_ROUTE_MASK)) == (MENU_POSITION_MAGIC & ~H9_MSG_ROUTE_MASK) && saved_page < FOLDER_COUNT &&
        (saved_page >= MENU_GROUPS || list->sections[saved_page].count))
        menu_open(list, &state, &view, saved_page, saved_row & 0xffu);
    else menu_open(list, &state, &view, MENU_HOME, 0);
    MENU_SELECTION = menu_position(&state);
    state.accent = saved_menu_color();
    uint32_t last_repeat = 0;
    unsigned repeat_wait = REPEAT_DELAY;
#define DRAW_MENU() draw_menu(loader, list, &state, &view)
#define ITEM_COUNT view.count
#define SAVE_POSITION() (MENU_SELECTION = menu_position(&state))
#else
    if (saved_row < list->count) state.selected = saved_row;
#define DRAW_MENU() draw_menu(loader, list, &state)
#define ITEM_COUNT list->count
#define SAVE_POSITION() (MENU_SELECTION = state.selected)
#endif
    state.available = read_settings(api, &state);
#ifdef H9_RADIO_SETTINGS
    state.beacon_type = radio_beacon_type(&radio_stock_storage, state.available ? &state.saved : 0);
#endif
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    DRAW_MENU();
    gfx_wait_release();
    uint32_t last_key = MENU_TICK;
    unsigned sample = H9_KEY_NONE, stable = H9_KEY_NONE;
    for (;;) {
        h9_delay(2);
        uint32_t now = MENU_TICK;
        int redraw = 0;
        if (state.dirty && now - state.last_edit >= SAVE_WAIT
#ifdef H9_RADIO_SETTINGS
            && !state.editing
#endif
        ) {
            save_pending(loader, api, &state);
            redraw = 1;
        }
        if (state.digit != NO_DIGIT && now - last_key >= DIGIT_WAIT) {
            select_number(&state, ITEM_COUNT, state.digit);
            redraw = 1;
        }
        if (now - last_key >= IDLE_WAIT) {
#ifdef H9_RADIO_SETTINGS
            if (!apply_radio(loader, &state)) { last_key = now; DRAW_MENU(); continue; }
#endif
            return 0;
        }

        unsigned key = h9_key();
        int pressed = key == sample && key != stable;
        if (pressed) stable = key;
#ifdef H9_RADIO_SETTINGS
        if (pressed) {
            last_repeat = now;
            repeat_wait = REPEAT_DELAY;
        } else if (key == stable && !state.editing && (key == H9_KEY_UP || key == H9_KEY_DOWN) &&
                   now - last_repeat >= repeat_wait) {
            last_repeat = now;
            repeat_wait = REPEAT_PERIOD;
            pressed = 1;
        }
#endif
        if (pressed && key != H9_KEY_NONE) {
            last_key = now;
            redraw = 1;
            state.notice = 0;
            if (key >= 2 && key <= 11) {   /* digit d = d + 2 */
                unsigned digit = key - 2;
#ifdef H9_RADIO_SETTINGS
                struct menu_option current = {0};
                if (state.editing) current = menu_option(list, state.page, state.selected);
                if (state.editing && current.action == APRS_CONTROL) {
                    state.notice = "USE UP/DN TO CHANGE";
                } else if (state.editing && state.page >= RADIO_FIRST) {
                    const struct radio_setting *setting = state.radio_draft.setting;
                    unsigned number = digit;
                    if (state.have_number && state.number <= 99999999 &&
                        (setting->kind != RADIO_CHOICE || (previewing_color(&state) && state.number == 1)))
                        number += state.number * 10;
                    state.number = number;
                    state.have_number = 1;
                    unsigned value = radio_is_signed(setting) && (int32_t)radio_value(&state.radio_draft) < 0
                        ? -number : number;
                    state.number_invalid = radio_set_number(&state.radio_draft, value) != RADIO_OK;
                    if (state.number_invalid) state.notice = "VALUE OUT OF RANGE";
                } else
#endif
                {
                save_pending(loader, api, &state);
                state.editing = 0;
#if defined(H9_RADIO_MENU_PREVIEW) || defined(H9_RADIO_SETTINGS)
                if (state.digit != NO_DIGIT) {
                    unsigned number = 10 * state.digit + digit;
                    if (number > ITEM_COUNT / 10) select_number(&state, ITEM_COUNT, number);
                    else state.digit = number;
                } else if (digit > ITEM_COUNT / 10) select_number(&state, ITEM_COUNT, digit);
                else state.digit = digit;
#else
                if (state.digit != NO_DIGIT) select_number(&state, ITEM_COUNT, 10 * state.digit + digit);
                else if (digit > ITEM_COUNT / 10) select_number(&state, ITEM_COUNT, digit);
                else state.digit = digit;
#endif
                }
            } else {
#if defined(H9_RADIO_MENU_PREVIEW) || defined(H9_RADIO_SETTINGS)
                if (key == H9_KEY_MENU && state.digit != NO_DIGIT)
                    select_number(&state, ITEM_COUNT, state.digit);
                else state.digit = NO_DIGIT;
#else
                state.digit = NO_DIGIT;
#endif
#ifdef H9_RADIO_SETTINGS
                struct menu_option option = { 0 };
                if (state.page < MENU_GROUPS) option = menu_option(list, state.page, state.selected);
#else
                struct menu_option option = list->options[state.selected];
#endif
#ifdef H9_RADIO_SETTINGS
                if (key == H9_KEY_MENU && state.page >= MENU_GROUPS)
                    menu_open(list, &state, &view, view.children[state.selected], 0);
                else
#endif
                if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
                    int direction = key == H9_KEY_UP ? 1 : -1;
                    if (state.editing) {
#ifdef H9_RADIO_SETTINGS
                        if (option.action == APRS_CONTROL && option.index == APRS_BEACON_TYPE) {
                            state.beacon_draft = (state.beacon_draft + (direction > 0 ? 1 : BEACON_TYPE_COUNT - 1)) % BEACON_TYPE_COUNT;
                        } else if (option.group >= RADIO_FIRST && option.action != APRS_CONTROL) {
                            if (radio_adjust(&state.radio_draft, direction)) state.notice = "CHECK VALUE";
                            state.have_number = 0;
                            state.number_invalid = 0;
                        } else
#endif
                        {
                        state.notice = aprs_setting_adjust(api, option.index, &state.draft, direction);
                        if (!state.notice) {
                            state.dirty = 1;
                            state.last_edit = now;
#ifdef H9_RADIO_SETTINGS
                            state.have_number = state.number_invalid = 0;
#endif
                        }
                        }
                    } else if (direction > 0) {
                        state.selected = state.selected ? state.selected - 1 : ITEM_COUNT - 1;
                    } else {
                        state.selected = (state.selected + 1) % ITEM_COUNT;
                    }
                } else if (state.editing && (key == H9_KEY_MENU || key == H9_KEY_BACK)) {
#ifdef H9_RADIO_SETTINGS
                    if (option.action == APRS_CONTROL && option.index == APRS_BEACON_TYPE) {
                        if (key == H9_KEY_BACK) state.editing = 0;
                        else {
                            state.notice = radio_beacon_save(&radio_stock_storage, loader, api, &state.saved, state.beacon_draft);
                            if (!state.notice) {
                                state.beacon_type = state.beacon_draft;
                                state.draft = state.saved;
                                state.editing = 0;
                                state.radio_changed = 1;
                            }
                        }
                    } else if (option.group >= RADIO_FIRST && option.action != APRS_CONTROL) {
                        if (key == H9_KEY_BACK) state.editing = 0;
                        else if (state.number_invalid) state.notice = "VALUE OUT OF RANGE";
                        else {
                            int result = radio_save(&radio_stock_storage, &state.radio_draft);
                            if (!result) {
                                state.radio_changed |= state.radio_draft.setting->offset < 0x6000;
                                if (previewing_color(&state))
                                    state.accent = menu_color(radio_value(&state.radio_draft));
                                state.editing = 0;
                                state.notice = "SAVED";
                            } else state.notice = result == RADIO_CONFLICT ? "CHANGED ELSEWHERE: REOPEN" : "SETTINGS SAVE FAILED";
                        }
                    } else
#endif
                    {
#ifdef H9_RADIO_SETTINGS
                    if (key == H9_KEY_BACK) {
                        state.draft = state.saved;
                        state.dirty = 0;
                        state.editing = 0;
                    } else {
#endif
                    save_pending(loader, api, &state);
                    state.editing = 0;
#ifdef H9_RADIO_SETTINGS
                    }
#endif
                    }
                } else if (key == H9_KEY_BACK) {
#ifdef H9_RADIO_SETTINGS
                    if (state.page != MENU_HOME) menu_parent(list, &state, &view);
                    else if (apply_radio(loader, &state)) return 0;
#else
                    return 0;
#endif
#ifdef H9_RADIO_SETTINGS
                } else if (key == H9_KEY_MENU && option.action == APRS_CONTROL) {
                    if (!state.available) state.available = read_settings(api, &state);
                    if (!state.available) state.notice = "SETTINGS UNAVAILABLE";
                    else if (option.index == APRS_BEACON_TYPE) {
                        if (state.beacon_type >= BEACON_TYPE_COUNT) state.notice = "SETTINGS READ FAILED";
                        else if (apply_radio(loader, &state)) {
                            state.beacon_draft = state.beacon_type;
                            state.editing = 1;
                        }
                    } else state.editing = 1;
#else
                } else if (key == H9_KEY_MENU && option.group == APRS_SETTINGS) {
                    if (!state.available) state.available = read_settings(api, &state);
                    if (!state.available) state.notice = "SETTINGS UNAVAILABLE";
                    else if (option.index == APRS_INTERVAL && !state.draft.timed) state.notice = "TIMED MODE ONLY";
                    else state.editing = 1;
#endif
                } else if (key == H9_KEY_MENU) {
#ifdef H9_RADIO_SETTINGS
                    if (option.action == APRS_MESSAGE) {
                        if (!list->aprs_launcher) state.notice = "APRS CLIENT UNAVAILABLE";
                        else if (apply_radio(loader, &state)) {
                            MENU_SELECTION = menu_position(&state) | (unsigned)option.index << H9_MSG_ROUTE_SHIFT;
                            return list->aprs_launcher;
                        }
                    } else if (option.action == MENU_ACTION) {
                        if (apply_radio(loader, &state)) {
                            int result = radio_action(loader, option.index);
                            state.radio_changed |= result == -1;
                            if (result == -1) {
                                state.accent = saved_menu_color();
                                state.available = read_settings(api, &state);
                                state.beacon_type = radio_beacon_type(&radio_stock_storage, state.available ? &state.saved : 0);
                            }
                            state.notice = result == -1 ? "SAVED" : result == RADIO_BUSY ? "RADIO BUSY: TRY AGAIN"
                                : result ? "ACTION FAILED" : 0;
                            last_key = MENU_TICK;
                            stable = sample = H9_KEY_NONE;
                            gfx_wait_release();
                        }
                    } else if (option.group >= RADIO_FIRST) {
                        const struct radio_setting *setting = &radio_settings[option.index];
                        if (!radio_is_text(setting)) {
                            int result = radio_begin(&radio_stock_storage, setting, &state.radio_draft);
                            if (!result) { state.editing = 1; state.have_number = state.number_invalid = 0; }
                            else state.notice = "SETTINGS READ FAILED";
                        } else {
                            int result = radio_editor(&radio_stock_storage, setting);
                            state.radio_changed |= result == -1;
                            state.notice = result == -1 ? "SAVED" : result ? "SETTINGS READ FAILED" : 0;
                            last_key = MENU_TICK;
                            stable = sample = H9_KEY_NONE;
                            gfx_wait_release();
                        }
                    } else
#endif
#ifdef H9_RADIO_MENU_PREVIEW
                    if (option.group >= RADIO_FIRST) {
                        state.notice = "STOCK CONTROL NOT WIRED";
                    } else
#endif
                    if (option.index) {
#ifdef H9_RADIO_SETTINGS
                        if (!apply_radio(loader, &state)) { DRAW_MENU(); continue; }
#endif
                        SAVE_POSITION();
                        return option.index;
                    }
                    else {
#ifdef H9_RADIO_SETTINGS
                        if (!apply_radio(loader, &state)) { DRAW_MENU(); continue; }
#endif
                        h9_usb_loader();
                        return 0;
                    }
                }
            }
        }
        sample = key;
        if (redraw) {
            SAVE_POSITION();
            DRAW_MENU();
        }
    }
}
#undef DRAW_MENU
#undef ITEM_COUNT
#undef SAVE_POSITION
