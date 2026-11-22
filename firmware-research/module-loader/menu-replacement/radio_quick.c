#include "app_theme.h"
#include "key_ramp.h"
#include "font.h"
#include "radio_channels.h"
#include "radio_system.h"

#ifndef RADIO_BYTE
#define RADIO_BYTE(address) (*(volatile uint8_t *)(uintptr_t)(address))
#define QUICK_TICK (*(const volatile uint32_t *)0x1D8B0u)
#define STOCK_ADC ((unsigned (*)(unsigned))0x01E4EC74u)
#define STOCK_REDRAW ((void (*)(unsigned))0x01E53306u)
#define STOCK_SQUELCH_REFRESH() ((void (*)(unsigned))0x01E58E3Eu)(0)
#define STOCK_BACKLIGHT() ((void (*)(void))0x01E50492u)()
#define STOCK_MONITOR_ON() ((void (*)(unsigned))0x01E7E3F6u)(3)
#define STOCK_MONITOR_OFF() ((void (*)(unsigned))0x01E7E494u)(3)
#endif

enum { KEY_HASH = 20, KEY_EXIT = 255, IDLE_TICKS = 2000 };
enum { STOCK_MENU = 1, KEY_QUEUE_COUNT = 0xC3B4, KEY_QUEUE_FIRST = 0xC3B5, KEY_QUEUE_CAPACITY = 12 };
enum { STOCK_BRIGHTNESS = 0x12BAA, STOCK_SQUELCH_LEVEL = 0x12B9E, SQUELCH_SETTING_OFFSET = 0xCA9 };
enum { RECEIVE_ACTIVE = 0x11AFE, MONITOR_FLAG = 0x11CC8, VOX_PREVIOUS_GAIN = 0x299FC };
enum { SQUELCH_TILE = 1, TONE_TILE = 2, RESUME_TILE = 3, MONITOR_TILE = 4, WIDTH_TILE = 7, BRIGHT_TILE = 8 };
enum { CHOICE, CYCLE, MONITOR, VOX };
enum { RX_FREQ, TX_FREQ, RX_TONE, TX_TONE, WIDTH, POWER, DIRECTION = 11 };

void toolbox_open_radio_settings(void);

static const struct quick_tile {
    const char *label, *id;
    uint8_t field, action;
} tiles[] = {
    { "POWER", 0, POWER, CYCLE },       { "SQUELCH", "squelch", 0, CHOICE }, { "TONE", 0, TX_TONE, CHOICE },
    { "RESUME", "scanMode", 0, CYCLE },            { "MONITOR", 0, TX_FREQ, MONITOR },   { "VOX", "voxGain", 0, VOX },
    { "DUAL", "dualWatch", 0, CYCLE },  { "WIDTH", 0, WIDTH, CYCLE },       { "BRIGHT", "brightness", 0, CYCLE },
};

static unsigned active_target(void)
{
    unsigned side = RADIO_BYTE(0x11B50u) >> 7;
    if (!RADIO_BYTE(0x11AE9u + side)) return 200 + side;
    return RADIO_BYTE(0x12BB2u + side) == 1 ? RADIO_BYTE(0x11AEFu + side) : 0;
}

static int keypad_home(void)
{
    return RADIO_BYTE(0x11AB0u) <= 1 && RADIO_BYTE(0x11AC6u) == 3 &&
           !RADIO_BYTE(0x11AC2u) && !RADIO_BYTE(0x11AFBu) &&
           !RADIO_BYTE(0xC13Au) && !RADIO_BYTE(0xC140u) && !RADIO_BYTE(0xC141u);
}

static int home_ready(void)
{
    return keypad_home() && !RADIO_BYTE(0x11AFDu) && !RADIO_BYTE(0x11B23u);
}

static int ptt_down(void)
{
    /* Same ADC channel and voltage window as stock's main PTT scan. */
    return STOCK_ADC(3) - 63u <= 120u;
}

static int quick_allowed(void)
{
    if (RADIO_BYTE(0x11AB0u) > 1 || RADIO_BYTE(0x11AC2u) ||
        RADIO_BYTE(0x11AFBu) || ptt_down()) return 0;
    return keypad_home() || RADIO_BYTE(RECEIVE_ACTIVE) || RADIO_BYTE(MONITOR_FLAG);
}

