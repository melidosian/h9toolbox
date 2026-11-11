/* Host test for the GPS app: NMEA parsing, derived values and page text. */
#include <stdio.h>
#include <string.h>

static unsigned char host_ring[2048];
static unsigned short host_write, host_read;
static unsigned host_tick = 1000;
static unsigned char host_tz_byte = 7;   /* UTC-5 */
#define GPS_HOST_TEST
#define RING host_ring
#define RING_WRITE host_write
#define RING_READ host_read
#define TICK host_tick

/* stubs for the radio-side helpers */
#include "h9mod.h"
#include "gfx.h"
static char screen[16][64];
static int rows_seen;
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
unsigned gfx_text(unsigned x, unsigned y, const char *t, unsigned max, unsigned s, uint16_t fg, uint16_t bg)
{
    (void)x; (void)s; (void)fg; (void)bg;
    unsigned r = y / 11;
    if (r < 16) { strncpy(screen[r], t, max); screen[r][max] = 0; if ((int)r + 1 > rows_seen) rows_seen = (int)r + 1; }
    return x + 6 * max;
}
void gfx_wait_release(void) {}
unsigned gfx_wait_key(void) { return 0; }
unsigned h9_key(void) { return 0; }
void h9_delay(unsigned u) { (void)u; }
int fix_load_micro(void (*r)(void *, uint32_t, uint32_t), int32_t *a, int32_t *b) { (void)r; (void)a; (void)b; return 0; }

#define module_main gps_main
#include "gps.c"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
#define SEQ(s) strcmp(screen[1], s) == 0

static void feed(struct state *st, const char *nmea)   /* as the modem frames it: C0 10 text CRLF C0 */
{
    host_ring[host_write++ & 2047] = 0xC0; host_ring[host_write++ & 2047] = 0x10;
    for (const char *p = nmea; *p; p++) host_ring[host_write++ & 2047] = (unsigned char)*p;
    host_ring[host_write++ & 2047] = '\r'; host_ring[host_write++ & 2047] = '\n';
    host_ring[host_write++ & 2047] = 0xC0;
    poll_ring(st);
}

/* One frame with several sentences, as the modem batches them (over 100 bytes). */
static void feed_batch(struct state *st, const char *const *lines, unsigned n)
{
    host_ring[host_write++ & 2047] = 0xC0; host_ring[host_write++ & 2047] = 0x10;
    for (unsigned i = 0; i < n; i++) {
        for (const char *p = lines[i]; *p; p++) host_ring[host_write++ & 2047] = (unsigned char)*p;
        host_ring[host_write++ & 2047] = '\r'; host_ring[host_write++ & 2047] = '\n';
    }
    host_ring[host_write++ & 2047] = 0xC0;
    poll_ring(st);
}

