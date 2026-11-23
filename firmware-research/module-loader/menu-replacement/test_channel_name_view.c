#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t ram[0x20000];
static unsigned tick, revision, omit_a, windows, ascii, fallbacks;
static unsigned last_y, last_side, last_frequency, last_color, last_right;
static unsigned tx_side, frequency_draws, tx_windows, tx_stock_names;
static unsigned focus_enabled, syncs, redraws, focus_lines, short_reads;
static const char *short_name = "LOCAL FM";
static struct {
    unsigned y, scale;
    uint16_t color;
    char text[33];
} focus_text[3];
enum { NAME_TX_RED = 0xf800 };
static char last_text[16];
static struct name_row *fixture_rows(void);
static void stock_a(unsigned x, unsigned y, unsigned font);
static void stock_b(unsigned x, unsigned y, unsigned font);
static void stock_classic_draw(unsigned x, unsigned y, unsigned side, unsigned frequency, unsigned color);
static unsigned stock_display_side(void);
static void stock_frequency_draw(unsigned x, unsigned y, unsigned frequency, unsigned color, unsigned font);
static void stock_mark_redraw(unsigned side);
#define RADIO_BYTE(address) ram[address]
#define RADIO_TICK tick
#define NAME_ROWS fixture_rows()
#define STOCK_DRAW_A stock_a
#define STOCK_DRAW_B stock_b
#define STOCK_DRAW_CLASSIC stock_classic_draw
#define STOCK_DISPLAY_SIDE stock_display_side
#define STOCK_DRAW_FREQUENCY stock_frequency_draw
#define STOCK_MARK_REDRAW stock_mark_redraw
#include "channel_name_view.c"

static struct name_row *fixture_rows(void)
{
    static struct name_row rows[2];
    return rows;
}

static void stock_a(unsigned x, unsigned y, unsigned font)
{ (void)x; (void)y; (void)font; ++fallbacks; }

static void stock_b(unsigned x, unsigned y, unsigned font)
{ stock_a(x, y, font); }

static void stock_classic_draw(unsigned x, unsigned y, unsigned side, unsigned frequency, unsigned color)
{
    (void)x;
    last_y = y;
    last_side = side;
    last_frequency = frequency;
    last_color = color;
    ++fallbacks;
    if (color == NAME_TX_RED) ++tx_stock_names;
}

static unsigned stock_display_side(void) { return tx_side; }

static void stock_classic_tx(unsigned frequency)
{
    /* Match the pinned caller, which refreshes the opposite row first. */
    unsigned y = tx_side ? 76 : 22;
    radio_label_draw_classic(12, tx_side ? 22 : 76, tx_side ^ 1, 14652000, NAME_WHITE);
    radio_label_draw_tx_frequency(12, y, frequency, NAME_TX_RED, CLASSIC_FONT);
}

static void stock_frequency_draw(unsigned x, unsigned y, unsigned frequency, unsigned color, unsigned font)
{
    assert(x == 12 && y == 76 && color == NAME_TX_RED && font == CLASSIC_FONT);
    last_frequency = frequency;
    ++frequency_draws;
}

unsigned gfx_text(unsigned x, unsigned y, const char *text, unsigned pixels,
                  unsigned scale, uint16_t color, uint16_t bg)
{
    assert(scale & GFX_PROPORTIONAL);
    unsigned cell = (scale & 255) * 6;
    unsigned n = 0;
    while (n < pixels / cell && text[n]) ++n;
    if (scale & GFX_MEASURE) return n;
    if (bg) {
        assert(focus_enabled && x == 12 && pixels == 108 && focus_lines < 3);
        assert(y >= 22 && y + (scale & 255) * 8 <= 100);
        focus_text[focus_lines].y = y;
        focus_text[focus_lines].scale = scale & 255;
        focus_text[focus_lines].color = color;
        memcpy(focus_text[focus_lines].text, text, n);
        focus_text[focus_lines++].text[n] = 0;
        return x + n * cell;
    }
    assert(x + pixels <= 160 && y >= 4 && n < sizeof last_text);
    memcpy(last_text, text, n);
    last_text[n] = 0;
    last_y = y - 4;
    last_color = color;
    last_right = x + pixels;
    if (cell == 12) ++windows;
    else ++ascii;
    if (color == NAME_TX_RED) ++tx_windows;
    return x + n * cell;
}

