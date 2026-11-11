/* GPS: everything the radio's GPS receiver reports, on four pages.
 *
 *   1 POSITION  latitude / longitude, altitude, speed, course, satellites,
 *               HDOP, fix quality, Maidenhead grid
 *   2 SKY       sky plot: every satellite in view at its azimuth / elevation,
 *               coloured by signal strength (north up, centre = overhead)
 *   3 SIGNALS   one bar per satellite: signal-to-noise (dB-Hz)
 *   4 INFO      UTC date and time, last fix saved on the chip, sentence counts
 *
 * Data: NMEA sentences (GGA, RMC, GSV) that the APRS modem forwards into the
 * main-side serial ring as command 0x10 (see aprs_monitor.c). Read only; no
 * keys are needed to start recording: the loader stub saves the fix in the
 * background (modules/common/fix_store.h).
 *
 * Keys: Up = previous page, Menu / Down = next page, Back = leave. */
#include <stdint.h>

#include "fix_store.h"
#include "gfx.h"
#include "h9mod.h"

#ifndef GPS_HOST_TEST   /* the host test supplies its own */
#define RING ((volatile uint8_t *)0x16634)
#define RING_WRITE (*(volatile uint16_t *)0x16E34)
#define RING_READ (*(volatile uint16_t *)0x16E36)
#define TICK (*(const volatile uint32_t *)0x1D8B0)   /* 100 Hz */
#endif
#ifdef GPS_HOST_TEST
#define GPS_TZ host_tz_byte
#else
#define GPS_TZ (*(const volatile uint8_t *)0xC1C6)
#endif
#define RING_MASK 2047u

enum { NMEA = 100, SATS = 32, PAGES = 4, CYAN = 0x07ff };
enum { BG = GFX_BLACK, DIM = GFX_GREY };

struct sat { uint8_t id, con, elev, snr; uint16_t az; };

struct state {
    /* from GGA / RMC */
    int32_t lat, lon, alt10, spd10;       /* microdegrees; altitude and speed (knots) in tenths */
    unsigned course, hdop10, quality, used, view;
    uint8_t status, valid;                /* RMC status char; a position has been seen */
    char time[7], date[7];
    unsigned lines, rmcs, ggas, gsvs;     /* sentences seen */
    uint32_t last_rmc;                    /* tick of the last RMC */
    unsigned nsats, ver;                  /* satellite table size; bumps when it changes */
    unsigned ntypes, maxframe;            /* distinct sentence names seen (types), longest frame */
    char types[8][6];
    struct sat sat[SATS];
    /* ring deframer */
    unsigned frame_len, cmd, escape, nmea_len;
    char nmea[NMEA];
    /* ui */
    unsigned page, key, key_seen, drawn_ver, last_draw;
    int32_t saved_lat, saved_lon;
    uint8_t saved;
};

/* ---- text helpers ------------------------------------------------------- */

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
/* v with `frac` decimals ("12.3"), optional minus sign */
static char *fixed(char *d, int32_t v, unsigned frac)
{
    uint32_t m = (uint32_t)(v < 0 ? -v : v), scale = 1;
    for (unsigned i = 0; i < frac; i++) scale *= 10;
    if (v < 0) *d++ = '-';
    d = num(d, m / scale, 1);
    if (frac) { *d++ = '.'; d = num(d, m % scale, frac); }
    return d;
}
static void pad(char *d, char *start, unsigned width)   /* blank to `width` so text redraws in place */
{
    while ((unsigned)(d - start) < width) *d++ = ' ';
    *d = 0;
}
static void row(unsigned y, const char *s, uint16_t fg)
{
    char b[28];
    char *d = put(b, s);
    pad(d, b, 26);
    gfx_text(0, y, b, 26, 1, fg, BG);
}

/* ---- NMEA --------------------------------------------------------------- */

static unsigned dig(char c) { return c >= '0' && c <= '9' ? (unsigned)(c - '0') : 0; }

/* "-12.34" -> value x 10^frac (extra digits dropped, missing ones zero) */
static int32_t val(const char *s, unsigned frac)
{
    int neg = *s == '-';
    uint32_t v = 0;
    unsigned k = 0;
    if (neg) s++;
    while (*s >= '0' && *s <= '9') v = v * 10 + dig(*s++);
    if (*s == '.') s++;
    for (; k < frac; k++) v = v * 10 + (*s >= '0' && *s <= '9' ? dig(*s++) : 0);
    return neg ? -(int32_t)v : (int32_t)v;
}