static int tile_edit(unsigned index, unsigned target, struct radio_setting *field, struct radio_edit *edit)
{
    const struct quick_tile *tile = &tiles[index];
    const struct radio_setting *setting = 0;
    if (tile->id) setting = radio_setting_by_id(tile->id);
    else if (!radio_channel_field(target, tile->field, field)) setting = field;
    return radio_begin(&radio_stock_storage, setting, edit);
}

/* The normal 2x font takes 12 pixels per character. Use an 8x16 cell
 * on the grid so a complete CTCSS frequency or DCS code fits a column. */
static void draw_grid_value(unsigned x, unsigned y, unsigned width, const char *text,
                            uint16_t color, uint16_t background)
{
    unsigned length = 0;
    while (length < 6 && text[length] && text[length] != ' ') ++length;
    x += (width - length * 8) / 2;
    for (unsigned i = 0; i < length; ++i, x += 8) {
        char c = text[i];
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        unsigned glyph = 0;
        while (FONT_CHARS[glyph] && FONT_CHARS[glyph] != c) ++glyph;
        const uint8_t *rows = FONT[FONT_CHARS[glyph] ? glyph : 0];
        h9_lcd_window(x, y, x + 7, y + 15);
        for (unsigned half = 0; half < 2; ++half) {
            uint8_t pixels[128];
            for (unsigned py = 0; py < 8; ++py) {
                unsigned row = half * 4 + py / 2;
                unsigned bits = row < 7 ? rows[row] : 0;
                for (unsigned px = 0; px < 8; ++px) {
                    uint16_t pixel = px < 7 && (bits & (16u >> (px * 5 / 7))) ? color : background;
                    unsigned offset = (py * 8 + px) * 2;
                    pixels[offset] = pixel >> 8;
                    pixels[offset + 1] = pixel;
                }
            }
            h9_lcd_write(pixels, sizeof pixels);
        }
    }
}

struct split_edit {
    struct radio_setting frequency, direction;
    struct radio_edit offset, shift;
    unsigned rx, tx;
};

static int begin_split(unsigned target, struct split_edit *split)
{
    radio_channel_field(target, RX_FREQ, &split->frequency);
    radio_channel_field(target, DIRECTION, &split->direction);
    int result = radio_begin(&radio_stock_storage, &split->frequency, &split->offset);
    if (result) return result;
    split->rx = radio_value(&split->offset);
    radio_channel_field(target, TX_FREQ, &split->frequency);
    result = radio_begin(&radio_stock_storage, &split->frequency, &split->offset);
    if (result) return result;
    result = radio_begin(&radio_stock_storage, &split->direction, &split->shift);
    if (result) return result;
    unsigned direction = radio_value(&split->shift), offset = radio_value(&split->offset);
    split->tx = split->rx;
    if (offset <= 60000000) {
        if (direction == 1 && offset <= split->rx) split->tx -= offset;
        if (direction == 2) split->tx += offset;
    }
    return RADIO_OK;
}

static const char *grid_value(unsigned index, unsigned target, char *value)
{
    if (index == MONITOR_TILE) {
        if (target < 200) return RADIO_BYTE(MONITOR_FLAG) ? "ON" : "OFF";
        struct split_edit split;
        return begin_split(target, &split) ? "--" : split.tx == split.rx ? "SIMP" : "SPLIT";
    }
    struct radio_setting field;
    struct radio_edit edit;
    if (tile_edit(index, target, &field, &edit)) return "--";
    unsigned current = radio_value(&edit);
    if (index == RESUME_TILE && current == 1) return "CARR";
    if (index == WIDTH_TILE) return current ? "12.5k" : "25k";
    if (tiles[index].action == VOX) return current >= 1 && current <= 5 ? "ON" : "OFF";
    const char *label = radio_value_label(&edit, value);
    return index == BRIGHT_TILE && label[0] == 'L' ? label + sizeof("Level ") - 1 : label;
}

