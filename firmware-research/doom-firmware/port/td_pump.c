// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: the LCD pump (contract: DMC100-Doom platform/lcd.h).
//
// The engine draws a 320 x 240 column-major screen: 8-bit palette columns,
// each top to bottom, columns left to right (the 3D view as 160 columns sent
// twice in low detail). The TD-H9 panel is 160 x 128, so each column is
// downsampled as it arrives and written at once as a 1-pixel-wide window:
//   x: screen column sx goes to panel column sx / 2 (even columns only; in
//      low detail every submitted column is one panel column)
//   y: screen row y goes to panel row y * 128 / 240, keeping the first
//      screen row of each panel row
// No frame buffer is kept: up to 16 consecutive panel columns (4 KiB) are
// held and sent through one window (pump_end, pump_sync, or a break in the
// columns flushes them). Nothing is pending once those return.

#include <stdint.h>

#include "lcd.h"

#include "td.h"

uint16_t pump_lut[256];
volatile struct pump_stats pump_stats;

static int win_x0, win_y0, win_h, win_double, column;

// Batching (build 29): up to BATCH consecutive panel columns are kept,
// row-major, and sent through one window. Build 28 showed the per-column
// windows (160 a frame) cost about a third of the frame.
enum { BATCH = 16 };
static uint16_t batch[TD_H][BATCH];   // big-endian RGB565 as sent
static unsigned batch_x0, batch_n, batch_first, batch_last;

static void flush(void)
{
    if (!batch_n)
        return;
    h9_lcd_window(batch_x0, batch_first, batch_x0 + batch_n - 1, batch_last);
    uint8_t out[128];
    unsigned used = 0;
    for (unsigned row = batch_first; row <= batch_last; row++) {
        for (unsigned i = 0; i < batch_n; i++) {
            uint16_t c = batch[row][i];
            out[used++] = (uint8_t)(c >> 8);
            out[used++] = (uint8_t)c;
            if (used == sizeof out) {
                h9_lcd_write(out, used);
                used = 0;
            }
        }
    }
    if (used)
        h9_lcd_write(out, used);
    batch_n = 0;
    td_sound_pump();
}

void pump_begin(int x0, int x1, int y0, int y1, int flags)
{
    (void)x1;
    flush();
    win_x0 = x0;
    win_y0 = y0;
    win_h = y1 - y0 + 1;
    win_double = flags & PUMP_DOUBLE;
    column = 0;
}

static unsigned panel_row(int y)
{
    return (unsigned)(y * TD_H / LCD_H);
}

// One screen column of h palette indices starting at screen row y0, into the
// batch at panel column px.
static void put_column(unsigned px, const uint8_t *col, int h, int y0)
{
    unsigned first = panel_row(y0), last = panel_row(y0 + h - 1);
    if (px >= TD_W || first >= TD_H)
        return;
    if (last >= TD_H)
        last = TD_H - 1;
    if (batch_n && (px != batch_x0 + batch_n || first != batch_first || last != batch_last || batch_n == BATCH))
        flush();
    if (!batch_n) {
        batch_x0 = px;
        batch_first = first;
        batch_last = last;
    }
    int prev = -1;
    for (int i = 0; i < h; i++) {
        unsigned row = panel_row(y0 + i);
        if ((int)row == prev)
            continue;  // keep the first screen row of each panel row
        prev = (int)row;
        if (row > last)
            break;
        batch[row][batch_n] = pump_lut[col[i]];
    }
    batch_n++;
}

int td_lcd_skip;  // build 28: key 8 skips the panel output, to time the renderer alone

void pump_submit(const uint8_t *cols, int ncols)
{
    td_pump_strips++;
    if (td_lcd_skip) {
        td_sound_pump();
        pump_stats.fills += (uint32_t)ncols;
        return;
    }
    td_sound_pump();  // keep the DAC fed while the frame is drawn
    for (int k = 0; k < ncols; k++, column++) {
        const uint8_t *col = cols + k * win_h;
        int sx = win_x0 + (win_double ? 2 * column : column);
        if (!win_double && (sx & 1))
            continue;
        put_column((unsigned)sx / 2, col, win_h, win_y0);
    }
    pump_stats.fills += (uint32_t)ncols;
}

void pump_sync(int max)
{
    (void)max;
    flush();
}

int pump_pending(void)
{
    return 0;
}

void pump_end(void)
{
    flush();
}
