/* .SAT TRACK: live view of one pass with Doppler-corrected frequencies.
 *
 * Third page of SAT PASS (tabs LIST PLOT TRACK). For the pass SAT PASS or the
 * plot left selected, once a second: where the satellite is (azimuth,
 * elevation, range), the time to rise or set, and the frequencies to use:
 *   DOWN  what to listen on   = downlink - Doppler shift
 *   UP    what to transmit on = uplink   + Doppler shift
 * so the satellite hears you on its own channel. The shift comes from the
 * change of the slant range over eight seconds centred on now (orbit_look's
 * range), scaled by f / c (doppler.h). Before the rise the figures are those at the rise.
 * SEND (uplink and its tone) is on top, LISTEN (downlink) below. Up / Down switch pass; Back
 * returns to SAT PASS (the list).
 *
 * Menu opens the VFO TRACKING start screen (what will be written, which side listens, whether
 * the other side sends); starting it hands over to the hidden module .SAT TRACKER (satauto.c), which
 * arms the resident tracker and restarts the radio. */
#include <stdint.h>

#include "doppler.h"
#include "gfx.h"
#include "h9mod.h"
#include "orbit.h"
#include "satshare.h"
#include "satdata.h"

#ifndef TICK   /* host renders supply their own */
#define TICK (*(const volatile uint32_t *)0x1D8B0)   /* 100 Hz */
#endif

enum { BG = GFX_BLACK, DIM = GFX_GREY };

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
static void pad(char *d, char *start, unsigned width)
{
    while ((unsigned)(d - start) < width) *d++ = ' ';
    *d = 0;
}
static void row(unsigned y, const char *s, uint16_t fg)
{
    char b[32];   /* the longest start-screen line is 28 characters: only 26 are shown */
    pad(put(b, s), b, 26);
    gfx_text(0, y, b, 26, 1, fg, BG);
}
/* 10 Hz units -> "436.7952" (MHz, 100 Hz resolution) */
static char *mhz(char *d, uint32_t f10)
{
    d = num(d, f10 / 100000u, 1);
    *d++ = '.';
    return num(d, f10 % 100000u / 10u, 4);
}
/* |shift| (10 Hz units) as kHz with one decimal: "9.8" */
static char *khz(char *d, int32_t shift10)
{
    uint32_t v = (uint32_t)(shift10 < 0 ? -shift10 : shift10) / 10u;   /* 100 Hz */
    d = num(d, v / 10u, 1);
    *d++ = '.';
    return num(d, v % 10u, 1);
}
static char *mmss(char *d, uint32_t s)
{
    d = num(d, s / 60, 2); *d++ = ':';
    return num(d, s % 60, 2);
}

