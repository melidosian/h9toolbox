/* APRS: station list (module "APRS"). Up/Down select, Menu: details
 * (module ".APRS INFO"), hold Menu: map, Back: module menu. Rows heard live
 * are white, rows from stock's Beacon List grey. On a fresh start it loads
 * stock's Beacon List. */
#include "aprs_core.h"

enum { ROW_H = 12, ROWS = 8 };

static void draw_row(unsigned i, unsigned row)
{
    unsigned y = BODY_Y + row * ROW_H;
    int sel = i == A->list_sel;
    uint16_t bg = sel ? GFX_YELLOW : GFX_BLACK;
    gfx_fill(0, y, H9_WIDTH, ROW_H, bg);
    if (i >= A->count) return;
    const struct station *s = &A->st[i];
    char r[16];
    gfx_text(4, y + 2, s->call, CALL, 1, sel ? GFX_BLACK : s->live ? GFX_WHITE : GFX_GREY, bg);
    unsigned n = put_range(r, s);
    gfx_text(H9_WIDTH - 4 - n * 6, y + 2, r, 15, 1, sel ? GFX_BLACK : GFX_GREEN, bg);
}

static void draw_list(void)
{
    char t[24];
    if (A->list_sel < A->list_top) A->list_top = A->list_sel;
    if (A->list_sel >= A->list_top + ROWS) A->list_top = A->list_sel - ROWS + 1;
    aprs_title("APRS");
    if (!A->count) {
        gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, GFX_BLACK);
        aprs_line(BODY_Y + 30, "NO STATIONS YET", GFX_YELLOW, GFX_BLACK);
        aprs_line(BODY_Y + 42, "LISTENING...", GFX_GREY, GFX_BLACK);
    } else {
        gfx_fill(0, TITLE_H, H9_WIDTH, BODY_Y - TITLE_H, GFX_BLACK);
        for (unsigned row = 0; row < ROWS; row++) draw_row(A->list_top + row, row);
        gfx_fill(0, BODY_Y + ROWS * ROW_H, H9_WIDTH, FOOT_Y - BODY_Y - ROWS * ROW_H, GFX_BLACK);
    }
    if (A->unread) {
        unsigned n = put_uint(t, A->unread);
        put_str(t + n, " NEW MSG  HOLD MENU");
        aprs_line(FOOT_Y, t, GFX_GREEN, GFX_BLACK);
    } else {
        /* The receiver stays where it was when the app opened (Dual Watch
         * pauses with the stock loop): show the working RX frequency, RAM
         * 0x11C8C in 10 Hz units (h9-firmware-research main-aprs-route-readback.md). */
        unsigned f = *(volatile uint32_t *)0x11C8C, n = put_str(t, "RX ");
        n += put_uint(t + n, f / 100000);
        t[n++] = '.';
        unsigned k = f % 100000 / 10;                /* kHz and 100 Hz: 3900 */
        for (unsigned d = 1000; d; d /= 10) t[n++] = (char)('0' + k / d % 10);
        put_str(t + n, " MHZ");
        aprs_line(FOOT_Y, t, GFX_GREY, GFX_BLACK);
    }
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    if (aprs_open()) aprs_load_stored(api);
    for (;;) {
        unsigned ev = aprs_event();
        if (ev == EV_REDRAW) draw_list();
        else if (ev == EV_HOLD) { if (aprs_go(api, APRS_MAP) || aprs_go(api, APRS_MSG)) return 0; }
        else if (ev == EV_BACK) { aprs_side(A->home_side); SPEAKER_ON(A->home_speaker); return 0; }
        else if (!A->count) continue;
        else if (ev == EV_UP) { A->list_sel = A->list_sel ? A->list_sel - 1 : A->count - 1; A->dirty = 1; }
        else if (ev == EV_DOWN) { A->list_sel = A->list_sel + 1 < A->count ? A->list_sel + 1 : 0; A->dirty = 1; }
        else if (ev == EV_MENU && aprs_go(api, APRS_INFO)) return 0;
    }
}