void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    assert(x + w <= 160 && y + h <= 128);
    if (color) assert(focus_enabled && x == 12 && w == 108 && h == 24 && (y == 22 || y == 76));
}

unsigned radio_label_revision(void) { return revision; }
int radio_focus_enabled(void) { return focus_enabled; }
void radio_focus_sync(void) { ++syncs; }
static void stock_mark_redraw(unsigned side) { assert(!side); ++redraws; }

const struct radio_theme *radio_theme(unsigned theme)
{
    static const struct radio_theme focus = { .background = 1, .panel = 2, .text = 3,
                                               .muted = 4, .accent = 5 };
    assert(theme == RADIO_THEME_FOCUS);
    return &focus;
}

const char *radio_frequency_label(unsigned frequency, char text[10])
{
    assert(frequency < 100000000);
    snprintf(text, 10, "%03u.%05u", frequency / 100000, frequency % 100000);
    return text;
}

int radio_channel_short_name(unsigned memory, char label[33])
{
    assert(memory);
    ++short_reads;
    strcpy(label, short_name);
    return RADIO_OK;
}

int radio_label_get(unsigned channel, char label[33])
{
    const char *value = channel == 7 ? (omit_a ? "" : "0123456789ABCDEFGHIJKLMNOPQRSTUV") :
                        channel == 42 ? "MOUNTAIN WEATHER DISTRICT" : "";
    strcpy(label, value);
    return 0;
}

