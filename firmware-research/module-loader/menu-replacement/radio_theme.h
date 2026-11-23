#ifndef H9_RADIO_THEME_H
#define H9_RADIO_THEME_H

#include <stdint.h>

enum { RADIO_THEME_SETTING = 0x6003 };
enum {
    RADIO_THEME_STOCK, RADIO_THEME_FOCUS, RADIO_THEME_SPLIT, RADIO_THEME_DAYLIGHT,
    RADIO_THEME_FOCUS_LIGHT, RADIO_THEME_DAYLIGHT_DARK, RADIO_THEME_COUNT
};

struct radio_theme {
    uint16_t background, panel, text, muted, accent, on_accent, rule;
};

const struct radio_theme *radio_theme(unsigned index);

struct channel_preview_row {
    unsigned frequency, memory;
    const char *name;
};

int radio_channel_short_name(unsigned memory, char name[33]);
int radio_channel_display_name(unsigned memory, char name[33]);
unsigned radio_channel_preview_snapshot(struct channel_preview_row rows[2], char names[2][33]);
void radio_channel_preview_draw(unsigned theme, unsigned active,
                                const struct channel_preview_row rows[2]);
int radio_channel_preview(void);

#endif
