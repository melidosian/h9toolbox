#include "app_theme.h"

enum option_state { UNKNOWN, EXCLUDED, INCLUDED };

static void line(unsigned row, const char *label, const char *value)
{
    unsigned y = 25 + row * 14, n = 0;
    while (value[n]) ++n;
    gfx_text(UI_LEFT, y, label, 14, 1, UI_DIM, UI_BG);
    gfx_text(H9_WIDTH - UI_LEFT - n * 6, y, value, n, 1, UI_TEXT, UI_BG);
}

static void option(unsigned row, const char *label, enum option_state state)
{
    line(row, label, state == INCLUDED ? "INCLUDED" : state == EXCLUDED ? "EXCLUDED" : "UNKNOWN");
}

static char *decimal(char *out, unsigned value)
{
    if (value >= 100) *out++ = (char)('0' + value / 100);
    if (value >= 10) *out++ = (char)('0' + value / 10 % 10);
    *out++ = (char)('0' + value % 10);
    return out;
}

static void version_text(char *out, uint32_t firmware)
{
    *out++ = 'V';
    out = decimal(out, (firmware >> 16) & 255u);
    *out++ = '.';
    out = decimal(out, (firmware >> 8) & 255u);
    *out++ = '.';
    out = decimal(out, firmware & 255u);
    if (firmware >> 24) {
        *out++ = '-';
        *out++ = 'R';
        *out++ = 'C';
        out = decimal(out, firmware >> 24);
    }
    *out = 0;
}

static uint32_t instruction(uint32_t address)
{
    const volatile uint16_t *words = (const volatile uint16_t *)(uintptr_t)address;
    return words[0] | ((uint32_t)words[1] << 16);
}

static enum option_state modem_updater(uint32_t firmware, uint32_t screen, uint32_t launch)
{
    /* Main-flash APRS updater screen/launch call sites from
     * on-radio-aprs/build.py. The shared 0.1.7 and 0.1.8 version markers
     * cannot distinguish their with-modem and no-modem packages. */
    if (screen == 0x8d68eabfu && launch == 0x8e39eabfu) return EXCLUDED;
    if (firmware != 0x000105u)
        return screen == 0xfe56eabeu && launch == 0xfe9beabeu ? INCLUDED : UNKNOWN;
    if (screen != 0xfc79ea80u || launch != 0xfcc1ea80u) return UNKNOWN;

    /* v0.1.5 installs its updater payload separately into a 24 KiB
     * plain flash region. Match the production and Dev staging images. */
    const volatile uint8_t *stage = (const volatile uint8_t *)(uintptr_t)0x01eef000u;
    uint16_t crc = 0;
    for (unsigned i = 0; i < 0x6000u; i++) {
        crc ^= (uint16_t)stage[i] << 8;
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0));
    }
    if (crc == 0x89f3u || crc == 0x199fu) return INCLUDED;
    return crc == 0xc7bcu ? EXCLUDED : UNKNOWN;
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    char value[20];
    uint32_t firmware = api->version >= 2 ? api->firmware : 0;
    uint32_t version = firmware & 0x00ffffffu;
    enum option_state quick = UNKNOWN, band = UNKNOWN, modem = UNKNOWN;
    if (version >= 0x000105u && version <= 0x000108u) {
        quick = version >= 0x000107u ? INCLUDED : EXCLUDED;
        band = version == 0x000108u ? INCLUDED : EXCLUDED;
        modem = modem_updater(version, instruction(0x01e8857eu), instruction(0x01e88588u));
    }
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    ui_title("ABOUT");
    if (firmware) version_text(value, firmware);
    line(0, "FIRMWARE", firmware ? value : "UNKNOWN");
    option(1, "QUICK CHANGE", quick);
    option(2, "CHANNEL EDIT", quick);
    option(3, "FASTER SCROLL", quick);
    option(4, "33 CM TOOLS", band);
    option(5, "MODEM UPDATER", modem);
    ui_footer("MENU OR BACK  TOOLBOX");

    gfx_wait_release();
    for (;;) {
        unsigned key = gfx_wait_key();
        if (key == H9_KEY_MENU || key == H9_KEY_BACK) break;
    }
    gfx_wait_release();
    return 0;
}
