#include "app_theme.h"
#include "channel_labels_native.h"
#include "radio_channels.h"
#include "radio_theme.h"

#define RGB(r, g, b) (((r) >> 3 << 11) | ((g) >> 2 << 5) | ((b) >> 3))

/* Colors sampled from the original 160 x 128 concepts. */
static const struct radio_theme themes[] = {
    { RGB(8,12,16), RGB(16,32,41), RGB(247,247,247), RGB(165,182,189),
      RGB(123,235,214), RGB(8,32,33), RGB(49,65,74) },
    { RGB(8,12,16), RGB(24,28,33), RGB(247,247,247), RGB(181,186,198),
      RGB(255,207,115), RGB(33,24,8), RGB(74,81,90) },
    { RGB(231,239,222), RGB(214,227,206), RGB(16,40,33), RGB(66,89,74),
      RGB(33,81,66), RGB(247,247,239), RGB(165,186,165) },
    { RGB(247,247,247), RGB(231,239,239), RGB(16,40,49), RGB(74,97,107),
      RGB(0,109,90), RGB(247,255,255), RGB(198,211,206) },
    { RGB(16,24,16), RGB(24,40,33), RGB(239,247,239), RGB(173,190,181),
      RGB(173,219,165), RGB(16,36,24), RGB(57,81,66) },
};
_Static_assert(sizeof themes / sizeof *themes == RADIO_THEME_COUNT - 1, "theme choices");

const struct radio_theme *radio_theme(unsigned index)
{
    return index > RADIO_THEME_STOCK && index < RADIO_THEME_COUNT ? &themes[index - 1] : 0;
}

enum { CHANNEL_TEXT_WIDTH = 150 };

static const char *skip_spaces(const char *text)
{
    while (*text == ' ') ++text;
    return text;
}

static void draw_focus_name(const struct radio_theme *theme, unsigned y, const char *name,
                            unsigned scale, uint16_t bg)
{
    unsigned end = gfx_label_fit(name, CHANNEL_TEXT_WIDTH / scale);
    if (!name[end] || scale == 1) {
        gfx_label(5, y, name, CHANNEL_TEXT_WIDTH, scale, theme->text, bg);
        return;
    }

    scale = 2;
    end = gfx_label_fit(name, CHANNEL_TEXT_WIDTH / scale);
    if (!name[end]) {
        gfx_label(5, y, name, CHANNEL_TEXT_WIDTH, scale, theme->text, bg);
        return;
    }

    /* Prefer a word boundary, but use all available space if a word is too
     * wide. Two normal-size lines can fit even a 32-character name. */
    unsigned cut = end;
    while (cut && name[cut] != ' ') --cut;
    if (!cut) cut = end;
    const char *second = skip_spaces(name + cut);
    if (second[gfx_label_fit(second, CHANNEL_TEXT_WIDTH / scale)]) {
        cut = end;
        second = skip_spaces(name + cut);
        if (second[gfx_label_fit(second, CHANNEL_TEXT_WIDTH / scale)]) scale = 1;
    }

    char first[33];
    for (unsigned i = 0; i < cut; ++i) first[i] = name[i];
    first[cut] = 0;
    gfx_label(5, y, first, CHANNEL_TEXT_WIDTH, scale, theme->text, bg);
    gfx_label(5, y + 8 * scale + 2, second, CHANNEL_TEXT_WIDTH, scale, theme->text, bg);
}

static void draw_channel(const struct radio_theme *theme, unsigned y, unsigned height,
                         const struct channel_preview_row *row, unsigned side,
                         unsigned active, unsigned scale, unsigned band, unsigned focus)
{
    char id[] = {'A' + side, 0};
    char memory[] = {'C', 'H', '0' + row->memory / 100, '0' + row->memory / 10 % 10,
                     '0' + row->memory % 10, 0};
    char frequency[10];
    unsigned badge_width = band ? H9_WIDTH : 18;
    uint16_t bg = active ? theme->panel : theme->background;
    uint16_t header = active ? theme->accent : theme->rule;
    uint16_t ink = active ? theme->on_accent : theme->text;

    gfx_fill(0, y, H9_WIDTH, height, bg);
    gfx_fill(0, y, badge_width, 13, header);
    gfx_label(6, y + 3, id, 6, 1, ink, header);
    gfx_label(25, y + 3, row->memory ? memory : "VFO", 30, 1,
              band ? ink : theme->muted, band ? header : bg);

    const char *freq = row->frequency ? radio_frequency_label(row->frequency, frequency) : "--";
    const char *name = skip_spaces(row->name);
    if (focus && row->memory && *name) {
        unsigned count = 0;
        while (freq[count]) ++count;
        gfx_text(156 - count * 6, y + 3, freq, count, 1, theme->muted, bg);
        draw_focus_name(theme, y + 17, name, scale, bg);
    } else {
        if (!focus)
            gfx_label(63, y + (scale > 1 ? 3 : 17), row->name, 93, 1,
                      band ? ink : theme->text, band ? header : bg);
        gfx_label(5, y + 17, freq, focus ? H9_WIDTH - 5 : CHANNEL_TEXT_WIDTH, scale,
                  theme->text, bg);
    }
}

