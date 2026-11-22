#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "radio_channels.h"
#include "channel_labels_native.h"
#include "h9mod.h"

static unsigned samples[512], sample_at, sample_count;
#define CHANNEL_TICK (sample_at * 2u)
#include "radio_channel_editor.c"
enum { TEST_ACCENT = 0x001f };
static char selected_number[4];
static unsigned full_fills, row_fills, header_draws, footer_draws, notice_clears;
static unsigned saw_short_mhz, saw_precise_mhz;
static uint8_t image[0x6000];
static unsigned writes, separate_editors, check_value, saw_vfo, saw_name, saw_long_name, saw_color;
static int read_image(unsigned offset, void *dst, unsigned n) { memcpy(dst, image + offset, n); return 0; }
static int write_image(unsigned offset, const void *src, unsigned n) { memcpy(image + offset, src, n); ++writes; return 0; }
static const struct radio_storage storage = { read_image, write_image };
static unsigned long_edits;
static char stored_label[32];
static int read_label(unsigned channel, void *dst, unsigned n)
{ assert(channel && channel <= 199 && n == 32); if (channel == 1) memcpy(dst, stored_label, n); else memset(dst, 0, n); return 0; }
static int write_label(unsigned channel, const void *src, unsigned n)
{ assert(channel && channel <= 199 && n == 32); memcpy(stored_label, src, n); return 0; }
const struct radio_storage radio_long_name_storage = { read_label, write_label };
int radio_editor(const struct radio_storage *s, const struct radio_setting *field)
{
    assert(field->kind == RADIO_TEXT);
    if (s == &radio_long_name_storage) {
        assert(field->length == 32 && field->offset == 1);
        ++long_edits;
        memcpy(stored_label, "Long label 123456789012345678901", sizeof stored_label);
    } else {
        assert(s == &storage && field->length == 8);
        ++separate_editors;
    }
    return RADIO_OK;
}
static void use_keys(const unsigned *keys, unsigned count, int hold)
{
    sample_at = sample_count = 0;
    full_fills = row_fills = header_draws = footer_draws = notice_clears = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned key = keys[i];
        unsigned ticks = hold && (key == H9_KEY_UP || key == H9_KEY_DOWN) ? (unsigned)hold : 3;
        assert(sample_count + 2 + ticks < sizeof samples / sizeof *samples);
        samples[sample_count++] = 0;
        samples[sample_count++] = 0;
        while (ticks--) samples[sample_count++] = key;
    }
}
#define USE_KEYS(keys, hold) use_keys(keys, sizeof keys / sizeof *keys, hold)
unsigned h9_key(void) { assert(sample_at < sample_count); return samples[sample_at]; }
void h9_delay(unsigned ticks) { assert(ticks == 20); ++sample_at; }
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    (void)color;
    full_fills += x == 0 && y == 0 && w == H9_WIDTH && h == H9_HEIGHT;
    row_fills += x == 0 && y >= 40 && y < 100 && w == H9_WIDTH && h == 12;
    notice_clears += x == 0 && y == 105 && w == H9_WIDTH && h == 8;
    assert(x + w <= H9_WIDTH && y + h <= H9_HEIGHT);
}
unsigned gfx_text(unsigned x, unsigned y, const char *s, unsigned max, unsigned scale, uint16_t fg, uint16_t bg)
{
    header_draws += y == UI_TITLE_Y;
    footer_draws += y == UI_FOOT_Y;
    check_value += !strcmp(s, "CHECK VALUE");
    saw_short_mhz |= !strcmp(s, "142.230");
    saw_precise_mhz |= !strcmp(s, "142.23456");
    saw_vfo |= !strcmp(s, "VFO A") || !strcmp(s, "VFO B");
    saw_name |= x == 33 && !strcmp(s, "LOCAL");
    saw_long_name |= x == 33 && max == 21 && !strcmp(s, "A really long channel name");
    if (x == 3 && y >= 42 && y < 102 && max == 3 && bg == TEST_ACCENT) {
        assert(fg == 0xffff);
        memcpy(selected_number, s, 4);
        saw_color = 1;
    }
    unsigned n = 0; while (n < max && s[n]) ++n;
    assert(x + n * 6 * scale <= H9_WIDTH && y + 8 * scale <= H9_HEIGHT);
    return x + n * 6 * scale;
}
int main(void)
{
    memcpy(image + 0xd40, "LOCAL", 6);
    const unsigned frequency[] = { 2, 2, 3, H9_KEY_MENU, H9_KEY_MENU, 3, 6, 7, 7, 4, 7, 2, 2,
        H9_KEY_MENU, H9_KEY_BACK, H9_KEY_BACK };
    USE_KEYS(frequency, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == -1 && writes == 1 && !separate_editors);
    assert(!saw_vfo && saw_name && saw_color);
    const uint8_t expected[] = {0, 0x25, 0x55, 0x14};
    assert(!memcmp(image + 16, expected, 4));
    const unsigned cancel[] = { H9_KEY_MENU, H9_KEY_MENU, H9_KEY_UP, H9_KEY_BACK, H9_KEY_BACK, H9_KEY_BACK };
    USE_KEYS(cancel, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == RADIO_OK && writes == 1 && !separate_editors);
    assert(!memcmp(image + 16, expected, 4));
    const unsigned last_channel[] = { 3,11,11,H9_KEY_MENU,H9_KEY_MENU,3,6,7,7,4,7,2,2,
        H9_KEY_MENU,H9_KEY_BACK,H9_KEY_BACK };
    USE_KEYS(last_channel, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == -1 && writes == 2 && !separate_editors);
    assert(!memcmp(image + 199 * 16, expected, 4));
    const unsigned switch_channel[] = { 3,11,11,H9_KEY_MENU,H9_KEY_BACK,H9_KEY_UP,H9_KEY_MENU,
        H9_KEY_MENU,H9_KEY_UP,H9_KEY_MENU,H9_KEY_BACK,H9_KEY_BACK };
    USE_KEYS(switch_channel, 0); /* Back returns to 199, then Up selects 198. */
    assert(radio_channel_editor(&storage, TEST_ACCENT) == -1 && writes == 3);
    assert(!memcmp(image + 16, expected, 4) && !memcmp(image + 199 * 16, expected, 4));
    assert(image[198 * 16 + 2] == 0x80 && image[198 * 16 + 3] == 0x01);
    const unsigned names[] = {
        H9_KEY_MENU, H9_KEY_UP, H9_KEY_UP, H9_KEY_UP, H9_KEY_UP, H9_KEY_UP, H9_KEY_UP,
        H9_KEY_MENU, H9_KEY_UP, H9_KEY_MENU, H9_KEY_BACK, H9_KEY_BACK
    };
    USE_KEYS(names, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == RADIO_OK && long_edits == 1 && separate_editors == 1);
    assert(stored_label[31] && image[0xd40] == 'L');
    const unsigned delete_channel[] = { H9_KEY_MENU, H9_KEY_UP, H9_KEY_MENU, H9_KEY_MENU, H9_KEY_BACK, H9_KEY_BACK };
    USE_KEYS(delete_channel, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == -1 && !stored_label[0]);
    memcpy(stored_label, "A really long channel name", sizeof "A really long channel name");
    const unsigned scroll_down[] = { H9_KEY_DOWN, H9_KEY_BACK };
    USE_KEYS(scroll_down, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == RADIO_OK && !strcmp(selected_number, "002"));
    assert(full_fills == 1 && header_draws == 1 && footer_draws == 1 && row_fills == 10);
    const unsigned scroll_view[] = { H9_KEY_DOWN, H9_KEY_DOWN, H9_KEY_DOWN, H9_KEY_BACK };
    USE_KEYS(scroll_view, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == RADIO_OK && !strcmp(selected_number, "004"));
    assert(full_fills == 1 && header_draws == 1 && footer_draws == 1 && row_fills == 20 && !notice_clears);
    const unsigned invalid_jump[] = { 11, 11, 11, H9_KEY_UP, H9_KEY_BACK };
    USE_KEYS(invalid_jump, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == RADIO_OK && !strcmp(selected_number, "098"));
    assert(full_fills == 1 && notice_clears == 1);
    USE_KEYS(scroll_down, 31);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == RADIO_OK && !strcmp(selected_number, "003"));
    assert(saw_long_name);
    const unsigned scroll_up[] = { H9_KEY_UP, H9_KEY_BACK };
    USE_KEYS(scroll_up, 31);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == RADIO_OK && !strcmp(selected_number, "198"));

    const unsigned retry[] = { H9_KEY_MENU, H9_KEY_MENU, 3, H9_KEY_MENU,
        6, 7, 7, 4, 7, 2, 2, H9_KEY_MENU, H9_KEY_BACK, H9_KEY_BACK };
    USE_KEYS(retry, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == -1 && check_value == 1);
    assert(!memcmp(image + 16, expected, 4));
    const unsigned tone[] = { H9_KEY_MENU, H9_KEY_DOWN, H9_KEY_DOWN, H9_KEY_MENU,
        9, H9_KEY_UP, H9_KEY_MENU, H9_KEY_BACK, H9_KEY_BACK };
    image[24] = image[25] = 0xff;
    USE_KEYS(tone, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == -1 && image[24] == 0x70 && image[25] == 0x06);
    const unsigned short_mhz[] = { H9_KEY_MENU, H9_KEY_MENU, 3, 6, 4, 4, 5,
        H9_KEY_MENU, H9_KEY_BACK, H9_KEY_BACK };
    USE_KEYS(short_mhz, 0);
    const uint8_t short_bcd[] = {0, 0x30, 0x22, 0x14};
    assert(radio_channel_editor(&storage, TEST_ACCENT) == -1 && saw_short_mhz);
    assert(!memcmp(image + 16, short_bcd, sizeof short_bcd));
    const unsigned precision[] = { H9_KEY_MENU, H9_KEY_DOWN, H9_KEY_MENU, 3, 6, 4, 4, 5, 6, 7, 8,
        H9_KEY_MENU, H9_KEY_BACK, H9_KEY_BACK };
    USE_KEYS(precision, 0);
    const uint8_t precise_bcd[] = {0x56, 0x34, 0x22, 0x14};
    assert(radio_channel_editor(&storage, TEST_ACCENT) == -1 && saw_precise_mhz);
    assert(!memcmp(image + 20, precise_bcd, sizeof precise_bcd));
    const unsigned overlong[] = { H9_KEY_MENU, H9_KEY_MENU, 3, 6, 4, 4, 5, 6, 7, 8, 9,
        H9_KEY_MENU, H9_KEY_BACK, H9_KEY_BACK, H9_KEY_BACK };
    USE_KEYS(overlong, 0);
    assert(radio_channel_editor(&storage, TEST_ACCENT) == RADIO_OK);
    assert(!memcmp(image + 16, short_bcd, sizeof short_bcd));
    puts("MHz entry and channel redraw bounds, names, menu color, hold-to-scroll, and keypad saves passed.");
}
