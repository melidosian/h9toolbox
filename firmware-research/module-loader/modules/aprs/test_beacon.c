/* Host test for APRS BEACON: the RMC parser, the KISS ring feeder, the packet
 * and the vendored scheduler. cc -std=c11 -Wall -Wextra -fsanitize=address,undefined
 *   -Wno-missing-braces test_beacon.c beacon_core.c aprs_tx.c third_party/smartbeacon/smartbeacon.c */
#include <stdio.h>
#include <string.h>

#include "aprs_tx.h"
#include "beacon_core.h"
#include "third_party/smartbeacon/smartbeacon.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static int feed_str(struct beacon_feed *f, struct beacon_gps *g, const char *kiss_cmd_and_text, unsigned len)
{
    int got = 0;
    for (unsigned i = 0; i < len; i++) got |= beacon_feed(f, (uint8_t)kiss_cmd_and_text[i], g);
    return got;
}

static void parse_tests(void)
{
    struct beacon_gps g;
    char a[] = "$GNRMC,123519,A,4807.0380,N,01131.0000,E,022.4,084.4,230394,003.1,W*6A";
    memset(&g, 0, sizeof g);
    CHECK(beacon_rmc(a, &g) == 1 && g.fix && g.seen);
    CHECK(!strcmp(g.lat, "4807.03N") && !strcmp(g.lon, "01131.00E"));
    CHECK(g.knots10 == 224 && g.kmh10 == 414 && g.course == 84);

    char v[] = "$GNRMC,,V,,,,,,,,,,N*4D";                      /* no fix: nothing used */
    CHECK(beacon_rmc(v, &g) == 1 && !g.fix && g.seen);

    char nocourse[] = "$GPRMC,010203,A,5130.1200,N,00007.3400,W,0.0,,101126,,,A*00";
    g.course = 123;
    CHECK(beacon_rmc(nocourse, &g) == 1 && g.fix && g.course == 123 && g.knots10 == 0);   /* the last course is kept */

    char gga[] = "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    CHECK(beacon_rmc(gga, &g) == 0);
    char bad[] = "$GNRMC,1,A,48x7.0380,N,01131.0000,E,1,1,1,,,";  /* a corrupted latitude is refused */
    CHECK(beacon_rmc(bad, &g) == 1 && !g.fix);
    char shortline[] = "$GNRMC,1,A";
    CHECK(beacon_rmc(shortline, &g) == 0);
}

static void feed_tests(void)
{
    struct beacon_feed f;
    struct beacon_gps g;
    memset(&f, 0, sizeof f); memset(&g, 0, sizeof g);
    /* garbage, then an AX.25 frame (ignored), then a GPS frame split over two frames, with an escaped byte */
    const char junk[] = "\x01\x02\x03";
    CHECK(!feed_str(&f, &g, junk, 3));
    const char ax[] = "\xC0\x00\x82\xA0$GNRMC\xC0";
    CHECK(!feed_str(&f, &g, ax, sizeof ax - 1) && !g.seen);
    const char part1[] = "\xC0\x10$GNRMC,123519,A,4807.0380,N,01131.0";
    const char part2[] = "000,E,022.4,084.4,230394,003.1,W*6A\r\n\xC0";
    CHECK(!feed_str(&f, &g, part1, sizeof part1 - 1));
    CHECK(feed_str(&f, &g, part2, sizeof part2 - 1) && g.fix && g.knots10 == 224);
    /* an escaped C0 (DB DC) inside the payload becomes one byte and does not end the frame */
    const char esc[] = "\xC0\x10$GNRMC,1,A,4807.0380,N,01131.0000,E,5.5,10.0,1,,\xDB\xDC\xDB\xDD,\r\n\xC0";
    CHECK(feed_str(&f, &g, esc, sizeof esc - 1) && g.fix && g.knots10 == 55);
    /* a GPS line that ends with the frame (no CR/LF) still counts */
    const char nocrlf[] = "\xC0\x10$GNRMC,1,A,4807.0380,N,01131.0000,E,6.0,20.0,1,,,A\xC0";
    CHECK(feed_str(&f, &g, nocrlf, sizeof nocrlf - 1) && g.knots10 == 60 && g.course == 20);
}

