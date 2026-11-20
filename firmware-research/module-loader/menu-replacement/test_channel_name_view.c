#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t ram[0x20000];
static unsigned tick, revision, omit_a, windows, ascii, fallbacks;
static unsigned last_y, last_side, last_frequency, last_color, last_right;
static char last_text[10];
static struct name_row *fixture_rows(void);
static void stock_a(unsigned x, unsigned y, unsigned font);
static void stock_b(unsigned x, unsigned y, unsigned font);
static void stock_classic_draw(unsigned x, unsigned y, unsigned side, unsigned frequency, unsigned color);
static void ascii_draw(unsigned x, unsigned y, const char *text, unsigned color, unsigned size, unsigned bg);
static void window_draw(unsigned x, unsigned y, const char *text, unsigned color,
                        unsigned size, unsigned left, unsigned right);

#define RADIO_BYTE(address) ram[address]
#define RADIO_TICK tick
#define NAME_ROWS fixture_rows()
#define STOCK_DRAW_A stock_a
#define STOCK_DRAW_B stock_b
#define STOCK_DRAW_CLASSIC stock_classic_draw
#define STOCK_DRAW_ASCII ascii_draw
#define STOCK_DRAW_WINDOW window_draw
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
}

static void ascii_draw(unsigned x, unsigned y, const char *text, unsigned color, unsigned size, unsigned bg)
{
    assert(x + 64 <= 160 && y + 16 <= 128 && size == 8 && !bg && color == 0xffff);
    strcpy(last_text, text);
    ++ascii;
}

static void window_draw(unsigned x, unsigned y, const char *text, unsigned color,
                        unsigned size, unsigned left, unsigned right)
{
    assert(y + 24 <= 128 && size == 24 && x == left && right <= 160);
    assert(strlen(text) * 12 <= right - left);
    last_y = y;
    last_color = color;
    last_right = right;
    strcpy(last_text, text);
    ++windows;
}

void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{ assert(x + w <= 160 && y + h <= 128 && !color); }

unsigned radio_label_revision(void) { return revision; }

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
    assert(ascii == 1 && !strcmp(last_text, "01234567"));
    puts("Classic long names, clipping, A/B timing and stock fallbacks passed.");
}
