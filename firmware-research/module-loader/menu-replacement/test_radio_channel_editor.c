#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "radio_channels.h"
#include "channel_labels_native.h"
#include "h9mod.h"

static const unsigned *keys;
static uint8_t image[0x6000];
static unsigned writes, separate_editors;
static int read_image(unsigned offset, void *dst, unsigned n) { memcpy(dst, image + offset, n); return 0; }
static int write_image(unsigned offset, const void *src, unsigned n) { memcpy(image + offset, src, n); ++writes; return 0; }
static const struct radio_storage storage = { read_image, write_image };
static unsigned long_edits;
static char stored_label[32];
static int read_label(unsigned channel, void *dst, unsigned n)
{ assert(channel && channel <= 199 && n == 32); memcpy(dst, stored_label, n); return 0; }
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
unsigned gfx_wait_key(void) { unsigned key = *keys++; assert(key); return key; }
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{ (void)color; assert(x + w <= H9_WIDTH && y + h <= H9_HEIGHT); }
unsigned gfx_text(unsigned x, unsigned y, const char *s, unsigned max, unsigned scale, uint16_t fg, uint16_t bg)
{
    (void)fg; (void)bg;
    unsigned n = 0; while (n < max && s[n]) ++n;
    assert(x + n * 6 * scale <= H9_WIDTH && y + 8 * scale <= H9_HEIGHT);
    return x + n * 6 * scale;
}
int main(void)
{
    const unsigned frequency[] = { H9_KEY_DOWN, H9_KEY_MENU, 3, 6, 7, 7, 4, 7, 2, 2, H9_KEY_MENU, H9_KEY_BACK };
    keys = frequency;
    assert(radio_channel_editor(&storage) == -1 && writes == 1 && !separate_editors);
    const uint8_t expected[] = {0, 0x25, 0x55, 0x14};
    assert(!memcmp(image + 16, expected, 4));
    const unsigned cancel[] = { H9_KEY_DOWN, H9_KEY_MENU, H9_KEY_UP, H9_KEY_BACK, H9_KEY_BACK };
    keys = cancel;
    assert(radio_channel_editor(&storage) == RADIO_OK && writes == 1 && !separate_editors);
    assert(!memcmp(image + 16, expected, 4));
    const unsigned target[] = { H9_KEY_MENU, 3,11,11,H9_KEY_MENU, H9_KEY_DOWN,H9_KEY_MENU,
        3,6,7,7,4,7,2,2,H9_KEY_MENU,H9_KEY_BACK };
    keys = target;
    assert(radio_channel_editor(&storage) == -1 && writes == 2 && !separate_editors);
    assert(!memcmp(image + 199 * 16, expected, 4));
    const unsigned cancel_target[] = { H9_KEY_MENU, 3,11,11,H9_KEY_BACK, H9_KEY_DOWN,H9_KEY_MENU,
        H9_KEY_UP,H9_KEY_MENU,H9_KEY_BACK };
    keys = cancel_target;
    assert(radio_channel_editor(&storage) == -1 && writes == 3 && !separate_editors);
    assert(memcmp(image + 16, expected, 4) && !memcmp(image + 199 * 16, expected, 4));
    const unsigned names[] = {
        H9_KEY_UP, H9_KEY_UP, H9_KEY_UP, H9_KEY_UP, H9_KEY_UP, H9_KEY_UP,
        H9_KEY_MENU, H9_KEY_UP, H9_KEY_MENU, H9_KEY_BACK
    };
    keys = names; /* wrap from 0 to 19, then reach Long name and Short name */
    assert(radio_channel_editor(&storage) == RADIO_OK && long_edits == 1 && separate_editors == 1);
    assert(stored_label[31] && !image[0xd40]);
    const unsigned delete_channel[] = { H9_KEY_UP, H9_KEY_MENU, H9_KEY_MENU, H9_KEY_BACK };
    keys = delete_channel;
    assert(radio_channel_editor(&storage) == -1 && !stored_label[0]);
    puts("Channel frequencies, native short names, 32-character names and delete passed.");
}
