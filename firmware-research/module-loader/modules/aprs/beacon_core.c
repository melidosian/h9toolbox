#include "beacon_core.h"

static unsigned digits(const char *s, unsigned n)
{
    for (unsigned i = 0; i < n; i++) if (s[i] < '0' || s[i] > '9') return 0;
    return 1;
}

/* "12.34" -> tenths (123); "" -> 0 */
static unsigned tenths(const char *s)
{
    unsigned v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (unsigned)(*s++ - '0');
    v *= 10;
    if (*s == '.' && s[1] >= '0' && s[1] <= '9') v += (unsigned)(s[1] - '0');
    return v;
}

int beacon_rmc(char *p, struct beacon_gps *g)
{
    const char *f[8];
    unsigned n = 0, len = 0;
    while (p[len]) len++;
    if (len < 6 || p[0] != '$' || p[3] != 'R' || p[4] != 'M' || p[5] != 'C') return 0;
    for (unsigned i = 0; i < len && n < 8; i++)
        if (p[i] == ',') { p[i] = 0; f[n++] = p + i + 1; }
    if (n < 7) return 0;
    g->seen = 1;
    const char *la = f[2], *lo = f[4];
    unsigned ok = f[1][0] == 'A' &&
                  digits(la, 4) && la[4] == '.' && digits(la + 5, 2) && (f[3][0] == 'N' || f[3][0] == 'S') &&
                  digits(lo, 5) && lo[5] == '.' && digits(lo + 6, 2) && (f[5][0] == 'E' || f[5][0] == 'W');
    g->fix = (uint8_t)ok;
    if (!ok) return 1;
    for (unsigned i = 0; i < 7; i++) g->lat[i] = la[i];
    g->lat[7] = f[3][0]; g->lat[8] = 0;
    for (unsigned i = 0; i < 8; i++) g->lon[i] = lo[i];
    g->lon[8] = f[5][0]; g->lon[9] = 0;
    unsigned kn10 = n > 6 ? tenths(f[6]) : 0;
    g->knots10 = (uint16_t)(kn10 > 9999 ? 9999 : kn10);
    g->kmh10 = (uint16_t)(g->knots10 * 1852u / 1000u);
    if (n > 7 && f[7][0] >= '0' && f[7][0] <= '9') g->course = (uint16_t)(tenths(f[7]) / 10 % 360);
    return 1;
}

int beacon_feed(struct beacon_feed *f, uint8_t b, struct beacon_gps *g)
{
    int got = 0;
    if (b == 0xC0) {
        if (f->started && f->cmd == 0x10 && f->len) {            /* a whole GPS frame: the line may end with the frame */
            f->nmea[f->len] = 0;
            got = beacon_rmc(f->nmea, g);
        }
        f->started = 1; f->cmd = 0xFF; f->esc = 0; f->len = 0;
        return got;
    }
    if (!f->started) return 0;
    if (f->cmd == 0xFF) { f->cmd = b; return 0; }                /* first byte after C0: the command */
    if (f->cmd != 0x10) return 0;                                /* AX.25 frames and the rest: skipped */
    if (f->esc) { b = b == 0xDC ? 0xC0 : b == 0xDD ? 0xDB : b; f->esc = 0; }
    else if (b == 0xDB) { f->esc = 1; return 0; }
    if (b == '$') f->len = 0;
    if (b == '\r' || b == '\n') {
        if (f->len) { f->nmea[f->len] = 0; got = beacon_rmc(f->nmea, g); }
        f->len = 0;
        return got;
    }
    if (f->len < BEACON_NMEA - 1) f->nmea[f->len++] = (char)b;
    return got;
}

static unsigned put3(char *o, unsigned v)
{
    o[0] = (char)('0' + v / 100 % 10); o[1] = (char)('0' + v / 10 % 10); o[2] = (char)('0' + v % 10);
    return 3;
}

unsigned beacon_info(char *out, const struct beacon_gps *g, char symbol, const char *comment)
{
    unsigned n = 0;
    out[n++] = '!';
    for (unsigned i = 0; g->lat[i]; i++) out[n++] = g->lat[i];
    out[n++] = '/';                                              /* primary symbol table */
    for (unsigned i = 0; g->lon[i]; i++) out[n++] = g->lon[i];
    out[n++] = symbol;
    unsigned knots = (g->knots10 + 5u) / 10u;
    if (knots >= 1) {
        n += put3(out + n, g->course == 0 ? 360 : g->course);    /* APRS: 001-360, 000 = unknown */
        out[n++] = '/';
        n += put3(out + n, knots > 999 ? 999 : knots);
    }
    if (comment && *comment) {
        out[n++] = ' ';
        for (unsigned i = 0; comment[i] && n < BEACON_INFO - 1; i++) out[n++] = comment[i];
    }
    out[n] = 0;
    return n;
}

/* "DDMM.hh" (deg_digits 2) or "DDDMM.hh" (3) and the hemisphere letter from microdegrees. */
static unsigned coord(char *out, int32_t micro, unsigned deg_digits, char pos, char neg)
{
    uint32_t a = (uint32_t)(micro < 0 ? -(int64_t)micro : micro);
    uint32_t deg = a / 1000000u, hund = (a % 1000000u) * 6u / 1000u;        /* hundredths of a minute, truncated */
    unsigned n = 0;
    if (deg_digits == 3) out[n++] = (char)('0' + deg / 100 % 10);
    out[n++] = (char)('0' + deg / 10 % 10); out[n++] = (char)('0' + deg % 10);
    out[n++] = (char)('0' + hund / 1000 % 10); out[n++] = (char)('0' + hund / 100 % 10); out[n++] = '.';
    out[n++] = (char)('0' + hund / 10 % 10); out[n++] = (char)('0' + hund % 10);
    out[n++] = micro < 0 ? neg : pos;
    out[n] = 0;
    return n;
}

int beacon_from_micro(struct beacon_gps *g, int32_t lat, int32_t lon)
{
    if (lat < -90000000 || lat > 90000000 || lon < -180000000 || lon > 180000000) return 0;
    coord(g->lat, lat, 2, 'N', 'S');
    coord(g->lon, lon, 3, 'E', 'W');
    g->knots10 = 0; g->kmh10 = 0;
    return 1;
}

int beacon_timed_due(uint32_t now_ms, uint32_t last_ms, unsigned sent, unsigned minutes)
{
    uint32_t gap = sent ? minutes * 60000u : 15000u;
    return now_ms - last_ms >= gap;
}
