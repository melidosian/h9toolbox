#include "app_theme.h"
#include "display_layouts.h"
#include "../../menu-replacement/radio_focus.h"
#include "../../menu-replacement/radio_system.h"
#include "../../menu-replacement/radio_theme.h"

#ifndef DISPLAY_LAYOUTS_REDRAW
#define DISPLAY_LAYOUTS_REDRAW() ((void (*)(unsigned))0x01E53306u)(0)
#endif

static void preview_row(const struct channel_preview_row *row, unsigned side,
                        unsigned y, unsigned height, unsigned selected, unsigned scale)
{
    char badge[] = { 'A' + side, 0 };
    char text[10];
    uint16_t ink = selected ? UI_ACCENT : UI_DIM;
    gfx_fill(5, y, 150, height, UI_BG);
    gfx_fill(5, y, 15, height, UI_RULE);
    gfx_label(10, y + 4, badge, 7, 1, ink, UI_RULE);
    gfx_label(25, y + 4, row->name, 125, 1, UI_TEXT, UI_BG);
    gfx_label(25, y + 16, row->frequency ? radio_frequency_label(row->frequency, text) : "--",
              130, scale, ink, UI_BG);
}

static void draw_preview(const struct radio_edit *edit, unsigned active,
                         const struct channel_preview_row rows[2], const char *notice)
{
    unsigned layout = radio_value(edit);
    if (layout == RADIO_DISPLAY_FOCUS) {
        radio_channel_preview_draw(RADIO_THEME_FOCUS, active, rows);
    } else {
        gfx_fill(0, 17, H9_WIDTH, 86, UI_BG);
        if (layout == RADIO_DISPLAY_SINGLE) {
            preview_row(&rows[active], active, 36, 42, 1, 2);
        } else if (layout == RADIO_DISPLAY_DUAL) {
            preview_row(&rows[active], active, 25, 34, 1, 2);
            preview_row(&rows[active ^ 1], active ^ 1, 65, 27, 0, 1);
        } else {
            preview_row(&rows[0], 0, 20, 36, active == 0, 2);
            preview_row(&rows[1], 1, 62, 36, active == 1, 2);
        }
    }

    char name[RADIO_MAX_VALUE + 1];
    ui_title(radio_value_label(edit, name));
    gfx_label(110, UI_TITLE_Y, "PREVIEW", 47, 1, UI_DIM, UI_BG);
    gfx_fill(0, 103, H9_WIDTH, 25, UI_BG);
    gfx_label(UI_LEFT, 106, notice ? notice : "UP/DN CHANGE LAYOUT", 154, 1,
              notice ? UI_DANGER : UI_TEXT, UI_BG);
    ui_footer("MENU SAVE   BACK CANCEL");
}

struct save_layout {
    struct radio_edit *edit;
    unsigned saved;
};

static int save_layout(void *context)
{
    struct save_layout *save = context;
    int result = radio_save(&radio_stock_storage, save->edit);
    if (result != RADIO_OK) return result;
    save->saved = 1;
    return radio_system_platform.reload();
}

unsigned display_layouts(const struct h9_api *api)
{
    const struct radio_setting *setting = radio_setting_by_id("display");
    struct radio_edit edit;
    if (radio_begin(&radio_stock_storage, setting, &edit) != RADIO_OK) {
        gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
        ui_title("DISPLAY LAYOUTS");
        gfx_label(UI_LEFT, 35, "SETTINGS READ FAILED", 154, 1, UI_DANGER, UI_BG);
        ui_footer("BACK RETURN");
        gfx_wait_release();
        while (gfx_wait_key() != H9_KEY_BACK) {}
        return 0;
    }
    if (radio_value(&edit) > RADIO_DISPLAY_FOCUS)
        radio_set_number(&edit, RADIO_DISPLAY_SINGLE);

    struct channel_preview_row rows[2];
    char names[2][33];
    unsigned active = radio_channel_preview_snapshot(rows, names);
    const char *notice = 0;
    for (;;) {
        draw_preview(&edit, active, rows, notice);
        gfx_wait_release();
        unsigned key = gfx_wait_key();
        if (key == H9_KEY_BACK) return 0;
        if (key == H9_KEY_UP || key == H9_KEY_DOWN) {
            radio_adjust(&edit, key == H9_KEY_UP ? -1 : 1);
            notice = 0;
        } else if (key == H9_KEY_MENU) {
            struct save_layout save = { .edit = &edit };
            int result = radio_with_rf(api, save_layout, &save);
            if (result == RADIO_OK) {
                gfx_wait_release();
                gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
                DISPLAY_LAYOUTS_REDRAW();
                return H9_RETURN_RADIO;
            }
            if (save.saved) notice = "SAVED. RESTART TO APPLY";
            else if (result == RADIO_BUSY) notice = "RADIO BUSY. TRY AGAIN";
            else if (result == RADIO_CONFLICT) notice = "SETTINGS CHANGED. REOPEN";
            else notice = "SAVE FAILED";
        }
    }
}
