/* SAT PASS: the next five FM repeater satellite passes over your position.
 * Times are in the radio's own time zone setting (UTC when it is UTC+0), shown
 * with an L (local) or Z (UTC) after the clock.
 *
 * Position and UTC come from the radio's GPS ($xxRMC sentences, which the
 * APRS modem forwards into the main-side APRS ring as command 0x10). Until a
 * fix arrives, the last fix saved on the resource chip (fix_store.h; the
 * firmware records it in the background) is used, so passes show as soon as
 * the GPS reports the time. The module stays below APRS's state in RAM, so
 * APRS's own last fix survives too. The
 * The orbits
 * are the satellite table in the hidden module .SAT DATA (satdata.h; made by
 * satpack.py or the App Manager's satellite settings), propagated by orbit.c.
 *
 * Keys: Up / Down select a pass, Menu opens the PLOT page (a horizon plot of the
 * pass; Up / Down step through the passes there), Back leaves. */
#include <stdint.h>

#include "gfx.h"
#include "h9mod.h"
#include "fix_store.h"           /* the last fix across power-off */
#include "orbit.h"
#include "satshare.h"
#include "satdata.h"

#define RING ((volatile uint8_t *)0x16634)
#define RING_WRITE (*(volatile uint16_t *)0x16E34)
#define RING_READ (*(volatile uint16_t *)0x16E36)
#define RING_MASK 2047u
#define TICK (*(const volatile uint32_t *)0x1D8B0)   /* 100 Hz */

enum { WANT = 5, HOURS = 36, NMEA = 80 };
enum { BG = GFX_BLACK, DIM = GFX_GREY };

struct state {
    uint32_t base_unix, base_tick;    /* UTC at a tick */
    int32_t lat, lon;                 /* binary angles */
    uint8_t have_time, have_pos, gps_fix, computing, saved;
    unsigned sel, found, frame_len, cmd, escape, nmea_len, key, key_seen, last_draw;
    uint32_t computed_at;
    int32_t calc_lat, calc_lon, store_lat, store_lon;
    struct satdata data;               /* the satellite table (read through the loader) */
    struct pass p[WANT];
    char nmea[NMEA];
};

/* ---- small text helpers (no library) ---------------------------------- */

static char *put(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *num(char *d, unsigned v, unsigned width)
{
    char t[10];
    unsigned n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n < width) t[n++] = '0';
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}
static char *hhmm(char *d, uint32_t unix, int seconds)
{
    uint32_t s = sat_local(unix) % 86400u;
    d = num(d, s / 3600, 2); *d++ = ':';
    d = num(d, s / 60 % 60, 2);
    if (seconds) { *d++ = ':'; d = num(d, s % 60, 2); }
    return d;
}
static char *span(char *d, uint32_t seconds)  /* 7m, 1h05, 9h */
{
    unsigned m = (seconds + 59) / 60;
    if (m < 60) { d = num(d, m, 1); return put(d, "m"); }
    d = num(d, m / 60, 1); *d++ = 'h';
    return num(d, m % 60, 2);
}

/* ---- GPS --------------------------------------------------------------- */

static unsigned dig(char c) { return c >= '0' && c <= '9' ? (unsigned)(c - '0') : 0; }