void radio_channel_preview_draw(unsigned index, unsigned active, const struct channel_preview_row rows[2])
{
    const struct radio_theme *theme = radio_theme(index);
    if (!theme) return;
    unsigned split = index == RADIO_THEME_SPLIT;
    unsigned focus = index == RADIO_THEME_FOCUS || index == RADIO_THEME_FOCUS_LIGHT;
    unsigned band = index == RADIO_THEME_DAYLIGHT || index == RADIO_THEME_DAYLIGHT_DARK;
    unsigned first = split ? 0 : active;
    unsigned second = first ^ 1;
    unsigned first_height = split ? 43 : 57;

    gfx_fill(0, 0, H9_WIDTH, 17, theme->background);
    gfx_label(5, 4, "CHANNEL SCREEN PREVIEW", 150, 1, theme->text, theme->background);
    gfx_fill(0, 16, H9_WIDTH, 1, theme->rule);
    draw_channel(theme, 17, first_height, &rows[first], first, first == active, split ? 2 : 3, band, focus);
    gfx_fill(0, 17 + first_height, H9_WIDTH, 1, theme->rule);
    draw_channel(theme, 18 + first_height, 103 - (18 + first_height), &rows[second], second,
                 second == active, split ? 2 : 1, 0, focus);
    gfx_fill(0, 103, H9_WIDTH, 25, theme->panel);
    gfx_fill(0, 103, H9_WIDTH, 1, theme->rule);
    gfx_label(5, 107, "UP/DN THEME   BACK MENU", 150, 1, theme->text, theme->panel);
    static const char *const names[] = {"FOCUS", "SPLIT", "DAYLIGHT", "FOCUS LIGHT", "DAYLIGHT DARK"};
    gfx_label(5, 118, names[index - 1], 150, 1, theme->muted, theme->panel);
}

#define RADIO_BYTE(address) (*(const volatile uint8_t *)(uintptr_t)(address))
#define RADIO_WORD(address) (*(const volatile uint32_t *)(uintptr_t)(address))

int radio_channel_preview(void)
{
    struct channel_preview_row rows[2];
    char names[2][33] = {{0}};
    unsigned active = RADIO_BYTE(0x11B50u) >> 7;
    for (unsigned side = 0; side < 2; ++side) {
        unsigned memory = radio_memory_channel(RADIO_BYTE, side);
        if (memory > 199) memory = 0;
        if (memory && (radio_label_get(memory, names[side]) || !names[side][0])) {
            struct radio_setting setting;
            struct radio_edit edit;
            char label[RADIO_MAX_VALUE + 1];
            names[side][0] = 0;
            if (!radio_channel_field(memory, 12, &setting) &&
                !radio_begin(&radio_stock_storage, &setting, &edit)) {
                radio_value_label(&edit, label);
                for (unsigned i = 0; i <= setting.length; ++i) {
                    names[side][i] = label[i];
                    if (!label[i]) break;
                }
            }
        }
        rows[side] = (struct channel_preview_row){
            RADIO_WORD(0x11CA0u + 8u * side), memory, names[side]
        };
    }

    uint8_t index;
    if (radio_stock_storage.read(RADIO_THEME_SETTING, &index, 1) || !radio_theme(index))
        index = RADIO_THEME_FOCUS;
    for (;;) {
        radio_channel_preview_draw(index, active, rows);
        gfx_wait_release();
        unsigned key = gfx_wait_key();
        if (key == H9_KEY_BACK) return RADIO_OK;
        if (key == H9_KEY_UP)
            index = index + 1 < RADIO_THEME_COUNT ? index + 1 : RADIO_THEME_FOCUS;
        if (key == H9_KEY_DOWN)
            index = index > RADIO_THEME_FOCUS ? index - 1 : RADIO_THEME_COUNT - 1;
    }
}