int main(void)
{
    static struct state st;
    memset(&st, 0, sizeof st);
    feed(&st, "$GNRMC,123519.00,A,4152.6800,N,08737.8000,W,5.4,275.5,101126,,,A*00");
    feed(&st, "$GNGGA,123519.00,4152.6800,N,08737.8000,W,1,09,0.9,192.3,M,-33.0,M,,*00");
    feed(&st, "$GPGSV,2,1,06,02,45,120,38,05,10,300,20,12,80,010,45,25,30,200,0*00");
    feed(&st, "$GPGSV,2,2,06,29,60,040,41,31,05,250,15*00");
    feed(&st, "$GLGSV,1,1,02,70,20,100,33,71,40,200,18*00");
    CHECK(host_read == host_write);
    CHECK(st.lines == 5 && st.rmcs == 1 && st.ggas == 1 && st.gsvs == 3);
    CHECK(st.valid && st.lat == 41878000 && st.lon == -87630000);
    CHECK(st.status == 'A' && st.quality == 1 && st.used == 9 && st.hdop10 == 9 && st.alt10 == 1923);
    CHECK(st.spd10 == 54 && st.course == 275);
    CHECK(!memcmp(st.time, "123519", 6) && !memcmp(st.date, "101126", 6));
    CHECK(st.nsats == 8 && st.view == 2);
    CHECK(st.sat[0].id == 2 && st.sat[0].elev == 45 && st.sat[0].az == 120 && st.sat[0].snr == 38 && st.sat[0].con == 0);
    CHECK(st.sat[3].snr == 0 && st.sat[5].id == 31 && st.sat[6].con == 1 && st.sat[6].id == 70);
    /* a new GPS report replaces the GPS satellites only */
    feed(&st, "$GPGSV,1,1,01,09,15,090,22*00");
    CHECK(st.nsats == 3 && st.sat[0].con == 1 && st.sat[2].con == 0 && st.sat[2].id == 9);

    char b[32];
    grid(b, st.lat, st.lon);
    CHECK(!strcmp(b, "EN61EV"));
    grid(b, -33869000, 151209000);   /* Sydney */
    CHECK(!strcmp(b, "QF56OD"));
    CHECK(sin_deg(0) == 0 && sin_deg(90) == 1024 && sin_deg(180) == 0 && sin_deg(270) == -1024);
    CHECK(sin_deg(45) > 715 && sin_deg(45) < 735 && cos_deg(0) == 1024 && cos_deg(180) == -1024);

    /* no fix: status V keeps the last position, drops nothing */
    feed(&st, "$GNRMC,123520.00,V,,,,,,,101126,,,N*00");
    CHECK(st.status == 'V' && st.valid && st.lat == 41878000);
    /* southern / western hemispheres, no leading-zero loss */
    feed(&st, "$GNRMC,000001.00,A,3352.1400,S,15112.5400,E,0.0,0.0,010127,,,A*00");
    CHECK(st.lat == -33869000 && st.lon == 151209000);

    /* pages: text */
    st.page = 0; memset(screen, 0, sizeof screen);
    page_position(&st);
    CHECK(strstr(screen[1], "LAT 33.869000 S") == screen[1]);
    page_info(&st, host_tick); CHECK(st.ntypes >= 3);
    CHECK(strstr(screen[5], "LOCAL 19:00  UTC-5") == screen[5]);
    printf("rows: %s | %s | %s\n", screen[1], screen[4], screen[7]);
    /* the real shape: GGA + RMC + GSV in one frame, far over 100 bytes */
    memset(&st, 0, sizeof st);
    static const char *const batch[] = {
        "$GNGGA,123519.00,4152.6800,N,08737.8000,W,1,09,0.9,192.3,M,-33.0,M,,*00",
        "$GNGSA,A,3,02,05,12,25,29,31,,,,,,,1.8,0.9,1.5*00",
        "$GNRMC,123519.00,A,4152.6800,N,08737.8000,W,5.4,275.5,101126,,,A*00",
        "$GPGSV,2,1,06,02,45,120,38,05,10,300,20,12,80,010,45,25,30,200,0*00",
        "$GPGSV,2,2,06,29,60,040,41,31,05,250,15*00",
    };
    feed_batch(&st, batch, 5);
    CHECK(st.ggas == 1 && st.rmcs == 1 && st.gsvs == 2 && st.nsats == 6 && st.valid);
    /* GGA alone (what the radio showed on 2026-11-10): position, time, altitude */
    memset(&st, 0, sizeof st);
    feed(&st, "$GNGGA,123519.00,4152.6800,N,08737.8000,W,1,09,0.9,192.3,M,-33.0,M,,*00");
    CHECK(st.valid && st.lat == 41878000 && st.status == 'A' && !memcmp(st.time, "123519", 6) && st.rmcs == 0);
    /* torn / garbage input never crashes and never makes a position */
    memset(&st, 0, sizeof st);
    feed(&st, "$GNRMC,12,A,41,N,08,W,1,2,3*00");
    feed(&st, "garbage");
    feed(&st, "$GPGSV,1,1,99,1,2,3*00");
    CHECK(!st.valid);
    printf(failures ? "%d failure(s)\n" : "gps ok\n", failures);
    return failures != 0;
}