/* ddmm.mmmm / dddmm.mmmm + hemisphere -> signed microdegrees; 0 = unusable */
static int coord(const char *f, unsigned deg_digits, char hemi, int32_t *out)
{
    uint32_t deg = 0;
    unsigned i;
    for (i = 0; i < deg_digits; i++) deg = deg * 10 + dig(f[i]);
    if (!f[i] || f[i + 2] != '.') return 0;
    uint32_t micro = deg * 1000000u + (uint32_t)val(f + i, 4) * 5u / 3u;   /* minutes x 1e4 -> microdegrees (x 1e6 / 60 / 1e4) */
    if (hemi == 'S' || hemi == 'W') *out = -(int32_t)micro;
    else if (hemi == 'N' || hemi == 'E') *out = (int32_t)micro;
    else return 0;
    return 1;
}

static void gsv(struct state *st, char **f, unsigned n, char talker)
{
    unsigned total = (unsigned)val(f[0], 0), msg = (unsigned)val(f[1], 0);
    unsigned con = talker == 'P' ? 0 : talker == 'L' ? 1 : talker == 'A' ? 2 : 3;
    (void)total;
    st->view = (unsigned)val(f[2], 0);
    if (msg == 1) {   /* a new report for this constellation: drop its old satellites */
        unsigned keep = 0;
        for (unsigned i = 0; i < st->nsats; i++)
            if (st->sat[i].con != con) st->sat[keep++] = st->sat[i];
        st->nsats = keep;
    }
    for (unsigned g = 3; g + 3 < n && st->nsats < SATS; g += 4) {
        if (!f[g][0]) continue;
        struct sat *s = &st->sat[st->nsats++];
        s->id = (uint8_t)val(f[g], 0); s->con = (uint8_t)con;
        s->elev = (uint8_t)val(f[g + 1], 0); s->az = (uint16_t)val(f[g + 2], 0); s->snr = (uint8_t)val(f[g + 3], 0);
    }
    st->ver++;
}

static void sentence(struct state *st)
{
    char *p = st->nmea, *f[24];
    unsigned n = 0;
    st->lines++;
    if (st->nmea_len < 6 || p[0] != '$') return;
    {   /* remember each distinct sentence name (GNGGA, GNRMC, ...) for the INFO page */
        unsigned t, k;
        for (t = 0; t < st->ntypes; t++) {
            for (k = 0; k < 5 && st->types[t][k] == p[1 + k]; k++) {}
            if (k == 5) break;
        }
        if (t == st->ntypes && t < 8) { for (k = 0; k < 5; k++) st->types[t][k] = p[1 + k]; st->types[t][5] = 0; st->ntypes++; }
    }
    for (unsigned i = 0; i < st->nmea_len && n < 24; i++) {
        if (p[i] == '*') { p[i] = 0; break; }
        if (p[i] == ',') { p[i] = 0; f[n++] = p + i + 1; }
    }
    if (p[3] == 'R' && p[4] == 'M' && p[5] == 'C' && n >= 9) {
        st->rmcs++;
        st->last_rmc = TICK;
        st->status = (uint8_t)f[1][0];
        for (unsigned i = 0; i < 6; i++) { st->time[i] = f[0][i] ? f[0][i] : '-'; st->date[i] = f[8][i] ? f[8][i] : '-'; }
        if (f[1][0] == 'A') {
            int32_t la, lo;
            if (coord(f[2], 2, f[3][0], &la) && coord(f[4], 3, f[5][0], &lo)) { st->lat = la; st->lon = lo; st->valid = 1; }
            st->spd10 = val(f[6], 1);
            st->course = (unsigned)val(f[7], 0);
        }
    } else if (p[3] == 'G' && p[4] == 'G' && p[5] == 'A' && n >= 9) {
        st->ggas++;
        st->quality = (unsigned)val(f[5], 0);
        st->used = (unsigned)val(f[6], 0);
        st->hdop10 = (unsigned)val(f[7], 1);
        if (st->quality) {   /* GGA carries position and time too (no date): useful when RMC is missing */
            int32_t la, lo;
            st->alt10 = val(f[8], 1);
            if (coord(f[1], 2, f[2][0], &la) && coord(f[3], 3, f[4][0], &lo)) { st->lat = la; st->lon = lo; st->valid = 1; st->status = 'A'; }
            if (!st->rmcs) for (unsigned i = 0; i < 6; i++) st->time[i] = f[0][i] ? f[0][i] : '-';
            st->last_rmc = TICK;
        }
    } else if (p[3] == 'G' && p[4] == 'S' && p[5] == 'V' && n >= 7) {
        st->gsvs++;
        gsv(st, f, n, p[2]);
    }
}