static void draw(const struct satdata *data, const struct satshare *sh, unsigned n, const struct observer *o, uint32_t now)
{
    uint32_t aos = sh->aos[n], los = aos + (sh->dur_sat[n] & 0xFFF);
    struct satentry entry;
    struct gmst_ref gmst;
    satdata_get(data, sh->dur_sat[n] >> 12, &entry);
    satdata_gmst(data, &gmst);
    const struct sat *sat = &entry.sat;
    const struct satfreq *f = &entry.freq;
    uint32_t t = now < aos ? aos : now > los ? los : now;
    struct look a, before, after;
    orbit_look(sat, &gmst, o, t, &a);
    orbit_look(sat, &gmst, o, t - DOPPLER_HALF, &before);
    orbit_look(sat, &gmst, o, t + DOPPLER_HALF, &after);
    int32_t shift_dn = doppler_shift(f->down10, after.range - before.range);
    int32_t shift_up = doppler_shift(f->up10, after.range - before.range);
    char s[28], *d;

    d = put(s, sat->name); *d++ = ' '; d = num(d, n + 1, 1); *d++ = '/'; num(d, sh->count, 1);
    row(2, s, GFX_YELLOW);
    if (now < aos) { d = put(s, "RISES IN "); mmss(d, aos - now); }
    else if (now <= los) { d = put(s, "IN VIEW, SETS IN "); mmss(d, los - now); }
    else put(s, "PASS OVER");
    row(14, s, now >= aos && now <= los ? GFX_GREEN : GFX_WHITE);
    d = put(s, now < aos ? "AT RISE AZ " : "AZ "); d = num(d, orbit_deg(a.az), 3);
    d = put(d, " EL "); d = num(d, a.el < 0 ? 0 : orbit_deg((uint32_t)a.el), 2);
    row(26, s, GFX_WHITE);
    d = put(s, "RANGE "); d = num(d, (uint32_t)a.range / 256u, 1); put(d, " KM");
    row(37, s, DIM);

    /* SEND on top (with its tone), LISTEN below */
    d = put(s, "SEND   "); mhz(d, (uint32_t)((int32_t)f->up10 + shift_up));
    row(52, s, GFX_YELLOW);
    d = put(s, "  BASE "); d = mhz(d, f->up10); *d++ = ' '; *d++ = shift_up > 0 ? '+' : '-';
    khz(d, shift_up);
    row(62, s, DIM);
    if (f->tone10) { d = put(s, "  TONE "); d = num(d, f->tone10 / 10u, 1); *d++ = '.'; d = num(d, f->tone10 % 10u, 1); put(d, " HZ"); }
    else put(s, "  NO TONE");
    row(72, s, GFX_WHITE);
    d = put(s, "LISTEN "); mhz(d, (uint32_t)((int32_t)f->down10 - shift_dn));
    row(88, s, GFX_GREEN);
    d = put(s, "  BASE "); d = mhz(d, f->down10); *d++ = ' '; *d++ = shift_dn > 0 ? '-' : '+';
    khz(d, shift_dn);
    row(98, s, DIM);
    sat_tabs(2);
    gfx_text(96, 119, "MENU:VFO", 8, 1, GFX_WHITE, BG);
}

/* Leave the choices where .SAT TRACKER finds them and ask the loader for it (it arms and restarts the radio). */
static void start_tracking(const struct h9_api *api, const struct satshare *s, unsigned n, unsigned flags, unsigned only)
{
    volatile struct satshare *sh = SAT_SHARED;
    sh->now = s->now + (TICK - s->tick) / 100u;
    sh->tick = TICK;
    sh->sel = (uint8_t)n;
    /* start with the chosen pass: drop the ones before it from the list */
    for (unsigned i = n; i < s->count; i++) { sh->aos[i - n] = s->aos[i]; sh->dur_sat[i - n] = s->dur_sat[i]; }
    sh->count = (uint8_t)(only ? 1 : s->count - n);   /* only the chosen pass, or it and every later one in the list */
    sh->st_flags = flags;
    sh->magic = SAT_MAGIC;
    volatile struct h9_request *rq = H9_REQUEST;
    const char *name = ".SAT TRACKER";
    unsigned k = 0;
    while (name[k]) { rq->name[k] = name[k]; k++; }
    while (k < H9_NAME_BYTES) rq->name[k++] = 0;
    rq->magic = H9_REQUEST_MAGIC;
    (void)api;
}

/* The start screen: what will be written, and the choices (which side listens, send on/off, squelch, one pass,
 * PF1). Menu starts, Back cancels, 1 starts the test. */
