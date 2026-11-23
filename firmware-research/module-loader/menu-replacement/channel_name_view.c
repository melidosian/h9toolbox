#include "channel_labels_native.h"
#include "channel_name_scroll.h"
#include "app_theme.h"

#ifndef RADIO_BYTE
#define RADIO_BYTE(address) (*(const volatile uint8_t *)(uintptr_t)(address))
#define RADIO_TICK (*(const volatile uint32_t *)0x1d8b0u)
#endif
#define DISPLAY_LAYOUT RADIO_BYTE(0x12BADu)
#define ACTIVE_SIDE (RADIO_BYTE(0x11B50u) >> 7)

enum { CLASSIC = 2, CLASSIC_FONT = 24, NAME_WHITE = 0xffff };
typedef void (*stock_name)(unsigned x, unsigned y, unsigned font);
typedef void (*stock_classic)(unsigned x, unsigned y, unsigned side, unsigned frequency, unsigned color);
typedef void (*stock_frequency)(unsigned x, unsigned y, unsigned frequency, unsigned color, unsigned font);

#ifndef STOCK_DRAW_A
#define STOCK_DRAW_A ((stock_name)0x01e5a486u)
#define STOCK_DRAW_B ((stock_name)0x01e5a54eu)
#define STOCK_DRAW_CLASSIC ((stock_classic)0x01e59a08u)
#define STOCK_DISPLAY_SIDE ((unsigned (*)(void))0x01e50f58u)
#define STOCK_DRAW_FREQUENCY ((stock_frequency)0x01e5977cu)
#endif

struct name_row {
    uint32_t start, revision, frequency;
    uint16_t color;
    uint8_t memory, x, y, font, layout, position, last;
    char label[33];
};

#ifndef NAME_ROWS
#define NAME_ROWS ((struct name_row *)0x29980u)
#endif
_Static_assert(2 * sizeof(struct name_row) <= 0x70, "channel name display RAM before 33 cm state");

static unsigned memory_channel(unsigned side)
{
    return radio_memory_channel(RADIO_BYTE, side);
}

static unsigned classic_right(unsigned x)
{
    return x <= H9_WIDTH - 108 ? x + 108 : H9_WIDTH;
}

static unsigned name_width(unsigned x, unsigned classic)
{
    unsigned right = classic ? classic_right(x) : x + (x <= H9_WIDTH - 72 ? 72 : 64);
    return right - x;
}

static void paint(const struct name_row *row)
{
    unsigned classic = row->layout == CLASSIC;
    unsigned width = name_width(row->x, classic);
    unsigned scale = classic ? 2 : 1, height = 8 * scale;
    unsigned x = gfx_label(row->x, row->y + 4, row->label + row->position,
                           width, scale, row->color, UI_BG);
    gfx_fill(row->x, row->y, width, 4, UI_BG);
    gfx_fill(x, row->y + 4, row->x + width - x, height, UI_BG);
    gfx_fill(row->x, row->y + 4 + height, width, 4, UI_BG);
}

static int draw_name(unsigned side, unsigned x, unsigned y, unsigned font,
                     unsigned frequency, unsigned color)
{
    struct name_row *row = &NAME_ROWS[side];
    unsigned memory = memory_channel(side), layout = DISPLAY_LAYOUT, revision = radio_label_revision();
    unsigned classic = font == CLASSIC_FONT;
    if (!memory || memory > 199 || x >= H9_WIDTH ||
        (classic && classic_right(x) - x < 12) ||
        (classic ? layout != CLASSIC || y > H9_HEIGHT - CLASSIC_FONT :
         layout >= CLASSIC || (font != 8 && font != 16) ||
         x > H9_WIDTH - CHANNEL_NAME_CELLS * 8 || y > H9_HEIGHT - 16)) {
        row->memory = 0;
        return 0;
    }
    unsigned now = RADIO_TICK;
    if (memory != row->memory || x != row->x || y != row->y || font != row->font ||
        layout != row->layout || revision != row->revision) {
        if (radio_label_get(memory, row->label)) {
            row->memory = 0;
            return 0;
        }
        row->memory = memory;
        row->x = x;
        row->y = y;
        row->font = font;
        row->layout = layout;
        row->revision = revision;
        row->start = now;
        unsigned budget = name_width(x, classic) >> classic;
        unsigned last = 0;
        while (row->label[last + gfx_label_fit(row->label + last, budget)]) ++last;
        row->last = last;
    }
    row->color = color;
    row->frequency = frequency;
    if (!row->label[0]) return 0;

    row->position = channel_name_scroll_start(now - row->start, row->last);
    paint(row);
    return 1;
}

void radio_label_draw_a(unsigned x, unsigned y, unsigned font)
{
    if (!draw_name(0, x, y, font, 0, NAME_WHITE)) STOCK_DRAW_A(x, y, font);
}

void radio_label_draw_b(unsigned x, unsigned y, unsigned font)
{
    if (!draw_name(1, x, y, font, 0, NAME_WHITE)) STOCK_DRAW_B(x, y, font);
}

void radio_label_draw_classic(unsigned x, unsigned y, unsigned side, unsigned frequency, unsigned color)
{
    if (side > 1 || !draw_name(side, x, y, CLASSIC_FONT, frequency, color))
        STOCK_DRAW_CLASSIC(x, y, side, frequency, color);
}

void radio_label_draw_tx_frequency(unsigned x, unsigned y, unsigned frequency, unsigned color, unsigned font)
{
    /* The Classic TX routine draws the other row normally, then sends the
     * red frequency draw straight to the active row. Keep Name + number. */
    unsigned side = STOCK_DISPLAY_SIDE();
    if (side < 2 && DISPLAY_LAYOUT == CLASSIC && font == CLASSIC_FONT && memory_channel(side))
        radio_label_draw_classic(x, y, side, frequency, color);
    else
        STOCK_DRAW_FREQUENCY(x, y, frequency, color, font);
}

void radio_label_display_tick(void)
{
    unsigned layout = DISPLAY_LAYOUT;
    if (RADIO_BYTE(0x11AB0u) != 1 || RADIO_BYTE(0x11AC6u) != 3 || RADIO_BYTE(0x11AFDu) ||
        RADIO_BYTE(0x11AFEu) || layout > CLASSIC) {
        NAME_ROWS[0].memory = NAME_ROWS[1].memory = 0;
        return;
    }
    if (RADIO_BYTE(0x11AFBu) || RADIO_BYTE(0x11B23u) || RADIO_BYTE(0xC3AFu)) return;

    unsigned now = RADIO_TICK, revision = radio_label_revision();
    for (unsigned side = 0; side < 2; ++side) {
        struct name_row *row = &NAME_ROWS[side];
        if (!row->memory) continue;
        if (row->memory != memory_channel(side) || row->layout != layout ||
            (row->font == 16 && side != ACTIVE_SIDE) ||
            (layout != CLASSIC && RADIO_BYTE(0xC142u) == side)) {
            row->memory = 0;
            continue;
        }
        if (row->revision != revision) {
            if (layout == CLASSIC)
                radio_label_draw_classic(row->x, row->y, side, row->frequency, row->color);
            else if (side) radio_label_draw_b(row->x, row->y, row->font);
            else radio_label_draw_a(row->x, row->y, row->font);
        } else if (row->label[0]) {
            unsigned position = channel_name_scroll_start(now - row->start, row->last);
            if (position != row->position) {
                row->position = position;
                paint(row);
            }
        }
    }
}