static void gps_bytes(struct state *st, const uint8_t *p, unsigned len)
{
    for (unsigned i = 0; i < len; i++) {
        char c = (char)p[i];
        if (c == '$') st->nmea_len = 0;
        if (c == '\r' || c == '\n') { if (st->nmea_len) { st->nmea[st->nmea_len] = 0; sentence(st); } st->nmea_len = 0; continue; }
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
        else { if (++st->frame_len > st->maxframe) st->maxframe = st->frame_len; if (st->cmd == 0x10) gps_bytes(st, &b, 1); }
    }
    RING_READ = (uint16_t)read;
}

/* ---- derived values ----------------------------------------------------- */

/* sin of a bearing in degrees (0..359), Q10, from a 10-degree table */
static int sin_deg(unsigned a)
{
    static const uint16_t T[10] = { 0, 178, 350, 512, 658, 784, 887, 962, 1008, 1024 };
    int neg = a >= 180;
    a %= 180;
    if (a > 90) a = 180 - a;
    unsigned i = a / 10, f = a % 10;
    int v = (int)T[i] + (i < 9 ? ((int)T[i + 1] - (int)T[i]) * (int)f / 10 : 0);
    return neg ? -v : v;
}
static int cos_deg(unsigned a) { return sin_deg((a + 90) % 360); }

static char *grid(char *d, int32_t lat, int32_t lon)   /* Maidenhead, 6 characters */
{
    uint32_t L = (uint32_t)(lon + 180000000), A = (uint32_t)(lat + 90000000), r;
    *d++ = (char)('A' + L / 20000000u); *d++ = (char)('A' + A / 10000000u);
    r = L % 20000000u; *d++ = (char)('0' + r / 2000000u);
    r = A % 10000000u; *d++ = (char)('0' + r / 1000000u);
    r = L % 2000000u;  *d++ = (char)('A' + r * 24u / 2000000u);
    r = A % 1000000u;  *d++ = (char)('A' + r * 24u / 1000000u);
    *d = 0;
    return d;
}

static uint16_t snr_color(unsigned snr)
{
    return !snr ? DIM : snr >= 30 ? GFX_GREEN : snr >= 20 ? GFX_YELLOW : GFX_RED;
}
static uint16_t con_color(unsigned con) { return con == 0 ? GFX_GREEN : con == 1 ? GFX_RED : con == 2 ? CYAN : GFX_YELLOW; }

/* ---- pages -------------------------------------------------------------- */

static void header(const struct state *st, uint32_t now)
{
    static const char *const NAME[PAGES] = { "POSITION", "SKY", "SIGNALS", "INFO" };
    char b[28], *d = put(b, "GPS ");
    d = num(d, st->page + 1, 1); d = put(d, "/4 "); d = put(d, NAME[st->page]);
    pad(d, b, 16);
    gfx_text(0, 0, b, 16, 1, GFX_YELLOW, BG);
    uint32_t age = st->rmcs ? (now - st->last_rmc) / 100u : 99;
    const char *s = !st->lines ? "NO DATA" : age > 3 ? "STALE" : st->status == 'A' ? (st->quality > 1 ? "FIX DGPS" : "FIX") : "NO FIX";
    uint16_t c = !st->lines || age > 3 ? DIM : st->status == 'A' ? GFX_GREEN : GFX_RED;
    d = put(b, s);
    pad(d, b, 9);
    gfx_text(160 - 54, 0, b, 9, 1, c, BG);
    gfx_fill(0, 10, H9_WIDTH, 1, GFX_BLUE);
}