static __attribute__((noinline)) void draw_grid(unsigned selected, unsigned target, const char *notice)
{
    enum { GRID_Y = 15, COL_W = 53, ROW_H = 31, FOCUS = 0x1a6f, DIVIDER = 0x738e };
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("QUICK CHANGE");
    for (unsigned i = 0; i < sizeof tiles / sizeof *tiles; ++i) {
        unsigned x = 1 + i % 3 * COL_W, y = GRID_Y + i / 3 * ROW_H;
        uint16_t bg = i == selected ? FOCUS : UI_BG;
        uint16_t title = i == selected ? UI_TEXT : 0xc618;
        char number[] = { '1' + i, 0 }, value[RADIO_MAX_VALUE + 1];
        const char *label = grid_value(i, target, value);
        const char *title_text = i == MONITOR_TILE && target >= 200 ? "TX FREQ" : tiles[i].label;
        gfx_fill(x, y, COL_W - 1, ROW_H - 1, bg);
        gfx_text(x + 2, y + 3, number, 1, 1, i == selected ? UI_ACCENT : UI_HINT, bg);
        gfx_text(x + 10, y + 3, title_text, 7, 1, title, bg);
        draw_grid_value(x, y + 12, COL_W - 1, label, UI_TEXT, bg);
    }
    for (unsigned n = 0; n <= 3; ++n) {
        gfx_fill(0, GRID_Y - 1 + n * ROW_H, H9_WIDTH, 1, DIVIDER);
        gfx_fill(n * COL_W, GRID_Y, 1, 3 * ROW_H - 1, DIVIDER);
    }
    if (notice) gfx_text(3, 109, notice, 26, 1, UI_DANGER, UI_BG);
    ui_footer("0 SETTINGS  MENU  BACK");
}

struct quick_input { uint32_t last; struct h9_key_ramp keys; };

static unsigned next_key(struct quick_input *input, int repeat)
{
    for (;;) {
        h9_delay(20);
        unsigned key = h9_key();
        uint32_t now = QUICK_TICK;
        if (ptt_down() || now - input->last >= IDLE_TICKS) return KEY_EXIT;
        key = h9_key_ramp(&input->keys, key, now, repeat);
        if (key) {
            input->last = now;
            return key;
        }
    }
}

static int save_and_apply(void *context)
{
    if (RADIO_BYTE(RECEIVE_ACTIVE) || !home_ready() || ptt_down()) return RADIO_BUSY;
    int result = radio_save(&radio_stock_storage, context);
    return result ? result : radio_system_platform.reload();
}

static int apply_live_setting(void *context)
{
    if (!quick_allowed()) return RADIO_BUSY;
    struct radio_edit *edit = context;
    int result = radio_save(&radio_stock_storage, edit);
    if (result) return result;
    unsigned value = radio_value(edit);
    if (edit->setting->offset == SQUELCH_SETTING_OFFSET) {
        /* Same level byte and RF refresh as the native squelch setter. */
        RADIO_BYTE(STOCK_SQUELCH_LEVEL) = value;
        STOCK_SQUELCH_REFRESH();
    } else {
        RADIO_BYTE(STOCK_BRIGHTNESS) = 4 - value;
        STOCK_BACKLIGHT();
    }
    return RADIO_OK;
}

static int toggle_monitor(void *context)
{
    (void)context;
    if (!quick_allowed()) return RADIO_BUSY;
    if (RADIO_BYTE(MONITOR_FLAG)) STOCK_MONITOR_OFF();
    else STOCK_MONITOR_ON();
    return RADIO_OK;
}

static int save_split(void *context)
{
    struct split_edit *split = context;
    if (RADIO_BYTE(RECEIVE_ACTIVE) || !home_ready() || ptt_down()) return RADIO_BUSY;
    const unsigned bands[][2] = {{13600000,17400000}, {22000000,26000000},
                                  {35000000,39000000}, {40000000,52000000}};
    unsigned band = 0;
    while (band < 4 && (split->rx < bands[band][0] || split->rx > bands[band][1])) ++band;
    if (band == 4 || split->tx < bands[band][0] || split->tx > bands[band][1]) return RADIO_INVALID;

    unsigned direction = split->tx == split->rx ? 0 : split->tx < split->rx ? 1 : 2;
    unsigned delta = direction == 1 ? split->rx - split->tx : split->tx - split->rx;
    unsigned original = radio_value(&split->shift);
    /* Publish simplex before changing the sign. An interrupted write then
     * leaves either the original split, simplex, or the complete new split. */
    if (original != direction && original) {
        radio_set_number(&split->shift, 0);
        int result = radio_save(&radio_stock_storage, &split->shift);
        if (result) return result;
    }
    if (direction) {
        radio_set_number(&split->offset, delta);
        int result = radio_save(&radio_stock_storage, &split->offset);
        if (result) return result;
    }
    radio_set_number(&split->shift, direction);
    int result = radio_save(&radio_stock_storage, &split->shift);
    return result ? result : radio_system_platform.reload();
}

