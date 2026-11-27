#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "radio_33cm.h"

static unsigned char ram[0x29A00], image[RADIO_33CM_SETTING + 1];
static unsigned tick, redraws, reads, writes, reloads, busy, settings_opens, backlight_updates, squelch_refreshes;
static const unsigned char *keys;

static unsigned adc(unsigned channel);
static void redraw(unsigned arg);
static void monitor(unsigned on);
#define STOCK_SQUELCH_REFRESH() (++squelch_refreshes)
#define STOCK_BACKLIGHT() (++backlight_updates)
#define STOCK_MONITOR_ON() monitor(1)
#define STOCK_MONITOR_OFF() monitor(0)
#define RADIO_BYTE(address) ram[address]
#define QUICK_TICK tick
#define STOCK_ADC adc
#define STOCK_REDRAW redraw
#include "radio_quick.c"
#include "radio_settings_catalog.h"

enum { PTT = 254 };
/* Each scripted key stays down for two 20 ms polls. */
#define PRESS(key) key, key
#define RELEASE PRESS(0)

static unsigned adc(unsigned channel)
{
    assert(channel == 3);
    return *keys == PTT ? 100 : 993;
}
static void redraw(unsigned arg) { assert(!arg); ++redraws; }
static void monitor(unsigned on) { ram[MONITOR_FLAG] = on; }
void toolbox_open_radio_settings(void) { ++settings_opens; }
unsigned h9_key(void) { return *keys == PTT ? 0 : *keys++; }
void h9_delay(unsigned ms) { tick += ms / 10; }
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    (void)color;
    assert(x + w <= H9_WIDTH && y + h <= H9_HEIGHT);
}
void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
    assert(x0 < x1 && y0 < y1 && x1 < H9_WIDTH && y1 < H9_HEIGHT);
}
void h9_lcd_write(const void *buffer, unsigned length)
{
    assert(buffer && length == 128);
}
unsigned gfx_text(unsigned x, unsigned y, const char *text, unsigned max, unsigned scale,
                  uint16_t fg, uint16_t bg)
{
    (void)fg;
    (void)bg;
    unsigned n = strlen(text);
    if (n > max) n = max;
    assert(x + n * scale * 6 <= H9_WIDTH && y + scale * 8 <= H9_HEIGHT);
    return x + n * scale * 6;
}
const struct radio_setting *radio_setting_by_id(const char *id)
{
    for (unsigned i = 0; i < RADIO_SETTING_COUNT; ++i)
        if (!strcmp(id, radio_settings[i].id)) return &radio_settings[i];
    return 0;
}
static int read(unsigned offset, void *dst, unsigned length)
{
    assert(offset + length <= sizeof image);
    ++reads;
    memcpy(dst, image + offset, length);
    return RADIO_OK;
}
static int write(unsigned offset, const void *src, unsigned length)
{
    assert(offset + length <= sizeof image);
    ++writes;
    memcpy(image + offset, src, length);
    return RADIO_OK;
}
static int reload(void) { ++reloads; return RADIO_OK; }
const struct radio_storage radio_stock_storage = {read, write};
const struct radio_system_platform radio_system_platform = {.reload = reload};

int radio_with_rf(const struct h9_api *api, radio_rf_action action, void *context)
{
    (void)api;
    return busy ? RADIO_BUSY : action(context);
}

static void run(const unsigned char *input)
{
    keys = input;
    ram[KEY_QUEUE_COUNT] = 1;
    ram[KEY_QUEUE_FIRST] = 33;
    if (radio_quick_menu(0)) {
        unsigned left = --ram[KEY_QUEUE_COUNT];
        memmove(ram + KEY_QUEUE_FIRST, ram + KEY_QUEUE_FIRST + 1, left);
    }
}