static void page_position(const struct state *st)
{
    char b[28], *d;
    if (!st->valid) { row(14, "NO POSITION YET", GFX_RED); row(25, "", GFX_WHITE); row(36, "", GFX_WHITE); }
    else {
        d = put(b, "LAT "); d = fixed(d, st->lat < 0 ? -st->lat : st->lat, 6); put(d, st->lat < 0 ? " S" : " N");
        row(14, b, GFX_WHITE);
        d = put(b, "LON "); d = fixed(d, st->lon < 0 ? -st->lon : st->lon, 6); put(d, st->lon < 0 ? " W" : " E");
        row(25, b, GFX_WHITE);
        d = put(b, "GRID "); grid(d, st->lat, st->lon);
        row(36, b, GFX_YELLOW);
    }
    d = put(b, "ALT "); d = fixed(d, st->alt10, 1); d = put(d, " M  "); d = num(d, (uint32_t)(st->alt10 < 0 ? 0 : st->alt10) * 328u / 1000u, 1); put(d, " FT");
    row(47, b, GFX_WHITE);
    d = put(b, "SPD "); d = fixed(d, st->spd10 * 1852 / 1000, 1); d = put(d, " KMH  "); d = fixed(d, st->spd10 * 11508 / 10000, 1); put(d, " MPH");
    row(58, b, GFX_WHITE);
    static const char *const PT[16] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
    d = put(b, "CRS "); d = num(d, st->course, 3); d = put(d, " "); put(d, PT[((st->course * 16u + 180u) / 360u) & 15]);
    row(69, b, GFX_WHITE);
    d = put(b, "SATS "); d = num(d, st->used, 1); d = put(d, " USED  "); d = num(d, st->view, 1); put(d, " VIEW");
    row(83, b, st->used >= 4 ? GFX_GREEN : GFX_YELLOW);
    d = put(b, "HDOP "); d = fixed(d, (int32_t)st->hdop10, 1); d = put(d, "  Q"); d = num(d, st->quality, 1);
    put(d, st->quality == 0 ? " NONE" : st->quality == 1 ? " GPS" : st->quality == 2 ? " DGPS" : " OTHER");
    row(94, b, st->hdop10 && st->hdop10 <= 20 ? GFX_GREEN : st->hdop10 <= 50 ? GFX_YELLOW : GFX_WHITE);
    row(112, "UP/DOWN PAGE   BACK EXIT", DIM);
}

static void page_sky(const struct state *st)
{
    enum { CX = 54, CY = 74, R = 44 };
    for (unsigned ring = 1; ring <= 3; ring++)          /* 0, 30 and 60 degree elevation circles */
        for (unsigned a = 0; a < 360; a += 10)
            gfx_fill((unsigned)(CX + sin_deg(a) * (int)(R * ring / 3) / 1024), (unsigned)(CY - cos_deg(a) * (int)(R * ring / 3) / 1024), 1, 1, DIM);
    gfx_fill(CX, CY - R, 1, 2 * R + 1, DIM);
    gfx_fill(CX - R, CY, 2 * R + 1, 1, DIM);
    gfx_text(CX - 2, 12, "N", 1, 1, GFX_WHITE, BG);
    for (unsigned i = 0; i < st->nsats; i++) {
        const struct sat *s = &st->sat[i];
        if (s->elev > 90) continue;
        int r = (int)(90 - s->elev) * R / 90;
        gfx_fill((unsigned)(CX + sin_deg(s->az) * r / 1024) - 1, (unsigned)(CY - cos_deg(s->az) * r / 1024) - 1, 3, 3, snr_color(s->snr));
    }
    char b[12], *d = put(b, "SATS ");
    num(d, st->nsats, 1);
    gfx_text(108, 24, b, 10, 1, GFX_WHITE, BG);
    d = put(b, "USED "); num(d, st->used, 1);
    gfx_text(108, 35, b, 10, 1, GFX_WHITE, BG);
    gfx_text(108, 52, "SIGNAL", 6, 1, DIM, BG);
    gfx_text(108, 63, ">=30", 4, 1, GFX_GREEN, BG);
    gfx_text(108, 74, ">=20", 4, 1, GFX_YELLOW, BG);
    gfx_text(108, 85, "<20", 3, 1, GFX_RED, BG);
    gfx_text(108, 96, "NONE", 4, 1, DIM, BG);
}

static void page_signals(const struct state *st)
{
    for (unsigned i = 0; i < st->nsats; i++) {
        const struct sat *s = &st->sat[i];
        unsigned h = (unsigned)s->snr * 3u / 2u;
        if (h > 80) h = 80;
        unsigned x = i * 5;
        if (h) gfx_fill(x, 108 - h, 4, h, con_color(s->con));
        else gfx_fill(x, 106, 4, 2, DIM);
    }
    gfx_fill(0, 108, H9_WIDTH, 1, GFX_BLUE);
    gfx_text(0, 112, "G", 1, 1, GFX_GREEN, BG);
    gfx_text(8, 112, "GPS", 3, 1, DIM, BG);
    gfx_text(34, 112, "R", 1, 1, GFX_RED, BG);
    gfx_text(42, 112, "GLO", 3, 1, DIM, BG);
    gfx_text(68, 112, "E", 1, 1, CYAN, BG);
    gfx_text(76, 112, "GAL", 3, 1, DIM, BG);
    gfx_text(102, 112, "B", 1, 1, GFX_YELLOW, BG);
    gfx_text(110, 112, "BDS", 3, 1, DIM, BG);
    gfx_text(0, 14, "BARS: SIGNAL 0-50 DB", 20, 1, DIM, BG);
}

