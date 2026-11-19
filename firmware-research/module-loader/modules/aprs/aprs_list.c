/* Station list (module ".APRS LIST", entered through "APRS"). Up/Down select, Menu: details
 * (module ".APRS INFO"), 1-5 or hold Menu: other screens (tab bar), Back:
 * module menu. Rows heard live are white with how long ago and the range,
 * rows from stock's Beacon List grey (loaded by aprs_load.c). */
#include "aprs_core.h"

enum { ROW_H = 12, ROWS = 7, AGE_X = 66, INFO_Y = FOOT_Y - 12 };

static void draw_row(unsigned i, unsigned row)
{
    unsigned y = BODY_Y + row * ROW_H;
    int sel = i == A->list_sel;
    uint16_t bg = sel ? UI_ACCENT : UI_BG;
    gfx_fill(0, y, H9_WIDTH, ROW_H, bg);
    if (i >= A->count) return;
    const struct station *s = &A->st[i];
    char r[16];
    gfx_text(4, y + 2, s->call, CALL, 1, sel ? UI_BG : s->live ? UI_TEXT : UI_DIM, bg);
    put_age(r, s);
    gfx_text(AGE_X, y + 2, r, 3, 1, sel ? UI_BG : UI_DIM, bg);
    unsigned n = put_range(r, s);
    gfx_text(H9_WIDTH - 4 - n * 6, y + 2, r, 15, 1, sel ? UI_BG : UI_OK, bg);
}

static void draw_list(void)
{
    char t[32];
    if (A->list_sel < A->list_top) A->list_top = A->list_sel;
    if (A->list_sel >= A->list_top + ROWS) A->list_top = A->list_sel - ROWS + 1;
    aprs_title("APRS STATIONS");
    if (!A->count) {
        gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, UI_BG);
        aprs_line(BODY_Y + 36, "NO STATIONS HEARD YET", UI_ACCENT, UI_BG);
    } else {
        gfx_fill(0, TITLE_H, H9_WIDTH, BODY_Y - TITLE_H, UI_BG);
        for (unsigned row = 0; row < ROWS; row++) draw_row(A->list_top + row, row);
        gfx_fill(0, BODY_Y + ROWS * ROW_H, H9_WIDTH, INFO_Y - BODY_Y - ROWS * ROW_H, UI_BG);
    }
    if (A->unread) {
        unsigned n = put_uint(t, A->unread);
        put_str(t + n, " NEW MESSAGE: PRESS 3");
        aprs_line(INFO_Y, t, UI_OK, UI_BG);
    } else {
        /* The receiver stays where it was when the app opened (Dual Watch
         * pauses with the stock loop): show the working RX frequency, RAM
         * 0x11C8C in 10 Hz units (h9-firmware-research main-aprs-route-readback.md). */
        unsigned f = *(volatile uint32_t *)0x11C8C, n = put_str(t, "RX ");
        n += put_uint(t + n, f / 100000);
        t[n++] = '.';
        unsigned k = f % 100000 / 10;                /* kHz and 100 Hz: 3900 */
        for (unsigned d = 1000; d; d /= 10) t[n++] = (char)('0' + k / d % 10);
        put_str(t + n, " MHZ  MENU INFO");
        aprs_line(INFO_Y, t, UI_DIM, UI_BG);
    }
    aprs_tabs(TAB_LIST);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    aprs_open();
    for (;;) {
        unsigned ev = aprs_event();
        if (ev == EV_REDRAW) draw_list();
        else if (aprs_tab(api, ev, TAB_LIST)) return 0;
        else if (ev == EV_HOLD) { if (aprs_go(api, APRS_MAP) || aprs_go(api, APRS_MSG)) return 0; }
        else if (ev == EV_BACK) { aprs_side(A->home_side); SPEAKER_ON(A->home_speaker); return 0; }
        else if (!A->count) continue;
        else if (ev == EV_UP) { A->list_sel = A->list_sel ? A->list_sel - 1 : A->count - 1; A->dirty = 1; }
        else if (ev == EV_DOWN) { A->list_sel = A->list_sel + 1 < A->count ? A->list_sel + 1 : 0; A->dirty = 1; }
        else if (ev == EV_MENU && aprs_go(api, APRS_INFO)) return 0;
    }
}