static void confirm(const struct h9_api *api, const struct satdata *data, const struct satshare *s, unsigned n, unsigned *done)
{
    unsigned transmit = 1, swap = 1, open = 1, only = 1, ptt2 = 1;   /* defaults: B listens, A sends (on), squelch open, this pass only, PF1 = PTT2 */
    char b[28], *d;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, BG);
    gfx_wait_release();
    for (;;) {
        struct satentry entry;
        satdata_get(data, s->dur_sat[n] >> 12, &entry);
        d = put(b, "VFO TRACKING "); d = put(d, entry.sat.name);
        row(2, b, GFX_YELLOW);
        /* VFO A on top, VFO B below, whatever each one does */
        const char *send = transmit ? "SEND (UP) ON" : "SEND (UP) OFF";
        d = put(b, "VFO A = "); put(d, swap ? send : "LISTEN (DOWN)");
        row(16, b, swap ? (transmit ? GFX_RED : DIM) : GFX_GREEN);
        d = put(b, "VFO B = "); put(d, swap ? "LISTEN (DOWN)" : send);
        row(28, b, swap ? GFX_GREEN : (transmit ? GFX_RED : DIM));
        row(40, "UP: SEND  DOWN: SWAP A/B", DIM);
        row(50, open ? "2: SQUELCH OPEN IN PASS ON" : "2: SQUELCH OPEN IN PASS OFF", open ? GFX_GREEN : DIM);
        row(62, only ? "3: ONLY THIS PASS  ON" : "3: ONLY THIS PASS  OFF", only ? GFX_GREEN : DIM);
        row(73, ptt2 ? "4: PF1 = PTT2 (TX ON A) ON" : "4: PF1 = PTT2 (TX ON A) OFF", ptt2 ? GFX_GREEN : DIM);
        row(84, "RESTARTS. HOLD BACK = STOP", GFX_WHITE);
        row(95, "1 = TEST NOW (ABOVE CHOICES)", GFX_GREEN);
        row(106, "MENU START   BACK CANCEL", GFX_YELLOW);
        unsigned k = gfx_wait_key();
        if (k == H9_KEY_UP) transmit ^= 1;
        else if (k == H9_KEY_DOWN) swap ^= 1;
        else if (k == H9_KEY_2) open ^= 1;
        else if (k == H9_KEY_3) only ^= 1;
        else if (k == H9_KEY_4) ptt2 ^= 1;
        else if (k == H9_KEY_MENU) { start_tracking(api, s, n, (transmit ? 2u : 0u) | (swap ? 16u : 0u) | (open ? 32u : 0u) | (ptt2 ? 512u : 0u), only); *done = 1; return; }
        else if (k == H9_KEY_1) { start_tracking(api, s, n, 8u | (transmit ? 2u : 0u) | (swap ? 16u : 0u) | (open ? 32u : 0u) | (ptt2 ? 512u : 0u), only); *done = 1; return; }   /* test now: acts at once, at the rise frequencies, with the choices above */
        else if (k == H9_KEY_BACK) return;
        gfx_wait_release();
    }
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    volatile struct satshare *sh = SAT_SHARED;
    struct satshare s = *sh;
    if (s.magic != SAT_MAGIC || !s.count || s.count > 5) return 0;
    struct satdata data;
    if (!satdata_open(api, &data)) return 0;
    struct observer o;
    orbit_observer(&o, s.lat, s.lon);
    unsigned n = s.sel < s.count ? s.sel : 0;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, BG);
    gfx_wait_release();
    uint32_t last = 0;
    for (;;) {
        uint32_t tick = TICK;
        if (!last || tick - last >= 100) { draw(&data, &s, n, &o, s.now + (tick - s.tick) / 100u); last = tick; }
        unsigned k = h9_key();
        if (k == H9_KEY_UP && n) { n--; last = 0; }
        else if (k == H9_KEY_DOWN && n + 1 < s.count) { n++; last = 0; }
        else if (k == H9_KEY_BACK) break;
        else if (k == H9_KEY_MENU) {
            unsigned done = 0;
            confirm(api, &data, &s, n, &done);
            if (done) return 0;   /* .SAT TRACKER was requested: the loader runs it next */
            gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, BG);
            last = 0;
        }
        if (k) gfx_wait_release();
        h9_delay(20);
    }
    sh->sel = (uint8_t)n;
    volatile struct h9_request *rq = H9_REQUEST;
    for (unsigned i = 0; i < H9_NAME_BYTES; i++) rq->name[i] = i < 9 ? "SAT PASS"[i] : 0;
    rq->magic = H9_REQUEST_MAGIC;
    return 0;
}
