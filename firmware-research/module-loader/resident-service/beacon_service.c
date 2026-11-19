#include "beacon_service.h"
#include <stddef.h>

enum { GPS_STALE_MS = 10000, RETRY_MS = 60000 };
static int valid(const struct h9_bs *s)
{
    return s && s->status.magic == H9_BS_MAGIC &&
           s->status.abi == H9_BS_ABI && s->status.size == sizeof s->status;
}
void h9_bs_init(struct h9_bs *s)
{
    if (!s) return;
    /* Keep the target freestanding: no implicit memset dependency. */
    volatile uint8_t *p = (volatile uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; ++i) p[i] = 0;
    s->status.magic = H9_BS_MAGIC;
    s->status.abi = H9_BS_ABI;
    s->status.size = sizeof s->status;
}
int h9_bs_arm(struct h9_bs *s, uint8_t timed, uint8_t minutes,
              uint8_t symbol, uint32_t now_ms)
{
    if (!valid(s) || timed > 1 || !minutes || minutes > 30 || symbol > 2 ||
        s->status.generation == UINT32_MAX) return 0;
    struct h9_bs_status *st = &s->status;
    ++st->generation;
    st->armed = 1;
    st->timed = timed;
    st->minutes = minutes;
    st->symbol = symbol;
    st->pending = 0;
    st->pending_id = 0;
    st->submitted = 0;
    st->last_submit_ms = now_ms;
    st->wait = H9_BS_INTERVAL;
    s->retry_active = 0;
    if (st->gps.fix && now_ms - s->gps_ms >= GPS_STALE_MS)
        st->gps.fix = 0;
    volatile uint8_t *p = (volatile uint8_t *)&s->smart;
    for (unsigned i = 0; i < sizeof s->smart; ++i) p[i] = 0;
    s->smart.cfg.guard_s = 15;
    s->smart.cfg.park_s = 600;
    s->smart.cfg.fast_s = 60;
    s->smart.cfg.park_kmh10 = 20;
    s->smart.cfg.dist_k = 10800;
    s->smart.cfg.turn_min = 30;
    s->smart.cfg.turn_slope = 2400;
    s->smart.cfg.turn_time_s = 15;
    s->smart.cfg.stop_s = 30;
    s->smart.cfg.fix_tmo_s = 5;
    s->smart.last_tx_ms = now_ms;
    s->smart.fix_pend = 1;
    if (st->gps.fix) sb_on_rmc(&s->smart, now_ms, 1,
                               st->gps.kmh10, st->gps.course);
    return 1;
}
void h9_bs_stop(struct h9_bs *s)
{
    if (!valid(s)) return;
    s->status.armed = 0;
    s->status.pending = 0;
    s->status.wait = H9_BS_OFF;
    s->retry_active = 0;
}
int h9_bs_feed_byte(struct h9_bs *s, uint8_t byte, uint32_t now_ms)
{
    if (!valid(s)) return 0;
    if (!beacon_feed(&s->feed, byte, &s->status.gps)) return 0;
    s->gps_ms = now_ms;
    if (s->status.armed && !s->status.timed)
        sb_on_rmc(&s->smart, now_ms, s->status.gps.fix,
                  s->status.gps.kmh10, s->status.gps.course);
    return 1;
}
void h9_bs_input_gap(struct h9_bs *s, uint32_t now_ms)
{
    s->feed.started = s->feed.len = s->feed.esc = 0;
    if (!s->status.gps.fix) return;
    s->status.gps.fix = 0;
    if (s->status.armed && !s->status.timed)
        sb_on_rmc(&s->smart, now_ms, 0, 0, s->status.gps.course);
}
void h9_bs_poll(struct h9_bs *s, uint32_t now_ms)
{
    if (!valid(s)) return;
    struct h9_bs_status *st = &s->status;
    if (st->gps.fix && now_ms - s->gps_ms >= GPS_STALE_MS) h9_bs_input_gap(s, now_ms);
    if (!st->armed) return;
    if (s->retry_active) {
        if (now_ms - s->retry_ms < RETRY_MS) {
            st->wait = H9_BS_RETRY;
            return;
        }
        s->retry_active = 0;
    }
    if (!st->pending) {
        int due = st->timed ? beacon_timed_due(now_ms, st->last_submit_ms,
                                               st->submitted, st->minutes) :
                              sb_tick(&s->smart, now_ms, 0);
        if (due) {
            st->pending = 1;
            if (++st->pending_id == 0) ++st->pending_id;
        }
    }
    if (!st->pending) st->wait = H9_BS_INTERVAL;
    else if (!st->gps.fix || !st->gps.lat[0]) st->wait = H9_BS_POSITION;
    else if (st->wait < H9_BS_BUSY) st->wait = H9_BS_READY;
}
int h9_bs_submitted(struct h9_bs *s, uint32_t generation, uint32_t pending_id,
                    int accepted, uint32_t now_ms, uint16_t sent_course)
{
    if (!valid(s) || !s->status.armed || !s->status.pending ||
        generation != s->status.generation || !pending_id ||
        pending_id != s->status.pending_id) return 0;
    if (s->retry_active) return 0;
    if (!accepted) {
        s->retry_ms = now_ms;
        s->retry_active = 1;
        s->status.wait = H9_BS_RETRY;
        return 1;
    }
    s->retry_active = 0;
    s->status.pending = 0;
    s->status.wait = H9_BS_INTERVAL;
    ++s->status.submitted;
    s->status.last_submit_ms = now_ms;
    s->smart.last_tx_ms = now_ms;
    s->smart.tx_course = sent_course;
    return 1;
}
int h9_bs_wait_for(struct h9_bs *s, uint32_t generation, uint32_t pending_id,
                   enum h9_bs_wait why)
{
    if (!valid(s) || !s->status.armed || !s->status.pending ||
        generation != s->status.generation || !pending_id ||
        pending_id != s->status.pending_id || why < H9_BS_POSITION ||
        why > H9_BS_TX_FAULT) return 0;
    s->status.wait = (uint8_t)why;
    return 1;
}
int h9_bs_status(const struct h9_bs *s, struct h9_bs_status *out)
{
    if (!valid(s) || !out) return 0;
    const uint8_t *src = (const uint8_t *)&s->status;
    uint8_t *dst = (uint8_t *)out;
    for (unsigned i = 0; i < sizeof *out; ++i) dst[i] = src[i];
    return 1;
}
