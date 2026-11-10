/* .APRS INFO: details of the station selected in the list (module
 * ".APRS INFO"; the leading dot hides it from the module menu). Menu: send
 * this station a message (APRS SEND), Back: list, hold Menu: map. */
#include "aprs_core.h"

static unsigned put_coord(char *out, int32_t v, unsigned deg_digits, char pos, char neg)
{
    unsigned a = (unsigned)(v < 0 ? -v : v), deg = a / 6000, rest = a % 6000, n = 0;
    if (deg_digits == 3) out[n++] = (char)('0' + deg / 100);
    out[n++] = (char)('0' + deg / 10 % 10); out[n++] = (char)('0' + deg % 10);
    out[n++] = (char)('0' + rest / 1000); out[n++] = (char)('0' + rest / 100 % 10); out[n++] = '.';
    out[n++] = (char)('0' + rest / 10 % 10); out[n++] = (char)('0' + rest % 10);
    out[n++] = v < 0 ? neg : pos;
    out[n] = 0;
    return n;
}

static void draw_detail(void)
{
    const struct station *s = &A->st[A->list_sel];
    char b[28];
    unsigned y = BODY_Y + 2;
    aprs_title(s->call);
    gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, GFX_BLACK);
    if (s->has_pos) {
        unsigned n = put_coord(b, s->lat, 2, 'N', 'S');
        b[n++] = ' ';
        put_coord(b + n, s->lon, 3, 'E', 'W');
        aprs_line(y, b, GFX_WHITE, GFX_BLACK); y += 11;
        n = put_str(b, "SYMBOL ");
        b[n++] = (char)s->table; b[n++] = (char)s->symbol; b[n] = 0;
        aprs_line(y, b, GFX_GREY, GFX_BLACK); y += 11;
        put_range(b, s);
        aprs_line(y, b, GFX_GREEN, GFX_BLACK); y += 13;
    } else {
        aprs_line(y, "NO POSITION", GFX_GREY, GFX_BLACK); y += 13;
    }
    aprs_wrap(y, s->text, FOOT_Y - 2, GFX_WHITE);
    aprs_line(FOOT_Y, s->live ? "LIVE  MENU: SEND  BACK" : "STORED  MENU: SEND  BACK", GFX_GREY, GFX_BLACK);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    aprs_open();
    for (;;) {
        unsigned ev = aprs_event();
        if (ev == EV_REDRAW) {
            if (A->list_sel < A->count) draw_detail();
            else if (aprs_go(api, APRS_LIST)) return 0;
            else return 0;
        }
        else if (ev == EV_HOLD) { if (aprs_go(api, APRS_MAP) || aprs_go(api, APRS_MSG)) return 0; }
        else if (ev == EV_MENU) {
            A->send_sel = 1;                        /* first message to this station */
            if (aprs_go(api, APRS_SEND)) return 0;
            aprs_line(FOOT_Y, "SEND NOT INSTALLED", GFX_RED, GFX_BLACK);
        }
        else if (ev == EV_BACK) { aprs_go(api, APRS_LIST); return 0; }
    }
}
