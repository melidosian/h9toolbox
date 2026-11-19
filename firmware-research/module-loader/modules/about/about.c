/* ABOUT: a small module that shows the loader's details and the live 100 Hz
 * tick, then returns to the menu when Menu or Back is pressed. It exercises
 * the module-returns-to-menu path (Brick Breaker never returns). */
#include "app_theme.h"

static void hex(char *out, uint32_t value, unsigned digits)
{
    for (unsigned i = 0; i < digits; i++) {
        unsigned nibble = (value >> (4 * (digits - 1 - i))) & 15;
        out[i] = (char)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
    out[digits] = 0;
}

static void line(unsigned row, const char *label, const char *value)
{
    unsigned y = 24 + row * 13, n = 0;
    while (value[n]) ++n;
    gfx_text(UI_LEFT, y, label, 14, 1, UI_DIM, UI_BG);
    gfx_text(H9_WIDTH - UI_LEFT - n * 6, y, value, n, 1, UI_TEXT, UI_BG);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    char value[16];
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("ABOUT");
    hex(value, api->version, 2);     line(0, "API VERSION", value);
    value[0] = '0'; value[1] = 'X';
    hex(value + 2, api->load, 5);    line(1, "LOAD ADDRESS", value);
    hex(value + 2, api->slot, 4);    line(2, "RAM SLOT", value);
    hex(value + 2, api->bundle, 6);  line(3, "BUNDLE", value);
    hex(value, self, 2);             line(4, "MODULE", value);
    if (api->version >= 2) {          /* 0x000104 reads "V0.1.4" */
        char *d = value;
        *d++ = 'V';
        for (unsigned shift = 16;; shift -= 8) {
            unsigned part = api->firmware >> shift & 255;
            if (part >= 100) *d++ = (char)('0' + part / 100);
            if (part >= 10) *d++ = (char)('0' + part / 10 % 10);
            *d++ = (char)('0' + part % 10);
            if (!shift) break;
            *d++ = '.';
        }
        *d = 0;
    } else {                          /* an older loader has no version field */
        value[0] = 'O'; value[1] = 'L'; value[2] = 'D'; value[3] = 0;
    }
    line(5, "FIRMWARE", value);
    ui_footer("MENU OR BACK  TOOLBOX");

    gfx_wait_release();
    unsigned last = H9_KEY_NONE;
    for (;;) {
        value[0] = '0'; value[1] = 'X';
        hex(value + 2, *(volatile uint32_t *)0x1d8b0, 8);
        line(6, "TICK", value);
        h9_delay(5);
        unsigned key = h9_key();
        if (key != last && (key == H9_KEY_MENU || key == H9_KEY_BACK)) break;
        last = key;
    }
    gfx_wait_release();
    return 0;
}