static __attribute__((noinline)) unsigned edit_split(const struct h9_api *api, unsigned target, struct quick_input *input,
                           const char **notice)
{
    struct split_edit split;
    if (begin_split(target, &split)) {
        *notice = "VFO READ FAILED";
        return H9_KEY_MENU;
    }
    unsigned digits = 0;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("TX FREQUENCY (MHZ)");
    ui_footer("0-9  MENU SAVE    BACK");
    for (;;) {
        char label[11];
        gfx_fill(3, 49, H9_WIDTH - 6, 16, UI_BG);
        const char *text = digits && digits < 8 ? radio_number_label(split.tx, label) : radio_frequency_label(split.tx, label);
        gfx_text(3, 49, text, 13, 2, UI_ACCENT, UI_BG);
        unsigned key = next_key(input, 1);
        if (key == KEY_EXIT || key == H9_KEY_BACK || key == KEY_HASH) return key;
        if (key == H9_KEY_MENU) {
            int result = digits && digits < 8 ? RADIO_INVALID : radio_with_rf(api, save_split, &split);
            if (!result) return key;
            gfx_text(3, 105, result == RADIO_INVALID ? "CHECK TX FREQUENCY" : "SAVE/APPLY FAILED",
                     26, 1, UI_DANGER, UI_BG);
        } else if (key >= 2 && key <= 11) {
            if (digits == 8) digits = 0;
            split.tx = (digits++ ? split.tx * 10 : 0) + key - 2;
        } else if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
            if (digits && digits < 8) split.tx = split.rx;
            if (key == H9_KEY_UP && split.tx < 60000000) split.tx += 500;
            if (key == H9_KEY_DOWN && split.tx >= 500) split.tx -= 500;
            digits = 0;
        }
    }
}

static int save_tones(void *context)
{
    struct radio_edit *edit = context;
    if (RADIO_BYTE(RECEIVE_ACTIVE) || !home_ready() || ptt_down()) return RADIO_BUSY;
    unsigned offset = edit[0].setting->offset;
    uint8_t original[4], next[4], current[4];
    for (unsigned i = 0; i < 4; ++i) {
        original[i] = edit[i / 2].original[i % 2];
        next[i] = edit[i / 2].value[i % 2];
    }
    if (radio_stock_storage.read(offset, current, 4)) return RADIO_IO_ERROR;
    unsigned changed = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (current[i] != original[i]) return RADIO_CONFLICT;
        changed |= current[i] ^ next[i];
    }
    if (!changed) return RADIO_OK;
    if (radio_stock_storage.write(offset, next, 4) ||
        radio_stock_storage.read(offset, current, 4)) return RADIO_IO_ERROR;
    for (unsigned i = 0; i < 4; ++i) if (current[i] != next[i]) return RADIO_VERIFY_ERROR;
    return radio_system_platform.reload();
}

static __attribute__((noinline)) unsigned edit_tones(const struct h9_api *api, unsigned target, struct quick_input *input,
                           const char **notice)
{
    struct radio_setting field[2];
    struct radio_edit edit[2];
    for (unsigned i = 0; i < 2; ++i) {
        if (radio_channel_field(target, RX_TONE + i, &field[i]) ||
            radio_begin(&radio_stock_storage, &field[i], &edit[i])) {
            *notice = "TONE READ FAILED";
            return H9_KEY_MENU;
        }
    }
    unsigned side = radio_value(&edit[0]) == radio_value(&edit[1]) ? 2 : 1;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_footer("# TX/RX  MENU SAVE  BACK");
    for (;;) {
        char value[RADIO_MAX_VALUE + 1];
        ui_title(side == 2 ? "RX + TX TONE" : field[side].label);
        gfx_fill(3, 49, H9_WIDTH - 6, 16, UI_BG);
        gfx_text(3, 49, radio_value_label(&edit[side != 0], value), 13, 2, UI_ACCENT, UI_BG);
        unsigned key = next_key(input, 1);
        if (key == KEY_EXIT || key == H9_KEY_BACK) return key;
        if (key == KEY_HASH) side = (side + 2) % 3;
        else if (key == H9_KEY_MENU) {
            if (radio_with_rf(api, save_tones, edit)) *notice = "SAVE/APPLY FAILED";
            return key;
        } else if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
            radio_adjust(&edit[side != 0], key == H9_KEY_UP ? 1 : -1);
            if (side == 2) radio_set_number(&edit[0], radio_value(&edit[1]));
        }
    }
}