static void page_info(const struct state *st, uint32_t now)
{
    char b[28], *d;
    if (st->rmcs && st->time[0] != '-') {
        d = b;
        *d++ = st->time[0]; *d++ = st->time[1]; *d++ = ':'; *d++ = st->time[2]; *d++ = st->time[3]; *d++ = ':';
        *d++ = st->time[4]; *d++ = st->time[5]; put(d, " UTC");
    } else put(b, "NO TIME");
    gfx_text(0, 14, b, 13, 2, GFX_WHITE, BG);
    if (st->rmcs && st->date[0] != '-') {
        d = put(b, "DATE 20"); *d++ = st->date[4]; *d++ = st->date[5]; *d++ = '-'; *d++ = st->date[2]; *d++ = st->date[3];
        *d++ = '-'; *d++ = st->date[0]; *d++ = st->date[1]; *d = 0;
    } else put(b, "DATE -");
    row(34, b, GFX_WHITE);
    if (st->saved) {
        d = put(b, "SAVED "); d = fixed(d, st->saved_lat, 3); *d++ = ' '; fixed(d, st->saved_lon, 3);
    } else put(b, "SAVED FIX NONE");
    row(48, b, st->saved ? GFX_WHITE : DIM);
    {   /* local time from the radio's Time zone setting (UTC-12..+12, option index at 0xC1C6) */
        unsigned v = GPS_TZ;
        int off = v <= 24 ? (int)v - 12 : 0;
        if (st->time[0] != '-' && st->rmcs + st->ggas) {
            unsigned hh = (unsigned)((int)(dig(st->time[0]) * 10 + dig(st->time[1])) + off + 24) % 24;
            d = put(b, "LOCAL "); d = num(d, hh, 2); *d++ = ':'; *d++ = st->time[2]; *d++ = st->time[3];
            d = put(d, "  UTC"); *d++ = off < 0 ? '-' : '+'; num(d, (uint32_t)(off < 0 ? -off : off), 1);
        } else put(b, "");
        row(59, b, GFX_WHITE);
    }
    d = put(b, "RMC "); d = num(d, st->rmcs, 1); d = put(d, " GGA "); d = num(d, st->ggas, 1); d = put(d, " GSV "); num(d, st->gsvs, 1);
    row(76, b, GFX_WHITE);
    d = put(b, "SENTENCES "); d = num(d, st->lines, 1); d = put(d, "  LAST RMC ");
    if (st->rmcs) { d = num(d, (now - st->last_rmc) / 100u, 1); put(d, "S"); } else put(d, "-");
    row(87, b, DIM);
    d = put(b, "STATUS "); *d++ = st->status ? (char)st->status : '-'; d = put(d, "  FRAME "); num(d, st->maxframe, 1);
    row(98, b, DIM);
    d = b;
    for (unsigned t = 0; t < st->ntypes && t < 4; t++) { d = put(d, st->types[t]); *d++ = ' '; }
    *d = 0;
    row(109, b, DIM);
    d = b;
    for (unsigned t = 4; t < st->ntypes; t++) { d = put(d, st->types[t]); *d++ = ' '; }
    *d = 0;
    row(120, b, DIM);
}

static void draw(const struct state *st, uint32_t now, int full)
{
    header(st, now);
    if (st->page == 0) page_position(st);
    else if (st->page == 1) { if (full) page_sky(st); }
    else if (st->page == 2) { if (full) page_signals(st); }
    else page_info(st, now);
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

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)self;
    struct state st;
    uint8_t *z = (uint8_t *)&st;
    for (unsigned i = 0; i < sizeof st; i++) z[i] = 0;
    st.key = st.key_seen = h9_key();
    st.saved = (uint8_t)fix_load_micro(api->read, &st.saved_lat, &st.saved_lon);
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, BG);
    unsigned dirty = 1;
    for (;;) {
        poll_ring(&st);
        unsigned k = key_press(&st);
        if (k == H9_KEY_BACK) return 0;
        if ((k == H9_KEY_MENU || k == H9_KEY_DOWN) && st.page < PAGES - 1) { st.page++; dirty = 1; }
        else if (k == H9_KEY_UP && st.page) { st.page--; dirty = 1; }
        uint32_t now = TICK;
        if (st.page && st.page < 3 && st.ver != st.drawn_ver && now - st.last_draw >= 100) dirty = 1;   /* sky / bars follow the satellite table, once a second */
        if (dirty || now - st.last_draw >= 100) {
            if (dirty) gfx_fill(0, 11, H9_WIDTH, H9_HEIGHT - 11, BG);
            draw(&st, now, dirty);
            st.drawn_ver = st.ver;
            st.last_draw = now;
            dirty = 0;
        }
        h9_delay(10);
    }
}
