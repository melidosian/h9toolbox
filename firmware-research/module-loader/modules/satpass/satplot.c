/* .SAT PLOT: one satellite pass drawn on a horizon (sky) plot.
 *
 * Opened from SAT PASS's detail view (Menu). The plot shows the sky as seen
 * from you: north at the top, east on the right, the rim is the horizon and
 * the centre is straight overhead. The line is the satellite's path from
 * rise (green dot) to set (red dot), with its highest point in yellow, and
 * the rings mark 30 and 60 degrees of elevation. The path is calculated here
 * with orbit.c from the pass list SAT PASS left in satshare.h. Up / Down step
 * through the listed passes; Menu or Back returns to SAT PASS, which carries
 * on from the same time at the pass last viewed. */
#include <stdint.h>

#include "gfx.h"
#include "h9mod.h"
#include "orbit.h"
#include "satshare.h"
#include "sats.h"

enum { CX = 58, CY = 64, R = 44, SAMPLES = 48 };

static char *put(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *num(char *d, uint32_t v, unsigned width)
{
    char t[10];
    unsigned n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n < width) t[n++] = '0';
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}

/* sky position -> pixel: azimuth from north clockwise, elevation 0..90 (binary angles) */
static void plot_xy(uint32_t az, int32_t el, unsigned *x, unsigned *y)
{
    int32_t s, c;
    int32_t r = (int32_t)(((int64_t)R * (0x40000000 - el)) >> 30);
    orbit_sincos(az, &s, &c);
    *x = (unsigned)(CX + (int32_t)(((int64_t)s * r) >> 30));
    *y = (unsigned)(CY - (int32_t)(((int64_t)c * r) >> 30));
}

static void dot(uint32_t az, int32_t el, unsigned size, uint16_t color)
{
    unsigned x, y;
    plot_xy(az, el, &x, &y);
    gfx_fill(x - size / 2, y - size / 2, size, size, color);
}

static void time_text(char *d, uint32_t t)
{
    uint32_t s = sat_local(t) % 86400u;
    d = num(d, s / 3600, 2); *d++ = ':'; d = num(d, s / 60 % 60, 2);
    *d++ = sat_zone_letter(); *d = 0;
}
static void az_text(char *d, uint32_t az)
{
    d = put(d, "AZ "); num(d, orbit_deg(az), 3);
}

/* Draw pass `n` of the shared list: grid, path, markers and the side panel. */
static void draw_pass(const struct satshare *s, unsigned n, const struct observer *o)
{
    uint32_t aos = s->aos[n], los = aos + (s->dur_sat[n] & 0xFFF);
    const struct sat *sat = &SATS[(s->dur_sat[n] >> 12) < SATS_COUNT ? s->dur_sat[n] >> 12 : 0];
    gfx_fill(0, 0, H9_WIDTH, 119, GFX_BLACK);
    /* rings (horizon, 30, 60 degrees) and the compass cross */
    for (unsigned ring = 0; ring < 3; ring++)
        for (unsigned a = 0; a < 64; a++)
            dot(a << 26, (int32_t)ring * (0x40000000 / 3), 1, GFX_GREY);
    gfx_fill(CX, CY - R, 1, 2 * R + 1, GFX_GREY);
    gfx_fill(CX - R, CY, 2 * R + 1, 1, GFX_GREY);
    gfx_text(CX - 3, CY - R - 10, "N", 1, 1, GFX_WHITE, GFX_BLACK);
    gfx_text(CX - 3, CY + R + 3, "S", 1, 1, GFX_WHITE, GFX_BLACK);
    gfx_text(CX + R + 4, CY - 3, "E", 1, 1, GFX_WHITE, GFX_BLACK);
    gfx_text(CX - R - 10, CY - 3, "W", 1, 1, GFX_WHITE, GFX_BLACK);

    uint32_t span = los > aos ? los - aos : 1;
    int32_t peak = -0x40000000;
    uint32_t peak_az = 0;
    struct look l;
    for (unsigned i = 0; i <= SAMPLES; i++) {
        orbit_look(sat, &GMST_REF, o, aos + span * i / SAMPLES, &l);
        if (l.el < 0) continue;
        if (l.el > peak) { peak = l.el; peak_az = l.az; }
        dot(l.az, l.el, 2, GFX_WHITE);
    }
    orbit_look(sat, &GMST_REF, o, aos, &l);
    uint32_t az_aos = l.az;
    dot(az_aos, 0, 5, GFX_GREEN);
    orbit_look(sat, &GMST_REF, o, los, &l);
    uint32_t az_los = l.az;
    dot(az_los, 0, 5, GFX_RED);
    dot(peak_az, peak, 5, GFX_YELLOW);

    char b[16];
    gfx_text(116, 2, sat->name, 7, 1, GFX_YELLOW, GFX_BLACK);
    gfx_text(116, 18, "RISE", 4, 1, GFX_GREEN, GFX_BLACK);
    time_text(b, aos); gfx_text(116, 28, b, 6, 1, GFX_WHITE, GFX_BLACK);
    az_text(b, az_aos); gfx_text(116, 38, b, 6, 1, GFX_GREY, GFX_BLACK);
    gfx_text(116, 54, "SET", 3, 1, GFX_RED, GFX_BLACK);
    time_text(b, los); gfx_text(116, 64, b, 6, 1, GFX_WHITE, GFX_BLACK);
    az_text(b, az_los); gfx_text(116, 74, b, 6, 1, GFX_GREY, GFX_BLACK);
    gfx_text(116, 90, "HIGH", 4, 1, GFX_YELLOW, GFX_BLACK);
    char *d = num(b, orbit_deg((uint32_t)peak), 1);
    put(d, " DEG");
    gfx_text(116, 100, b, 6, 1, GFX_WHITE, GFX_BLACK);
    d = num(b, n + 1, 1); *d++ = '/'; num(d, s->count, 1);   /* "2/5" */
    gfx_text(116, 110, b, 3, 1, GFX_GREY, GFX_BLACK);
    sat_tabs(1);
    gfx_text(72, 119, "UP/DN PASS", 10, 1, GFX_WHITE, GFX_BLACK);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)api; (void)self;
    volatile struct satshare *sh = SAT_SHARED;
    struct satshare s = *sh;
    if (s.magic != SAT_MAGIC || !s.count || s.count > 5) return 0;   /* not opened from SAT PASS */
    struct observer o;
    orbit_observer(&o, s.lat, s.lon);
    unsigned n = s.sel < s.count ? s.sel : 0;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, GFX_BLACK);
    draw_pass(&s, n, &o);
    gfx_wait_release();
    for (;;) {
        unsigned k = gfx_wait_key();
        if (k == H9_KEY_UP && n) draw_pass(&s, --n, &o);
        else if (k == H9_KEY_DOWN && n + 1 < s.count) draw_pass(&s, ++n, &o);
        else if (k == H9_KEY_UP || k == H9_KEY_DOWN) { /* at the end of the list: stay */ }
        else break;
        gfx_wait_release();
    }
    sh->sel = (uint8_t)n;   /* SAT PASS carries on at the pass last viewed */
    volatile struct h9_request *rq = H9_REQUEST;      /* back to SAT PASS */
    for (unsigned i = 0; i < H9_NAME_BYTES; i++) rq->name[i] = i < 9 ? "SAT PASS"[i] : 0;
    rq->magic = H9_REQUEST_MAGIC;
    return 0;
}
