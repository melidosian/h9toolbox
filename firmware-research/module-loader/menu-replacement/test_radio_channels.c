#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "radio_channels.h"

static uint8_t image[0x3140];
static int read_image(unsigned offset, void *dst, unsigned n)
{ assert(offset + n <= sizeof image); memcpy(dst, image + offset, n); return 0; }
static int write_image(unsigned offset, const void *src, unsigned n)
{ assert(offset + n <= sizeof image); memcpy(image + offset, src, n); return 0; }
static const struct radio_storage storage = { read_image, write_image };
int main(void)
{
    char label[10];
    assert(!strcmp(radio_frequency_label(14223000, label), "142.230"));
    assert(!strcmp(radio_frequency_label(14223450, label), "142.2345"));
    assert(!strcmp(radio_frequency_label(14223456, label), "142.23456"));
    assert(!strcmp(radio_frequency_label(1800000, label), "018.000"));
    for (unsigned target = 1; target <= RADIO_CHANNEL_TARGETS; ++target) {
        unsigned count = target <= 199 ? RADIO_CHANNEL_FIELDS : 12;
        for (unsigned field = 0; field < count; ++field) {
            struct radio_setting setting;
            struct radio_edit edit, readback;
            memset(image, 0, sizeof image);
            if (field == 14) {
                const uint8_t rx[] = {0, 0x25, 0x55, 0x14};
                memcpy(image + target * 16, rx, 4);
            }
            assert(radio_channel_field(target, field, &setting) == RADIO_OK);
            if (target > 199 && field == 1) {
                assert(setting.offset == (target == 200 ? 0xcb0 : 0xcb4));
                assert(setting.min == 0);
            }
            assert(radio_begin(&storage, &setting, &edit) == RADIO_OK);
            if (setting.kind == RADIO_TEXT) assert(radio_set_text(&edit, "TEST", 4) == RADIO_OK);
            else if (setting.kind == RADIO_FREQUENCY) assert(radio_set_number(&edit, 14552500) == RADIO_OK);
            else assert(radio_adjust(&edit, 1) == RADIO_OK);
            assert(radio_save(&storage, &edit) == RADIO_OK);
            assert(radio_begin(&storage, &setting, &readback) == RADIO_OK);
            assert(!memcmp(edit.value, readback.value, setting.length));
            if (setting.kind == RADIO_FREQUENCY) {
                const uint8_t expected[] = {0, 0x25, 0x55, 0x14};
                assert(!memcmp(image + setting.offset, expected, 4));
                assert(radio_value(&readback) == 14552500);
            }
            assert(image[setting.offset - 1] == 0 && image[setting.offset + setting.length] == 0);
        }
    }
    struct radio_setting setting;
    assert(radio_channel_field(0, 0, &setting) == RADIO_INVALID);
    assert(radio_channel_field(202, 0, &setting) == RADIO_INVALID);
    assert(radio_channel_field(200, 12, &setting) == RADIO_INVALID);
    memset(image, 0, sizeof image);
    const uint8_t vfo[] = {0, 0x25, 0x55, 0x14, 0, 0x25, 0x55, 0x14, 0xff, 0xff, 0xff, 0xff, 0, 0, 0x20, 0};
    memcpy(image + 0x1950, vfo, sizeof vfo);
    assert(radio_channel_copy(&storage, 9, 0) == RADIO_OK);
    assert(!memcmp(image + 9 * 16, vfo, sizeof vfo));
    assert(image[0x1901] == 1 && image[0x1921] == 1);
    image[0x1921] = 0xff;
    assert(radio_channel_delete(&storage, 9) == RADIO_OK);
    assert(image[0x1901] == 0 && image[0x1921] == 0xfe);
    assert(!memcmp(image + 9 * 16, vfo, sizeof vfo));
    image[0x1950+14] |= 1; /* native value 1 is minus */
    const uint8_t shift[] = {0,0,6,0};
    memcpy(image + 0xcb0, shift, 4);
    assert(radio_channel_copy(&storage, 11, 0) == RADIO_OK);
    const uint8_t minus[] = {0,0x25,0x49,0x14};
    assert(!memcmp(image + 11*16+4, minus, 4));
    image[0x1950+14] = (image[0x1950+14] & ~3) | 2;
    assert(radio_channel_copy(&storage, 12, 0) == RADIO_OK);
    const uint8_t plus[] = {0,0x25,0x61,0x14};
    assert(!memcmp(image + 12*16+4, plus, 4));
    memset(image + 0x1950, 0xff, 4);
    assert(radio_channel_copy(&storage, 10, 0) == RADIO_INVALID && !(image[0x1901] & 2));
    puts("199 channel and two VFO field round trips and frequency wire encoding passed.");
}
