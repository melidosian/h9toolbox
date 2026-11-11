/* APRS MAP: stations plotted around your GPS position (module "APRS MAP").
 * Up/Down zoom, Menu: next station, 1-5 or hold Menu: other screens, Back: station list.
 * Without a GPS fix the map centres on the selected station. */
#include "aprs_core.h"

enum { MAP_Y = TITLE_H, MAP_H = 90, CX = 80, CY = MAP_Y + 45, RADIUS_PX = 42, ZOOMS = 9, INFO_Y = FOOT_Y - 10 };
static const uint16_t RADIUS_KM[ZOOMS] = { 1, 2, 5, 10, 20, 50, 100, 200, 500 };

/* Map position of (lat, lon); 0 when outside the map. 54 units of 18.52 m
 * make a kilometre. */
static int project(int32_t lat, int32_t lon, unsigned *x, unsigned *y)
{
    int32_t e, n, per = (int32_t)RADIUS_KM[A->map_zoom] * 54;
    aprs_offset(lat, lon, &e, &n);
    if (e > per * 2 || e < -per * 2 || n > per * 2 || n < -per * 2) return 0;
    int32_t px = CX + e * RADIUS_PX / per, py = CY - n * RADIUS_PX / per;
    if (px < 2 || px > H9_WIDTH - 3 || py < MAP_Y + 2 || py > MAP_Y + MAP_H - 3) return 0;
    *x = (unsigned)px; *y = (unsigned)py;
    return 1;
}

/* Station index after `from` that has a position (A->count if none). */
static unsigned next_positioned(unsigned from)
{
    for (unsigned k = 1; k <= A->count; k++) {
        unsigned i = (from + k) % A->count;
        if (A->st[i].has_pos) return i;
    }
    return A->count;
}

static void draw(void)
{
    char b[28];
    unsigned x, y;
    if (A->map_sel >= A->count || !A->st[A->map_sel].has_pos) A->map_sel = A->count ? next_positioned(A->count - 1) : 0;
    int sel_ok = A->map_sel < A->count;
    int32_t keep_lat = A->my_lat, keep_lon = A->my_lon;
    if (!A->my_fix && sel_ok) { A->my_lat = A->st[A->map_sel].lat; A->my_lon = A->st[A->map_sel].lon; }

    aprs_title("MAP");
    gfx_fill(0, MAP_Y, H9_WIDTH, MAP_H, GFX_BLACK);
    if (A->my_fix) { gfx_fill(CX - 4, CY, 9, 1, GFX_GREEN); gfx_fill(CX, CY - 4, 1, 9, GFX_GREEN); }
    for (unsigned i = 0; i < A->count; i++) {
        const struct station *s = &A->st[i];
        if (!s->has_pos || i == A->map_sel || !project(s->lat, s->lon, &x, &y)) continue;
        gfx_fill(x - 1, y - 1, 3, 3, s->live ? GFX_WHITE : GFX_GREY);
    }
    if (sel_ok && project(A->st[A->map_sel].lat, A->st[A->map_sel].lon, &x, &y)) {
        gfx_fill(x - 2, y - 2, 5, 5, GFX_YELLOW);
        unsigned lx = x + 5 + 6 * 9 > H9_WIDTH ? x - 5 - 6 * 9 : x + 5;
        gfx_text(lx, y > MAP_Y + 10 ? y - 4 : y, A->st[A->map_sel].call, CALL, 1, GFX_YELLOW, GFX_BLACK);
    }
    unsigned n = put_str(b, "R ");
    n += put_uint(b + n, RADIUS_KM[A->map_zoom]);
    put_str(b + n, "KM");
    gfx_text(2, MAP_Y + MAP_H - 9, b, 8, 1, GFX_GREY, GFX_BLACK);

    A->my_lat = keep_lat; A->my_lon = keep_lon;
    if (sel_ok) {
        n = put_str(b, A->st[A->map_sel].call);
        b[n++] = ' ';
        put_range(b + n, &A->st[A->map_sel]);
        aprs_line(INFO_Y, b, GFX_WHITE, GFX_BLACK);
    } else {
        aprs_line(INFO_Y, "NO POSITIONS YET", GFX_GREY, GFX_BLACK);
    }
    aprs_tabs(TAB_MAP);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    aprs_open();
    for (;;) {
        unsigned ev = aprs_event();
        if (ev == EV_REDRAW) draw();
        else if (aprs_tab(api, ev, TAB_MAP)) return 0;
        else if (ev == EV_HOLD) { if (aprs_go(api, APRS_MSG) || aprs_go(api, APRS_LIST)) return 0; }
        else if (ev == EV_BACK) { aprs_go(api, APRS_LIST); return 0; }
        else if (ev == EV_UP && A->map_zoom) { A->map_zoom--; A->dirty = 1; }
        else if (ev == EV_DOWN && A->map_zoom + 1 < ZOOMS) { A->map_zoom++; A->dirty = 1; }
        else if (ev == EV_MENU && A->count) { A->map_sel = next_positioned(A->map_sel); A->dirty = 1; }
    }
}
