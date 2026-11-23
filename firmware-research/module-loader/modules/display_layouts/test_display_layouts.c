#define H9_GFX_TEST_STUB
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../menu-replacement/radio_settings_catalog.h"
#include "../../menu-replacement/radio_theme.h"
#include "../../menu-replacement/radio_system.h"

static unsigned redraws, focus_previews, snapshots;
#define DISPLAY_LAYOUTS_REDRAW() (++redraws)
#include "display_layouts.c"
#include "display_layouts_app.c"

static uint8_t layout;
static const unsigned *keys;
static int read_error, write_error, reload_error, busy, conflict;
static unsigned writes, reloads, notices;

enum { READ_ERROR = 1, WRITE_ERROR = 2, BUSY = 4, CONFLICT = 8, RESTART = 16, OLD_FIRMWARE = 32 };

const struct radio_setting *radio_setting_by_id(const char *id)
{
    assert(!strcmp(id, "display"));
    return &radio_settings[RADIO_DISPLAY_LAYOUT];
}

static int read_layout(unsigned offset, void *dst, unsigned count)
{
    assert(offset == radio_setting_by_id("display")->offset && count == 1);
    if (read_error) return RADIO_IO_ERROR;
    *(uint8_t *)dst = layout;
    return RADIO_OK;
}

static int write_layout(unsigned offset, const void *src, unsigned count)
{
    assert(offset == radio_setting_by_id("display")->offset && count == 1);
    if (write_error) return RADIO_IO_ERROR;
    layout = *(const uint8_t *)src;
    ++writes;
    return RADIO_OK;
}

const struct radio_storage radio_stock_storage = { read_layout, write_layout };

static int reload(void)
{
    ++reloads;
    return reload_error ? RADIO_IO_ERROR : RADIO_OK;
}

const struct radio_system_platform radio_system_platform = { .reload = reload };

int radio_with_rf(const struct h9_api *api, radio_rf_action action, void *context)
{
    assert(api);
    if (busy) return RADIO_BUSY;
    if (conflict) layout = RADIO_DISPLAY_DUAL;
    return action(context);
}

unsigned radio_channel_preview_snapshot(struct channel_preview_row rows[2], char names[2][33])
{
    ++snapshots;
    strcpy(names[0], "NORTH RIDGE LINK REPEATER");
    names[1][0] = 0;
    rows[0] = (struct channel_preview_row){14694000, 24, names[0]};
    rows[1] = (struct channel_preview_row){44600000, 0, names[1]};
    return 0;
}

void radio_channel_preview_draw(unsigned theme, unsigned active, const struct channel_preview_row rows[2])
{
    assert(theme == RADIO_THEME_FOCUS && active == 0 && rows[0].memory == 24);
    ++focus_previews;
}

void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    assert(x + w <= H9_WIDTH && y + h <= H9_HEIGHT);
    (void)color;
}

unsigned gfx_text(unsigned x, unsigned y, const char *text, unsigned max, unsigned scale,
                  uint16_t fg, uint16_t bg)
{
    (void)fg;
    (void)bg;
    assert(x < H9_WIDTH && y + scale * 8 <= H9_HEIGHT);
    if (!strcmp(text, "SETTINGS READ FAILED")) notices |= READ_ERROR;
    if (!strcmp(text, "SAVE FAILED")) notices |= WRITE_ERROR;
    if (!strcmp(text, "RADIO BUSY. TRY AGAIN")) notices |= BUSY;
    if (!strcmp(text, "SETTINGS CHANGED. REOPEN")) notices |= CONFLICT;
    if (!strcmp(text, "SAVED. RESTART TO APPLY")) notices |= RESTART;
    if (!strcmp(text, "UPDATE H9 TOOLBOX FIRMWARE")) notices |= OLD_FIRMWARE;
    unsigned count = strlen(text);
    return x + 6 * scale * (max < count ? max : count);
}

void gfx_wait_release(void) {}
unsigned gfx_wait_key(void) { assert(*keys); return *keys++; }

int main(void)
{
    const struct h9_api api = {
        .version = H9_DISPLAY_LAYOUTS_API,
        .display_layouts_magic = H9_DISPLAY_LAYOUTS_MAGIC,
        .display_layouts = display_layouts
    };
    const struct h9_api legacy = {
        .version = H9_DISPLAY_LAYOUTS_API,
        .display_layouts = display_layouts
    };
    static const unsigned back[] = { H9_KEY_BACK };
    static const unsigned preview[] = { H9_KEY_UP, H9_KEY_DOWN, H9_KEY_BACK };
    static const unsigned choose_focus[] = { H9_KEY_UP, H9_KEY_MENU, H9_KEY_BACK };
    static const unsigned choose_single[] = { H9_KEY_DOWN, H9_KEY_MENU, H9_KEY_BACK };
    keys = back;
    assert(module_main(&legacy, 1) == 0 && !snapshots && notices == OLD_FIRMWARE);

    keys = preview;
    assert(module_main(&api, 1) == 0 && snapshots == 1 && focus_previews == 1);
    assert(layout == RADIO_DISPLAY_SINGLE && !writes && !redraws);

    busy = 1;
    keys = choose_focus;
    assert(module_main(&api, 1) == 0 && !writes && !reloads && (notices & BUSY));
    busy = 0;
    write_error = 1;
    keys = choose_focus;
    assert(module_main(&api, 1) == 0 && !writes && !reloads && (notices & WRITE_ERROR));
    write_error = 0;
    conflict = 1;
    keys = choose_focus;
    assert(module_main(&api, 1) == 0 && !writes && !reloads && (notices & CONFLICT));
    conflict = 0;

    layout = RADIO_DISPLAY_SINGLE;
    keys = choose_focus;
    assert(module_main(&api, 1) == H9_RETURN_RADIO && layout == RADIO_DISPLAY_FOCUS);
    assert(writes == 1 && reloads == 1 && redraws == 1);

    reload_error = 1;
    keys = choose_single;
    assert(module_main(&api, 1) == 0 && layout == RADIO_DISPLAY_SINGLE && (notices & RESTART));
    assert(writes == 2 && reloads == 2 && redraws == 1);
    reload_error = 0;

    read_error = 1;
    keys = back;
    assert(module_main(&api, 1) == 0 && (notices & READ_ERROR));

    puts("Display Layouts app: previews, cancel, save, errors, and loader compatibility passed.");
    return 0;
}