int main(void)
{
    static const unsigned char back[] = { PRESS(H9_KEY_BACK), RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT };
    static const unsigned char resume[] = { RELEASE, PRESS(H9_KEY_4), RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT };
    static const unsigned char bright[] = {
        RELEASE, PRESS(11), RELEASE, PRESS(11), RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT
    };
    static const unsigned char squelch[] = {
        RELEASE, PRESS(H9_KEY_2), RELEASE, PRESS(11), RELEASE, PRESS(H9_KEY_MENU),
        RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT
    };
    static const unsigned char cancel[] = {
        RELEASE, PRESS(H9_KEY_2), RELEASE, PRESS(5), RELEASE, PRESS(H9_KEY_BACK),
        RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT
    };
    static const unsigned char power[] = { RELEASE, PRESS(H9_KEY_MENU), RELEASE, PRESS(KEY_HASH), RELEASE, PTT };
    static const unsigned char ptt[] = { RELEASE, PRESS(H9_KEY_2), RELEASE, PRESS(5), PTT };
    ram[0x11AB0] = 1;
    ram[0x11AC6] = 3;

    ram[0x11AC2] = 1;
    run(back);
    assert(ram[KEY_QUEUE_COUNT] == 1 && !reads && !redraws);
    ram[0x11AC2] = 0;
    ram[0xC140] = 1;
    run(back);
    assert(ram[KEY_QUEUE_COUNT] == 1 && !reads && !redraws);
    ram[0xC140] = 0;
    run(back);
    assert(!ram[KEY_QUEUE_COUNT] && !writes && redraws == 1);
    ram[0x11AB0] = 0;
    run(back);
    assert(!ram[KEY_QUEUE_COUNT] && redraws == 2);
    ram[0x11AB0] = 1;
    run(squelch);
    assert(image[3241] == 9 && writes == 1 && squelch_refreshes == 1 && !reloads);
    run(cancel);
    run(ptt);
    assert(image[3241] == 9 && writes == 1);
    busy = 1;
    run(power);
    assert(writes == 1);
    busy = 0;

    /* A-side VFO power preserves width/shift bits. B-side memory edits its
     * own channel record instead of the A or B VFO. */
    image[0x195e] = 0x0a;
    run(power);
    assert(image[0x195e] == 0x1a && image[0x196e] == 0);
    ram[0x11B50] = 0x80;
    ram[0x11AEA] = ram[0x12BB3] = 1;
    ram[0x11AF0] = 42;
    run(power);
    assert(image[42 * 16 + 14] == 0x10 && image[0x196e] == 0);
    static const unsigned char settings[] = { RELEASE, PRESS(2), RELEASE, PTT };
    static const unsigned char vox[] = { RELEASE, PRESS(8), RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT };
    static const unsigned char mon[] = { RELEASE, PRESS(7), RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT };
    static const unsigned char tone[] = {
        RELEASE, PRESS(H9_KEY_3), RELEASE, PRESS(H9_KEY_UP), RELEASE,
        PRESS(KEY_HASH), RELEASE, PRESS(H9_KEY_UP), RELEASE, PRESS(H9_KEY_MENU),
        RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT
    };
    static const unsigned char split[] = {
        RELEASE, PRESS(7), RELEASE,
        PRESS(3), RELEASE, PRESS(6), RELEASE, PRESS(7), RELEASE, PRESS(8), RELEASE,
        PRESS(2), RELEASE, PRESS(2), RELEASE, PRESS(2), RELEASE, PRESS(2), RELEASE,
        PRESS(H9_KEY_MENU), RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT
    };
    run(settings);
    assert(settings_opens == 1 && ram[KEY_QUEUE_COUNT] == 1 && ram[KEY_QUEUE_FIRST] == STOCK_MENU);
    image[3239] = 0x43;
    run(vox);
    assert(image[3239] == 0x40);
    run(vox);
    assert(image[3239] == 0x43);
    run(mon);
    assert(ram[MONITOR_FLAG] == 1);
    ram[RECEIVE_ACTIVE] = 1;
    ram[0x11AFD] = ram[0x11B23] = 1;
    ram[0x11AC6] = 2;
    unsigned receive_writes = writes, receive_draws = redraws;
    run(power);
    assert(writes == receive_writes && redraws == receive_draws + 1);
    run(mon);
    assert(ram[MONITOR_FLAG] == 0);
    ram[RECEIVE_ACTIVE] = 0;
    ram[0x11AFD] = ram[0x11B23] = 0;
    ram[0x11AC6] = 3;
    keys = back;
    ram[KEY_QUEUE_COUNT] = 2;
    ram[KEY_QUEUE_FIRST] = 33;
    ram[KEY_QUEUE_FIRST + 1] = 34;
    assert(radio_quick_menu(0));
    assert(ram[KEY_QUEUE_COUNT] == 2 && ram[KEY_QUEUE_FIRST + 1] == 34);
    memset(image + 42 * 16 + 8, 0xff, 4);
    run(tone);
    /* RX and TX started together. After switching to TX only the RX tone
     * stays at the first choice and TX advances to the second. */
    assert(image[42 * 16 + 8] == 0x70 && image[42 * 16 + 10] == 0x93);

    ram[0x11AEA] = 0;
    image[0x1962] = 0x50;
    image[0x1963] = 0x14;
    run(split);
    assert((image[0x196e] & 3) == 2);
    assert(image[0xcb4] == 0 && image[0xcb5] == 0 && image[0xcb6] == 0x06 && image[0xcb7] == 0);
    struct split_edit draft;
    assert(begin_split(201, &draft) == RADIO_OK && draft.tx == 14560000);
    unsigned saved_writes = writes;
    keys = back;
    draft.tx = 44560000;
    assert(save_split(&draft) == RADIO_INVALID && writes == saved_writes);
    draft.tx = 14440000;
    assert(save_split(&draft) == RADIO_OK && (image[0x196e] & 3) == 1);
    assert(begin_split(201, &draft) == RADIO_OK && draft.tx == 14440000);
    draft.tx = draft.rx;
    assert(save_split(&draft) == RADIO_OK && (image[0x196e] & 3) == 0);
    image[0x1963] = 0x91;
    assert(begin_split(201, &draft) == RADIO_OK && draft.rx == 91500000);
    draft.tx = 92700000;
    assert(save_split(&draft) == RADIO_INVALID);
    image[RADIO_33CM_SETTING] = 1;
    assert(save_split(&draft) == RADIO_OK && (image[0x196e] & 3) == 2);
    assert(begin_split(201, &draft) == RADIO_OK && draft.tx == 92700000);
    draft.tx = 90199999;
    assert(save_split(&draft) == RADIO_INVALID);
    image[3233] = 0x15;
    run(resume);
    assert(image[3233] == 0x55 && !ram[KEY_QUEUE_COUNT]);
    image[3229] = 0; /* level 5 */
    unsigned saved_reloads = reloads;
    run(bright);
    assert(image[3229] == 3 && ram[STOCK_BRIGHTNESS] == 1 && backlight_updates == 2);
    assert(reloads == saved_reloads);
    char label[RADIO_MAX_VALUE + 1];
    assert(!strcmp(grid_value(BRIGHT_TILE, 201, label), "2"));
    static const unsigned char open_squelch_to_one[] = {
        RELEASE, PRESS(H9_KEY_2), RELEASE, PRESS(H9_KEY_1), RELEASE, PRESS(H9_KEY_MENU),
        RELEASE, PRESS(H9_KEY_BACK), RELEASE, PTT
    };
    unsigned before_squelch_reload = squelch_refreshes;
    image[SQUELCH_SETTING_OFFSET] = 0;
    ram[STOCK_SQUELCH_LEVEL] = 0;
    ram[RECEIVE_ACTIVE] = 1;
    ram[0x11AC6] = 2;
    run(open_squelch_to_one);
    assert(image[SQUELCH_SETTING_OFFSET] == 1 && ram[STOCK_SQUELCH_LEVEL] == 1);
    assert(squelch_refreshes == before_squelch_reload + 1 && reloads == saved_reloads);
    puts("Quick Change input and storage passed");
}