static void packet_tests(void)
{
    struct beacon_gps g = { "4807.03N", "01131.00E", 224, 414, 84, 1, 1 };
    char info[BEACON_INFO];
    unsigned n = beacon_info(info, &g, '>', "H9 Toolbox");
    CHECK(n == strlen(info) && !strcmp(info, "!4807.03N/01131.00E>084/022 H9 Toolbox"));
    g.knots10 = 3; g.course = 0;                                  /* under a knot: no course/speed */
    beacon_info(info, &g, '[', "H9 Toolbox");
    CHECK(!strcmp(info, "!4807.03N/01131.00E[ H9 Toolbox"));
    g.knots10 = 10; g.course = 0;                                  /* one knot, moving north: 360 */
    beacon_info(info, &g, 'b', "");
    CHECK(!strcmp(info, "!4807.03N/01131.00Eb360/001"));
    uint8_t kiss[APRS_TX_KISS_MAX];
    beacon_info(info, &g, '>', "H9 Toolbox");
    CHECK(aprs_tx_kiss(kiss, "N0CALL-9", APRS_TX_DEST, APRS_TX_PATH, info) > 20);
}

static void saved_and_timed_tests(void)
{
    struct beacon_gps g;
    memset(&g, 0, sizeof g);
    CHECK(beacon_from_micro(&g, 41878000, -87630000) && !strcmp(g.lat, "4152.68N") && !strcmp(g.lon, "08737.80W"));
    CHECK(beacon_from_micro(&g, -33868800, 151209300) && !strcmp(g.lat, "3352.12S") && !strcmp(g.lon, "15112.55E"));
    CHECK(beacon_from_micro(&g, 0, 0) && !strcmp(g.lat, "0000.00N") && !strcmp(g.lon, "00000.00E"));
    CHECK(!beacon_from_micro(&g, 91000000, 0) && !beacon_from_micro(&g, 0, -181000000));
    char info[BEACON_INFO];
    beacon_from_micro(&g, 41878000, -87630000);
    beacon_info(info, &g, '[', "H9 Toolbox, last known");
    CHECK(!strcmp(info, "!4152.68N/08737.80W[ H9 Toolbox, last known"));     /* no course/speed: a saved position is stationary */
    /* timed: 15 s after starting, then every N minutes */
    CHECK(!beacon_timed_due(14999, 0, 0, 5) && beacon_timed_due(15000, 0, 0, 5));
    CHECK(!beacon_timed_due(299999, 0, 1, 5) && beacon_timed_due(300000, 0, 1, 5));
    CHECK(beacon_timed_due(1000 + 60000, 1000, 1, 1) && !beacon_timed_due(1000 + 59999, 1000, 1, 1));
    CHECK(beacon_timed_due(5, 0xFFFFFFF0u, 1, 1) == 0);                           /* the 32-bit clock wraps */
}

/* The vendored scheduler as the screen drives it (once a second, boot 10 s). */
struct sim { sb_state_t sb; unsigned fired[8]; unsigned n; };
static void run(struct sim *m, unsigned from_s, unsigned to_s, unsigned fix, unsigned kmh10, unsigned course)
{
    for (unsigned s = from_s; s < to_s; s++) {
        sb_on_rmc(&m->sb, s * 1000u, (uint8_t)fix, (uint16_t)kmh10, (uint16_t)course);
        if (sb_tick(&m->sb, s * 1000u, 0) && m->n < 8) m->fired[m->n++] = s;
    }
}
static void scheduler_tests(void)
{
    sb_config_t cfg = SB_DEFAULTS;
    cfg.boot_s = 10;
    struct sim m;
    memset(&m, 0, sizeof m);
    sb_init(&m.sb, &cfg);
    run(&m, 0, 5, 0, 0, 0);          /* no fix yet: nothing, even after the boot time? */
    CHECK(m.n == 0);
    run(&m, 5, 30, 1, 0, 0);         /* a fix appears at 5 s: one beacon once the 15 s guard since start is over */
    CHECK(m.n == 1 && m.fired[0] == 15);
    m.n = 0;
    run(&m, 30, 200, 1, 600, 90);    /* 60 km/h: every 60 s (fast_s) */
    CHECK(m.n >= 2 && m.fired[1] - m.fired[0] == 60);
    unsigned before = m.n;
    run(&m, 200, 201, 1, 600, 180);  /* a sharp corner at speed: at once (past the 15 s minimum) */
    CHECK(m.n == before + 1 && m.fired[m.n - 1] == 200);
    before = m.n;
    run(&m, 201, 260, 1, 0, 180);    /* stopped: one final beacon after 30 s, then quiet */
    CHECK(m.n == before + 1);
    before = m.n;
    run(&m, 260, 800, 1, 0, 180);    /* parked: every 10 min at most */
    CHECK(m.n == before);
    run(&m, 800, 900, 1, 0, 180);
    CHECK(m.n == before + 1);
}

int main(void)
{
    parse_tests(); feed_tests(); packet_tests(); saved_and_timed_tests(); scheduler_tests();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    puts("beacon ok");
    return 0;
}