static __attribute__((noinline)) unsigned edit_tile(const struct h9_api *api, unsigned selected, struct radio_edit *edit,
                          struct quick_input *input, const char **notice)
{
    if (tiles[selected].action == VOX) {
        unsigned gain = radio_value(edit), previous = RADIO_BYTE(VOX_PREVIOUS_GAIN);
        unsigned enabled = gain >= 1 && gain <= 5;
        if (enabled) RADIO_BYTE(VOX_PREVIOUS_GAIN) = gain;
        radio_set_number(edit, enabled ? 0 : previous >= 1 && previous <= 5 ? previous : 1);
    } else if (tiles[selected].action == CYCLE) {
        radio_adjust(edit, 1);
    } else {
        gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
        ui_title(tiles[selected].label);
        ui_footer("UP/DN   MENU SAVE   BACK");
        for (;;) {
            char value[RADIO_MAX_VALUE + 1];
            gfx_fill(3, 49, H9_WIDTH - 6, 16, UI_BG);
            gfx_text(3, 49, radio_value_label(edit, value), 13, 2, UI_ACCENT, UI_BG);
            unsigned key = next_key(input, 1);
            if (key == KEY_EXIT) return KEY_EXIT;
            if (key == H9_KEY_BACK || key == KEY_HASH) return key;
            if (key == H9_KEY_MENU) break;
            if (key == H9_KEY_UP || key == H9_KEY_DOWN) radio_adjust(edit, key == H9_KEY_UP ? 1 : -1);
            if (selected == SQUELCH_TILE && key >= 2 && key <= 11) radio_set_number(edit, key - 2);
        }
    }
    int live = selected == BRIGHT_TILE || selected == SQUELCH_TILE;
    int result = radio_with_rf(api, live ? apply_live_setting : save_and_apply, edit);
    if (result) *notice = result == RADIO_BUSY ? "RADIO BUSY" : "SAVE/APPLY FAILED";
    return H9_KEY_MENU;
}

int radio_quick_menu(const struct h9_api *api)
{
    if (!quick_allowed()) return 0;

    struct quick_input input = { .last = QUICK_TICK, .keys = { .sample = H9_KEY_BACK, .stable = H9_KEY_BACK } };
    unsigned target = active_target(), selected = 0, stock_event = 0;
    const char *notice = 0;
    for (;;) {
        draw_grid(selected, target, notice);
        notice = 0;
        unsigned key = next_key(&input, 1);
        if (key == H9_KEY_BACK || key == KEY_HASH || key == KEY_EXIT) break;
        if (key == 2) { stock_event = STOCK_MENU; break; }
        if (key == H9_KEY_UP) selected = (selected + 8) % 9;
        else if (key == H9_KEY_DOWN) selected = (selected + 1) % 9;
        else if (key == H9_KEY_MENU || (key >= 3 && key <= 11)) {
            if (key != H9_KEY_MENU) selected = key - 3;
            if (selected == TONE_TILE) {
                if (edit_tones(api, target, &input, &notice) == KEY_EXIT) break;
                continue;
            }
            if (selected == MONITOR_TILE) {
                if (target >= 200) {
                    if (edit_split(api, target, &input, &notice) == KEY_EXIT) break;
                } else if (radio_with_rf(api, toggle_monitor, 0)) notice = "RADIO BUSY";
                continue;
            }
            struct radio_setting field;
            struct radio_edit edit;
            if (tile_edit(selected, target, &field, &edit)) notice = "SETTING UNAVAILABLE";
            else if (edit_tile(api, selected, &edit, &input, &notice) == KEY_EXIT) break;
        }
    }

    /* Do not let the exit key become a new press in the stock dispatcher. */
    uint32_t start = QUICK_TICK;
    while (!ptt_down() && h9_key() != H9_KEY_NONE && QUICK_TICK - start < 100u) h9_delay(20);
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    STOCK_REDRAW(0);
    unsigned queued = RADIO_BYTE(KEY_QUEUE_COUNT);
    if (stock_event && !ptt_down() && queued < KEY_QUEUE_CAPACITY) {
        toolbox_open_radio_settings();
        RADIO_BYTE(KEY_QUEUE_FIRST + queued) = stock_event;
        RADIO_BYTE(KEY_QUEUE_COUNT) = queued + 1;
    }
    return 1;
}