int main(void)
{
    ram[0x11AB0] = 1;
    ram[0x11AC6] = 3;
    ram[0x12BAD] = CLASSIC;
    ram[0x11AE9] = ram[0x11AEA] = ram[0x12BB2] = ram[0x12BB3] = 1;
    ram[0x11AEF] = 7;
    ram[0x11AF0] = 42;

    radio_label_draw_classic(12, 22, 0, 14652000, 0xffff);
    assert(windows == 1 && !fallbacks && !strcmp(last_text, "012345678") && last_right == 120);
    radio_label_draw_classic(70, 76, 1, 14439000, 0x07e0);
    assert(windows == 2 && !fallbacks && !strcmp(last_text, "MOUNTAI") && last_right == 160);
    tick = 99;
    radio_label_display_tick();
    assert(windows == 2);
    tick = 100;
    radio_label_display_tick();
    assert(windows == 4 && !strcmp(last_text, "OUNTAIN") && last_color == 0x07e0);

    omit_a = 1;
    ++revision;
    radio_label_display_tick();
    assert(fallbacks == 1 && last_side == 0 && last_frequency == 14652000);
    assert(windows == 5 && last_y == 76);
    ram[0x12BB3] = 0;
    radio_label_draw_classic(70, 76, 1, 14439000, 0x07e0);
    assert(fallbacks == 2 && last_side == 1 && last_frequency == 14439000 && last_color == 0x07e0);
    ram[0x11AFE] = 1;
    tick = 200;
    radio_label_display_tick();
    assert(windows == 5);

    ram[0x11AFE] = 0;
    ram[0x12BAD] = 1;
    omit_a = 0;
    radio_label_draw_a(4, 24, 8);
    assert(ascii == 1 && !strcmp(last_text, "0123456789AB"));

    ram[0x12BAD] = CLASSIC;
    ram[0x12BB3] = 1;
    stock_classic_tx(14612000);
    assert(last_y == 22 && last_color == NAME_TX_RED && tx_windows == 1 && !frequency_draws);
    tick += 100;
    radio_label_display_tick();
    assert(tx_windows == 2 && fixture_rows()[0].color == NAME_TX_RED);
    radio_label_draw_classic(12, 22, 0, 14652000, NAME_WHITE);
    assert(last_color == NAME_WHITE);

    tx_side = 1;
    ++revision;
    stock_classic_tx(14500000);
    assert(last_y == 76 && last_color == NAME_TX_RED && tx_windows == 3 &&
           !frequency_draws && !strncmp(last_text, "MOUNTAIN", 8));
    radio_label_draw_classic(12, 76, 1, 14439000, 0x07e0);
    assert(last_color == 0x07e0);

    tx_side = 0;
    omit_a = 1;
    ++revision;
    stock_classic_tx(14612000);
    assert(last_side == 0 && last_color == NAME_TX_RED && tx_stock_names == 1 && !frequency_draws);
    tx_side = 1;
    ram[0x12BB3] = 0;
    stock_classic_tx(14500000);
    assert(last_frequency == 14500000 && frequency_draws == 1 && tx_stock_names == 1);

    focus_enabled = 1;
    omit_a = 0;
    ram[0x12BB3] = 1;
    ram[0x11AC2] = 1; /* keypad lock leaves the home rows visible */
    radio_label_draw_classic(12, 22, 0, 14652001, NAME_WHITE);
    assert(focus_lines == 3 && focus_text[0].y == 22 && focus_text[1].y == 30 &&
           focus_text[2].y == 38 && focus_text[0].scale == 1 && focus_text[0].color == 5);
    assert(!strcmp(focus_text[0].text, "0123456789ABCDEFGH") &&
           !strcmp(focus_text[1].text, "IJKLMNOPQRSTUV") && !strcmp(focus_text[2].text, "146.52001"));
    focus_lines = 0;
    radio_label_draw_classic(12, 76, 1, 14439000, 0x07e0);
    assert(focus_lines == 3 && !strcmp(focus_text[0].text, "MOUNTAIN WEATHER") &&
           !strcmp(focus_text[1].text, "DISTRICT") && focus_text[2].color == 0x07e0);
    focus_lines = 0;
    radio_label_draw_tx_frequency(12, 76, 14500000, NAME_TX_RED, CLASSIC_FONT);
    assert(focus_lines == 3 && focus_text[0].color == NAME_TX_RED && focus_text[2].color == NAME_TX_RED);

    focus_lines = 0;
    ram[0xC140] = 1;
    unsigned before = syncs;
    radio_label_draw_classic(12, 22, 0, 14652000, NAME_WHITE);
    radio_label_display_tick();
    assert(!focus_lines && syncs == before);
    ram[0xC140] = 0;
    ram[0x11AFE] = 1;
    radio_label_display_tick();
    assert(syncs == before);
    ram[0x11AFE] = 0;
    radio_label_draw_classic(12, 22, 0, 14652000, NAME_WHITE);
    ++revision;
    radio_label_display_tick();
    assert(syncs == before + 1 && redraws == 1);

    focus_lines = 0;
    omit_a = 1;
    radio_label_draw_classic(12, 22, 0, 14652000, NAME_WHITE);
    assert(focus_lines == 2 && !strcmp(focus_text[0].text, "LOCAL FM") && short_reads == 1);
    focus_lines = 0;
    short_name = "NEW NAME";
    radio_label_draw_classic(12, 22, 0, 14652000, NAME_WHITE);
    assert(focus_lines == 2 && !strcmp(focus_text[0].text, "NEW NAME") && short_reads == 2);
    focus_lines = 0;
    short_name = "";
    radio_label_draw_classic(12, 22, 0, 14652000, NAME_WHITE);
    assert(focus_lines == 1 && focus_text[0].scale == 2 && !strcmp(focus_text[0].text, "146.52000"));
    focus_lines = 0;
    ram[0x12BB2] = 0;
    radio_label_draw_classic(12, 22, 0, 44600000, NAME_WHITE);
    assert(focus_lines == 1 && focus_text[0].scale == 2 && !strcmp(focus_text[0].text, "446.00000"));
    puts("Stock layouts and Focus row bounds, wrapping, RX/TX, key guards and name/frequency fallback passed.");
}
