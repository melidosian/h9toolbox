#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "radio_settings.h"
#include "h9mod.h"

static const unsigned *keys;
static unsigned saves, saw_error;
static uint8_t stored[64];
static int read_image(unsigned offset, void *dst, unsigned n) { (void)offset; memcpy(dst, stored, n); return 0; }
static int write_image(unsigned offset, const void *src, unsigned n) { (void)offset; memcpy(stored, src, n); ++saves; return 0; }
static const struct radio_storage storage = { read_image, write_image };
unsigned gfx_wait_key(void) { unsigned key = *keys++; assert(key); return key; }
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{ (void)color; assert(x + w <= H9_WIDTH && y + h <= H9_HEIGHT); }
unsigned gfx_text(unsigned x, unsigned y, const char *s, unsigned max, unsigned scale, uint16_t fg, uint16_t bg)
{
    (void)fg; (void)bg;
    unsigned n = 0; while (n < max && s[n]) ++n;
    assert(x + n * 6 * scale <= H9_WIDTH && y + 8 * scale <= H9_HEIGHT);
    saw_error |= !strcmp(s, "CHECK VALUE");
    return x + n * 6 * scale;
}
int main(void)
{
    static const struct radio_setting number = { .id = "interval", .label = "Beacon interval (seconds)",
        .length = 2, .kind = RADIO_NUMBER, .mask = 255, .min = 1, .max = 65534 };
    static const struct radio_setting call = { .id = "call", .label = "Callsign", .length = 6, .kind = RADIO_TEXT };
    const unsigned cancel[] = { H9_KEY_MENU, H9_KEY_UP, H9_KEY_BACK, H9_KEY_BACK };
    keys = cancel;
    assert(!radio_editor(&storage, &number) && !saves);
    const unsigned numeric[] = { H9_KEY_MENU, 8, 2, 2, H9_KEY_MENU, H9_KEY_MENU };
    keys = numeric;
    assert(radio_editor(&storage, &number) == -1 && saves == 1);
    assert((stored[0] | stored[1] << 8) == 600);
    memcpy(stored, "K9ABC", 6);
    const unsigned text[] = { H9_KEY_MENU, H9_KEY_UP, H9_KEY_BACK, H9_KEY_MENU };
    keys = text;
    assert(radio_editor(&storage, &call) == -1 && saves == 2 && !memcmp(stored, "L9ABC", 6));
    memset(stored, 0, 6);
    const unsigned invalid[] = { H9_KEY_DOWN, H9_KEY_MENU, H9_KEY_BACK };
    keys = invalid;
    assert(!radio_editor(&storage, &call) && saw_error && saves == 2);
    static const struct radio_setting dtmf = { .id = "dtmf", .label = "DTMF code 1", .length = 16,
        .kind = RADIO_DTMF_TERMINATED };
    memset(stored, 255, sizeof stored);
    const unsigned tones[] = { H9_KEY_MENU, 3,4, H9_KEY_DOWN, H9_KEY_BACK, H9_KEY_MENU };
    keys = tones;
    assert(radio_editor(&storage, &dtmf) == -1 && saves == 3);
    assert(stored[0] == 1 && stored[1] == 2 && stored[2] == 15 && stored[3] == 255);
    puts("Radio editor numeric entry, text edits, validation and cancel flows passed.");
}
