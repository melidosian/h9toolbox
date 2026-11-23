#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "radio_settings_catalog.h"
#include "radio_33cm.h"
#include "radio_theme.h"

static unsigned char image[0x6100];
static unsigned writes, fail_write, corrupt_write;
static int read_image(unsigned offset, void *dst, unsigned n)
{
    assert(offset + n <= sizeof image);
    memcpy(dst, image + offset, n);
    return 0;
}
static int write_image(unsigned offset, const void *src, unsigned n)
{
    ++writes;
    if (fail_write) return -1;
    memcpy(image + offset, src, n);
    if (corrupt_write) image[offset] ^= 1;
    return 0;
}
static const struct radio_storage storage = { read_image, write_image };

int main(void)
{
    assert(RADIO_SETTING_COUNT == 124);
    const struct radio_setting *theme = &radio_settings[RADIO_INTERFACE_THEME];
    assert(theme->offset == RADIO_THEME_SETTING && theme->choice_count == RADIO_THEME_COUNT);
    unsigned tx500 = 0;
    while (tx500 < RADIO_SETTING_COUNT && strcmp(radio_settings[tx500].id, "tx500")) ++tx500;
    assert(tx500 + 1 < RADIO_SETTING_COUNT);
    const struct radio_setting *band = &radio_settings[tx500 + 1];
    assert(!strcmp(band->label, "900TX") && band->group == radio_settings[tx500].group);
    assert(band->offset == RADIO_33CM_SETTING && band->choice_count == 2);
    for (unsigned i = 0; i < RADIO_SETTING_COUNT; ++i) {
        const struct radio_setting *s = &radio_settings[i];
        struct radio_edit edit, reloaded;
        char value[RADIO_MAX_VALUE + 1];
        memset(image, 0xa5, sizeof image);
        writes = 0;
        assert(radio_begin(&storage, s, &edit) == RADIO_OK);
        if (s->kind == RADIO_TEXT) assert(radio_set_text(&edit, "K9ABC", 5) == RADIO_OK);
        else if (radio_is_text(s)) assert(radio_set_text(&edit, "12AB#", s->length == 3 ? 3 : 5) == RADIO_OK);
        else assert(radio_adjust(&edit, 1) == RADIO_OK);
        assert(radio_save(&storage, &edit) == RADIO_OK);
        assert(writes == 1);
        assert(radio_begin(&storage, s, &reloaded) == RADIO_OK);
        assert(!memcmp(reloaded.value, edit.value, s->length));
        assert(radio_value_label(&reloaded, value));
        assert(radio_save(&storage, &reloaded) == RADIO_OK && writes == 1);
        if (s->length == 1 && s->kind != RADIO_TEXT)
            assert((image[s->offset] & ~s->mask) == (0xa5 & ~s->mask));
        assert(image[s->offset - 1] == 0xa5 && image[s->offset + s->length] == 0xa5);
    }
    const struct radio_setting *s = &radio_settings[1]; /* voice prompt, one bit */
    struct radio_edit edit;
    memset(image, 0, sizeof image);
    assert(radio_begin(&storage, s, &edit) == RADIO_OK);
    assert(radio_adjust(&edit, 1) == RADIO_OK);
    image[s->offset] |= 0x80; /* another client changes an unrelated bit */
    assert(radio_save(&storage, &edit) == RADIO_OK && image[s->offset] == 0x81);
    assert(radio_begin(&storage, s, &edit) == RADIO_OK);
    assert(radio_adjust(&edit, 1) == RADIO_OK);
    image[s->offset] ^= 1;
    assert(radio_save(&storage, &edit) == RADIO_CONFLICT);
    assert(radio_begin(&storage, s, &edit) == RADIO_OK);
    assert(radio_adjust(&edit, 1) == RADIO_OK);
    fail_write = 1;
    assert(radio_save(&storage, &edit) == RADIO_IO_ERROR);
    fail_write = 0; corrupt_write = 1;
    assert(radio_save(&storage, &edit) == RADIO_VERIFY_ERROR);
    corrupt_write = 0;
    memset(image, 0xff, sizeof image);
    for (unsigned i = 0; i < RADIO_SETTING_COUNT; ++i) {
        s = &radio_settings[i];
        if (s->kind >= RADIO_DTMF_FIXED) {
            assert(radio_begin(&storage, s, &edit) == RADIO_OK);
            assert(radio_set_text(&edit, "1B#", 3) == RADIO_OK);
            assert(radio_save(&storage, &edit) == RADIO_OK);
            const uint8_t tones[] = {1,11,15};
            unsigned prefix = s->kind == RADIO_DTMF_LENGTH;
            assert(!memcmp(image + s->offset + prefix, tones, 3));
            if (prefix) assert(image[s->offset] == 3);
            if (s->length > 4) assert(image[s->offset + prefix + 3] == 255);
            assert(radio_set_text(&edit, "Z", 1) == RADIO_INVALID);
            assert(radio_set_text(&edit, "", 0) == RADIO_OK);
            assert(radio_save(&storage, &edit) == RADIO_OK);
            assert(image[s->offset] == (prefix ? 0 : 255));
        } else if (s->kind == RADIO_DEGREES) {
            assert(radio_begin(&storage, s, &edit) == RADIO_OK);
            assert(radio_set_number(&edit, 87654321) == RADIO_OK);
            assert(radio_save(&storage, &edit) == RADIO_OK);
            const uint8_t degrees[] = {0x05,0x39,0x7f,0xb1};
            assert(!memcmp(image + s->offset, degrees, 4));
            char label[RADIO_MAX_VALUE + 1];
            assert(!strcmp(radio_value_label(&edit, label), "87.654321"));
            assert(radio_set_number(&edit, s->max + 1) == RADIO_INVALID);
        } else if (s->kind == RADIO_ALTITUDE) {
            assert(radio_begin(&storage, s, &edit) == RADIO_OK);
            assert(radio_set_number(&edit, (unsigned)-200) == RADIO_OK);
            assert(radio_save(&storage, &edit) == RADIO_OK);
            const uint8_t altitude[] = {0xf4,0x14,0x3e,0x00};
            assert(!memcmp(image + s->offset, altitude, 4));
            char label[RADIO_MAX_VALUE + 1];
            assert(!strcmp(radio_value_label(&edit, label), "-200"));
            assert(radio_adjust(&edit, -1) == RADIO_OK && (int32_t)radio_value(&edit) == -201);
            assert(radio_set_number(&edit, (unsigned)-1001) == RADIO_INVALID);
            assert(radio_set_number(&edit, 2148) == RADIO_INVALID);
        } else if (s->kind == RADIO_SIGNED16) {
            assert(radio_begin(&storage, s, &edit) == RADIO_OK);
            assert(radio_set_number(&edit, (unsigned)-125) == RADIO_OK);
            assert(radio_save(&storage, &edit) == RADIO_OK);
            assert(image[s->offset] == 0x83 && image[s->offset + 1] == 0xff);
            assert(radio_adjust(&edit, 1) == RADIO_OK && (int32_t)radio_value(&edit) == -115);
            assert(radio_set_number(&edit, 32768) == RADIO_INVALID);
            assert(radio_set_number(&edit, (unsigned)-32769) == RADIO_INVALID);
        }
    }
    puts("124 radio fields: round trips, calibration, DTMF, coordinates, shared bits and I/O failures passed.");
}
