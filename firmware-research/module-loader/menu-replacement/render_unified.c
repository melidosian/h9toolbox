/* Render the actual menu/editor C using the production font and LCD pipeline. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "h9mod.h"

static uint32_t host_tick, host_selection;
static max_align_t list_storage[128];
#define H9_RADIO_SETTINGS
#define MENU_TICK host_tick
#define MENU_SELECTION host_selection
#define MENU_LIST_STORAGE ((struct menu_list *)list_storage)
#include "../modules/menu/toolbox_menu.c"

static uint16_t pixels[H9_WIDTH * H9_HEIGHT];
static unsigned wx, wy, width, cursor;
static uint8_t settings_image[0x6100];
void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{ assert(x1 < H9_WIDTH && y1 < H9_HEIGHT); wx = x0; wy = y0; width = x1 - x0 + 1; cursor = 0; }
void h9_lcd_write(const void *buffer, unsigned n)
{
    const uint8_t *p = buffer;
    for (unsigned i = 0; i < n; i += 2, ++cursor)
        pixels[(wy + cursor / width) * H9_WIDTH + wx + cursor % width] = (unsigned)p[i] << 8 | p[i + 1];
}
unsigned h9_key(void) { return 0; }
void h9_delay(unsigned n) { host_tick += n; }
void h9_reset(void) { }
void h9_usb_loader(void) { }
int radio_editor(const struct radio_storage *storage, const struct radio_setting *setting)
{ (void)storage; (void)setting; return 0; }
int aprs_select_tx(struct ts_settings *settings) { (void)settings; return 1; }
int radio_apply_pending(const struct h9_api *api) { (void)api; return 0; }
int radio_action(const struct h9_api *api, unsigned action, uint16_t accent) { (void)api; (void)action; (void)accent; return 0; }
static int read_setting(unsigned offset, void *dst, unsigned n) { memcpy(dst, settings_image + offset, n); return 0; }
const struct radio_storage radio_stock_storage = { .read = read_setting };
const char *aprs_setting_value(enum aprs_setting s, const struct ts_settings *config, char buffer[8])
{ (void)s; (void)config; (void)buffer; return "OFF"; }
const char *aprs_setting_adjust(const struct h9_runtime_api *api, enum aprs_setting s, struct ts_settings *config, int direction)
{ (void)api; (void)s; (void)config; (void)direction; return 0; }
int aprs_settings_save(const struct h9_api *loader, const struct h9_runtime_api *api, const struct ts_settings *config, struct ts_settings *saved)
{ (void)loader; (void)api; (void)config; (void)saved; return 0; }
static const char names[][H9_NAME_BYTES] = {
    "MENU", "APRS", "APRS BEACON", "GPS", "SAT PASS", "ABOUT", "MAZE CHASE", "CUSTOM APP", "APRS MONITOR"
};
static void bundle_read(void *dst, uint32_t address, uint32_t n)
{
    if (!address && n == H9_BUNDLE_HEADER) { memset(dst, 0, n); return; }
    assert(n == H9_NAME_BYTES);
    memcpy(dst, names[(address - H9_BUNDLE_HEADER) / H9_BUNDLE_ENTRY], n);
}
static void frame(const char *folder, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", folder, name);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n160 128\n255\n");
    for (unsigned i = 0; i < H9_WIDTH * H9_HEIGHT; ++i) {
        unsigned c = pixels[i];
        fputc((c >> 11) * 255 / 31, f); fputc(((c >> 5) & 63) * 255 / 63, f); fputc((c & 31) * 255 / 31, f);
    }
    fclose(f);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    settings_image[0xca9] = 5; /* logical squelch offset */
    const struct h9_api loader = { .version = 5, .read = bundle_read };
    struct menu_list list = {0};
    struct menu_state state = { .digit = NO_DIGIT };
    struct menu_view view;
    build_list(&list, &loader, sizeof names / sizeof *names);
    settings_image[radio_settings[RADIO_MENU_COLOR].offset] = 7;
    state.accent = saved_menu_color();
    menu_open(&list, &state, &view, MENU_HOME, 0);
    draw_menu(&loader, &list, &state, &view);
    frame(argv[1], "unified-home");
    menu_open(&list, &state, &view, APRS_FOLDER, 1);
    draw_menu(&loader, &list, &state, &view);
    frame(argv[1], "unified-aprs");

    menu_open(&list, &state, &view, APRS_BEACON_PAGE, 0);
    state.editing = 1;
    state.beacon_draft = BEACON_SMART;
    draw_menu(&loader, &list, &state, &view);
    frame(argv[1], "beacon-type");

    menu_open(&list, &state, &view, MESSAGES, 0);
    state.editing = 0;
    draw_menu(&loader, &list, &state, &view);
    frame(argv[1], "messages");
}
