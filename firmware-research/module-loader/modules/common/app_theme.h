#ifndef H9_APP_THEME_H
#define H9_APP_THEME_H

#include "gfx.h"
#include "h9mod.h"

/* RGB565 for Toolbox screens. Game pieces and plot data can use their own colors. */
#define UI_BG      0x0000u
#define UI_TEXT    0xffffu
#define UI_DIM     0x8410u
#define UI_ACCENT  0xffe0u
#define UI_EDIT    UI_TEXT
#define UI_HINT    0x07ffu
#define UI_OK      0x07e0u
#define UI_DANGER  0xf800u
#define UI_RULE    0x4208u

enum { UI_LEFT = 3, UI_TITLE_Y = 4, UI_TITLE_H = 17, UI_FOOT_Y = 118, UI_CELLS = 26 };

unsigned menu_luma(uint16_t rgb);
uint16_t menu_ink(uint16_t accent);

static inline void ui_title(const char *text)
{
    gfx_fill(0, 0, H9_WIDTH, UI_TITLE_H, UI_BG);
    gfx_text(UI_LEFT, UI_TITLE_Y, text, UI_CELLS, 1, UI_ACCENT, UI_BG);
}

static inline void ui_footer(const char *keys)
{
    gfx_fill(0, UI_FOOT_Y - 1, H9_WIDTH, H9_HEIGHT - UI_FOOT_Y + 1, UI_BG);
    gfx_text(UI_LEFT, UI_FOOT_Y, keys, UI_CELLS, 1, UI_HINT, UI_BG);
}

#endif
