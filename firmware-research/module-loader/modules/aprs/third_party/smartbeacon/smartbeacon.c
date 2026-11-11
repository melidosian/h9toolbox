/*
 * smartbeacon.c - speed/turn driven APRS beacon scheduler.
 *
 * Rules (checked once per second, never closer than guard_s apart):
 *  - parked (< park_kmh10) or no fix: every park_s
 *  - moving: interval = dist_k / speed, clamped to [fast_s, park_s]
 *            walking 5 km/h ~216 s, cycling 15 km/h ~72 s, driving >= 18 km/h 60 s
 *  - corner: |d course| > turn_min + turn_slope / speed and >= turn_time_s
 *            since last beacon (walking: 78 deg, driving 50 km/h: 35 deg)
 *  - stop:   one beacon after stop_s parked (final position)
 *  - fix:    immediate beacon when fix appears or comes back
 */
#include "smartbeacon.h"

const sb_config_t SB_DEFAULTS = {
    .boot_s      = 30,
    .guard_s     = 15,
    .park_s      = 600,
    .fast_s      = 60,
    .park_kmh10  = 20,      /* 2 km/h: below that it's GNSS noise */
    .dist_k      = 10800,   /* s * 0.1 km/h => ~300 m */
    .turn_min    = 30,
    .turn_slope  = 2400,
    .turn_time_s = 15,
    .stop_s      = 30,
    .fix_tmo_s   = 5,
};

void sb_init(sb_state_t *s, const sb_config_t *cfg)
{
    *s = (sb_state_t){0};
    s->cfg = *cfg;
}

void sb_on_rmc(sb_state_t *s, uint32_t now_ms, uint8_t valid,
               uint16_t kmh10, uint16_t course)
{
    s->gnss.fix = valid;
    if (!valid) return;
    s->gnss.fix_ms = now_ms;
    s->gnss.kmh10  = kmh10;
    s->gnss.course = course % 360u;
}

static uint16_t course_delta(uint16_t a, uint16_t b)
{
    uint16_t d = (uint16_t)((a + 360u - b) % 360u);
    return d > 180u ? (uint16_t)(360u - d) : d;
}

int sb_tick(sb_state_t *s, uint32_t now_ms, int tx_busy)
{
    const sb_config_t *c = &s->cfg;
    sb_gnss_t *g = &s->gnss;
    uint32_t since;
    int fire = 0;

    if (g->fix && now_ms - g->fix_ms > c->fix_tmo_s * 1000u) g->fix = 0;
    if (g->fix && !s->fix_prev) s->fix_pend = 1;
    s->fix_prev = g->fix;
    if (now_ms < c->boot_s * 1000u || tx_busy) return 0;

    since = (now_ms - s->last_tx_ms) / 1000u;
    if (since < c->guard_s) return 0;

    if (s->fix_pend) fire = 1;
    if (!g->fix || g->kmh10 < c->park_kmh10) {
        if (g->fix && s->moving) {
            if (s->park_ms == 0) s->park_ms = now_ms;
            if (now_ms - s->park_ms >= c->stop_s * 1000u) { fire = 1; s->moving = 0; }
        }
        if (since >= c->park_s) fire = 1;
    } else {
        uint32_t ivl = c->dist_k / g->kmh10;
        s->moving = 1;
        s->park_ms = 0;
        if (ivl < c->fast_s) ivl = c->fast_s;
        if (ivl > c->park_s) ivl = c->park_s;
        if (since >= ivl) fire = 1;
        if (since >= c->turn_time_s &&
            course_delta(g->course, s->tx_course) > c->turn_min + c->turn_slope / g->kmh10)
            fire = 1;
    }
    if (!fire) return 0;
    s->fix_pend = 0;
    s->tx_course = g->course;
    s->last_tx_ms = now_ms;
    return 1;
}