static uint32_t days_from_civil(unsigned y, unsigned m, unsigned d)
{
    y -= m <= 2;
    unsigned era = y / 400, yoe = y - era * 400;
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* ddmm.mmmm or dddmm.mmmm with a hemisphere letter -> binary angle. */
static int coord(const char *f, unsigned deg_digits, char hemi, int32_t *out)
{
    unsigned deg = 0, i;
    for (i = 0; i < deg_digits; i++) deg = deg * 10 + dig(f[i]);
    unsigned minutes = dig(f[i]) * 10 + dig(f[i + 1]);
    if (f[i + 2] != '.') return 0;
    unsigned frac = 0, scale = 10000;
    for (unsigned k = i + 3; f[k] >= '0' && f[k] <= '9' && scale > 1; k++) { scale /= 10; frac += dig(f[k]) * scale; }
    uint32_t e4min = (deg * 60 + minutes) * 10000 + frac;           /* 1e-4 arc minutes */
    int32_t angle = (int32_t)(((uint64_t)e4min * 333599386u) >> 24);  /* 2^56 / 216e6 */
    if (hemi == 'S' || hemi == 'W') angle = -angle;
    else if (hemi != 'N' && hemi != 'E') return 0;
    *out = angle;
    return 1;
}

/* $G?RMC,hhmmss.ss,A,ddmm.mmmm,N,dddmm.mmmm,W,speed,course,ddmmyy,... */
static void rmc(struct state *st)
{
    char *p = st->nmea;
    const char *f[10];
    unsigned n = 0;
    if (st->nmea_len < 6 || p[3] != 'R' || p[4] != 'M' || p[5] != 'C') return;
    for (unsigned i = 0; i < st->nmea_len && n < 10; i++)
        if (p[i] == ',') { p[i] = 0; f[n++] = p + i + 1; }
    if (n < 9) return;
    const char *t = f[0], *date = f[8];
    if (t[0] && date[0] && date[5]) {
        unsigned day = dig(date[0]) * 10 + dig(date[1]), mon = dig(date[2]) * 10 + dig(date[3]);
        unsigned year = 2000 + dig(date[4]) * 10 + dig(date[5]);
        unsigned secs = (dig(t[0]) * 10 + dig(t[1])) * 3600 + (dig(t[2]) * 10 + dig(t[3])) * 60 + dig(t[4]) * 10 + dig(t[5]);
        if (mon >= 1 && mon <= 12 && day >= 1 && day <= 31 && year >= 2024) {
            st->base_unix = days_from_civil(year, mon, day) * 86400u + secs;
            st->base_tick = TICK;
            st->have_time = 1;
        }
    }
    int32_t lat, lon;
    st->gps_fix = f[1][0] == 'A' && coord(f[2], 2, f[3][0], &lat) && coord(f[4], 3, f[5][0], &lon);
    if (st->gps_fix) { st->lat = lat; st->lon = lon; st->have_pos = 1; }
}

static void gps_bytes(struct state *st, const uint8_t *p, unsigned len)
{
    for (unsigned i = 0; i < len; i++) {
        char c = (char)p[i];
        if (c == '$') st->nmea_len = 0;
        if (c == '\r' || c == '\n') { if (st->nmea_len) { st->nmea[st->nmea_len] = 0; rmc(st); } st->nmea_len = 0; continue; }
        if (st->nmea_len < NMEA - 1) st->nmea[st->nmea_len++] = c;
    }
}

/* The APRS modem's frames: C0 <cmd> <payload> C0, cmd 0x10 = GPS NMEA.
 * One frame can carry several sentences (GGA, RMC, GSV...), far more than a
 * frame buffer should hold, so the payload is fed to the line assembler
 * byte by byte as it arrives. */
static void poll_ring(struct state *st)
{
    unsigned write = RING_WRITE & RING_MASK, read = RING_READ & RING_MASK;
    while (read != write) {
        uint8_t b = RING[read];
        read = (read + 1) & RING_MASK;
        if (b == 0xC0) { st->frame_len = 0; st->escape = 0; continue; }
        if (st->escape) { b = b == 0xDC ? 0xC0 : b == 0xDD ? 0xDB : b; st->escape = 0; }
        else if (b == 0xDB) { st->escape = 1; continue; }
        if (!st->frame_len) { st->cmd = b; st->frame_len = 1; }
        else if (st->cmd == 0x10) gps_bytes(st, &b, 1);
    }
    RING_READ = (uint16_t)read;
}

static uint32_t now_of(const struct state *st) { return st->base_unix + (TICK - st->base_tick) / 100u; }

/* ---- screens ----------------------------------------------------------- */

/* orbit_passes fetches satellites through this (ctx = the satellite table) */
static void get_sat(void *ctx, unsigned i, struct sat *out)
{
    struct satentry e;
    satdata_get((const struct satdata *)ctx, i, &e);
    *out = e.sat;
}

static void header(const struct state *st, uint32_t now)
{
    char b[32], *d = b;
    gfx_fill(0, 0, H9_WIDTH, 22, BG);
    gfx_text(0, 0, "SAT PASS", 8, 1, GFX_YELLOW, BG);
    if (st->have_time) { d = hhmm(b, now, 1); *d++ = sat_zone_letter(); *d = 0; gfx_text(106, 0, b, 9, 1, GFX_WHITE, BG); }
    const char *gps = st->gps_fix ? "GPS OK" : st->have_pos ? "LAST FIX" : "NO FIX";
    gfx_text(0, 11, gps, 8, 1, st->gps_fix ? GFX_GREEN : GFX_RED, BG);
    /* the elements' date; red once they are over two weeks old */
    d = put(b, "TLE "); put(d, st->data.head.date);
    gfx_text(160 - 6 * 14, 11, b, 14, 1, now > st->data.head.newest + 14 * 86400u ? GFX_RED : DIM, BG);
}

static void list(const struct state *st, uint32_t now, int full)
{
    if (full) gfx_fill(0, 22, H9_WIDTH, H9_HEIGHT - 22, BG);
    if (!st->have_time || !st->have_pos) {
        gfx_text(0, 44, "WAITING FOR GPS", 15, 1, GFX_WHITE, BG);
        return;
    }
    if (st->computing) { gfx_text(0, 50, "CALCULATING...", 14, 1, GFX_WHITE, BG); return; }
    gfx_text(0, 24, "SAT    AOS  MAX  DUR IN", 23, 1, DIM, BG);
    for (unsigned i = 0; i < st->found; i++) {
        const struct pass *p = &st->p[i];
        char b[40], *d = b;
        struct satentry e;
        satdata_get(&st->data, p->sat, &e);
        d = put(d, e.sat.name);
        while (d < b + 7) *d++ = ' ';
        d = hhmm(d, p->aos, 0);
        *d++ = ' ';
        if (p->max_el < 10) *d++ = ' ';
        d = num(d, p->max_el, 1);
        d = put(d, p->max_el < 100 ? "  " : " ");
        d = span(d, p->los - p->aos);
        while (d < b + 23) *d++ = ' ';
        if (now >= p->aos) d = put(d, "NOW");
        else d = span(d, p->aos - now);
        while (d < b + 26) *d++ = ' ';   /* fixed width: the refresh overwrites in place */
        *d = 0;
        uint16_t fg = now >= p->aos ? GFX_GREEN : p->max_el >= 30 ? GFX_YELLOW : GFX_WHITE;
        unsigned y = 36 + i * 13;
        if (i == st->sel) gfx_fill(0, y - 2, H9_WIDTH, 12, GFX_BLUE);
        gfx_text(0, y, b, 26, 1, fg, i == st->sel ? GFX_BLUE : BG);
    }
    if (!st->found) gfx_text(0, 50, "NO PASSES IN 36H", 16, 1, GFX_WHITE, BG);
}

/* A key once two readings agree (the keypad ADC has no debounce). */
static unsigned key_press(struct state *st)
{
    unsigned k = h9_key();
    if (k != st->key) { st->key = k; return 0; }
    if (k == st->key_seen) return 0;
    st->key_seen = k;
    return k;
}

static int far(int32_t a, int32_t b)  /* more than about 0.1 degree apart */
{
    int32_t d = a - b;
    return d > 1200000 || d < -1200000;
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    struct state st;
    uint8_t *z = (uint8_t *)&st;
    for (unsigned i = 0; i < sizeof st; i++) z[i] = 0;
    st.key = st.key_seen = h9_key();
    if (!satdata_open(api, &st.data)) {   /* no satellite table in the bundle (install the SAT PASS app again) */
        gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, BG);
        gfx_text(0, 40, "NO SATELLITE DATA", 17, 1, GFX_RED, BG);
        gfx_text(0, 54, "REINSTALL SAT PASS", 18, 1, GFX_WHITE, BG);
        gfx_wait_release();
        gfx_wait_key();
        return 0;
    }
    st.have_pos = (uint8_t)fix_load_micro(api->read, &st.lat, &st.lon);
    st.lat = fix_bam(st.lat); st.lon = fix_bam(st.lon);
    st.store_lat = st.lat; st.store_lon = st.lon; st.saved = !st.have_pos ? 2 : 0;   /* 2: nothing stored yet */
    volatile struct satshare *sh = SAT_SHARED;
    if (sh->magic == SAT_MAGIC) {   /* back from the pass plot: carry on at the same time and place */
        st.base_unix = sh->now; st.base_tick = sh->tick;
        st.have_time = st.have_pos = 1; st.sel = sh->sel;
        sh->magic = 0;
    }
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, BG);
    unsigned dirty = 1;
    for (;;) {
        poll_ring(&st);
        uint32_t now = now_of(&st);
        if (st.gps_fix && st.saved != 1) {   /* the first fix of a session: keep it on the chip for the next start */
            if (st.saved == 2 || far(st.lat, st.store_lat) || far(st.lon, st.store_lon))
                fix_save(api->read, fix_micro(st.lat), fix_micro(st.lon));
            st.saved = 1;
        }
        if (st.have_time && st.have_pos &&
            (!st.computed_at || far(st.lat, st.calc_lat) || far(st.lon, st.calc_lon) || (st.found && now >= st.p[0].los) || now - st.computed_at > 3600)) {
            st.computing = 1;
            list(&st, now, 1);
            struct observer o;
            orbit_observer(&o, st.lat, st.lon);
            struct gmst_ref gmst;
            satdata_gmst(&st.data, &gmst);
            st.found = orbit_passes(get_sat, &st.data, st.data.head.count, &gmst, &o, now, HOURS, 0, st.p, WANT);
            st.computing = 0;
            st.computed_at = now;
            st.calc_lat = st.lat; st.calc_lon = st.lon;
            if (st.sel >= st.found) st.sel = 0;
            dirty = 1;
        }
        unsigned k = key_press(&st);
        if (k == H9_KEY_BACK) return 0;
        else if (k == H9_KEY_MENU && st.found) {   /* the pass plot (hidden module .SAT PLOT) steps through the list */
            volatile struct h9_request *rq = H9_REQUEST;
            sh->lat = st.lat; sh->lon = st.lon; sh->now = now; sh->tick = TICK; sh->sel = (uint8_t)st.sel; sh->count = (uint8_t)st.found;
            for (unsigned i = 0; i < st.found; i++) {
                sh->aos[i] = st.p[i].aos;
                sh->dur_sat[i] = (uint16_t)(st.p[i].sat << 12 | (st.p[i].los - st.p[i].aos));
            }
            sh->magic = SAT_MAGIC;
            for (unsigned i = 0; i < H9_NAME_BYTES; i++) rq->name[i] = i < 9 ? ".SAT PLOT"[i] : 0;
            rq->magic = H9_REQUEST_MAGIC;
            return 0;
        }
        else if (k == H9_KEY_UP && st.sel) { st.sel--; dirty = 1; }
        else if (k == H9_KEY_DOWN && st.sel + 1 < st.found) { st.sel++; dirty = 1; }
        if (dirty || TICK - st.last_draw >= 100) {
            header(&st, now);
            list(&st, now, (int)dirty);
            if (st.found) sat_tabs(0);
            st.last_draw = TICK;
            dirty = 0;
        }
        h9_delay(10);
    }
}
