/* Permanent scheduler on the pinned stock 1.0.38 application. */
#include "runtime.h"
#include "modem_input.h"
#include <stddef.h>

#ifdef H9_RESIDENT_TX
#include "beacon_tx.h"
struct tx_feedback {
    uint32_t generation, pending_id, completed_ms;
    uint16_t course;
    uint8_t result;
    volatile uint8_t ready;
};
_Static_assert(offsetof(struct h9_runtime_api, normal_tx) == 12 &&
               offsetof(struct h9_runtime_api, prepare_tx) == 16 &&
               offsetof(struct h9_runtime_api, emit_frame) == 20 &&
               offsetof(struct h9_runtime_api, airtime) == 24 &&
               offsetof(struct h9_runtime_api, stock_tx) == 28, "Main-flash TX hook ABI");
#endif

struct resident_state {
    struct h9_runtime_prefix prefix;
    struct h9_bs beacon;
    struct h9_modem_cursor modem;
    struct ts_settings requested;
    struct h9_runtime_status live;
    volatile uint32_t request_version, status_version;
#ifdef H9_RESIDENT_TX
    volatile uint32_t applied_version;
    uint32_t last_probe_ms;
    struct h9_beacon_tx tx;
    struct tx_feedback feedback;
#endif
};
_Static_assert(sizeof(struct resident_state) <= H9_RUNTIME_BYTES, "Resident APRS exceeds its RAM reservation");
_Static_assert(offsetof(struct resident_state, prefix.mute) == 4, "Mute hook offset");
_Static_assert(offsetof(struct resident_state, prefix.inspected) == 5, "Startup hook offset");

#define STATE ((struct resident_state *)(uintptr_t)H9_RUNTIME_RAM)
#define NOR_READY (*(const volatile uint16_t *)0x12234u)
#define NOW_MS (*(const volatile uint32_t *)0x1D8B0u * 10u)
#define UART1 (*(const volatile uint32_t *)0x11CFCu == 0x1E32Cu)
#define DMA_STATE ((const volatile uint32_t *)0x1E350u)
#define FENCE() __asm__ volatile("" ::: "memory")

static unsigned copy_snapshot(void *out, const volatile void *src, unsigned size,
                              const volatile uint32_t *seq)
{
    uint32_t before = *seq;
    if (before & 1u) return 0;
    FENCE();
    uint8_t *dst = out;
    const volatile uint8_t *p = src;
    for (unsigned i = 0; i < size; ++i) dst[i] = p[i];
    FENCE();
    return before == *seq;
}

static unsigned read_status(struct h9_runtime_status *out)
{
    return copy_snapshot(out, &STATE->live, sizeof *out, &STATE->status_version);
}

static void publish_settings(const struct ts_settings *settings)
{
    ++STATE->request_version;
    FENCE();
    STATE->requested = *settings;
    STATE->prefix.mute = settings->mute;
    FENCE();
    ++STATE->request_version;
    if (!STATE->live.worker_ready) {
        STATE->live.settings = *settings;
        STATE->live.store_result = TSP_OK;
    }
}

#ifdef H9_RESIDENT_TX
/* The worker is the sole writer of scheduler state. my_app submits at most
 * one attempt while the worker hasn't consumed and published its result. */
static void normal_tx(void)
{
    struct resident_state *s = STATE;
    struct h9_runtime_status status;
    /* Let the next awake window through even if the battery saver and
     * route probe interval happen to repeat at the same cadence. */
    if (*(const volatile uint8_t *)H9_RF_SLEEP_FLAG || NOW_MS - s->last_probe_ms < 2000u ||
        s->feedback.ready || s->applied_version != s->request_version ||
        !read_status(&status) || !status.worker_ready || !status.uart_online ||
        status.store_result != TSP_OK || !status.settings.armed ||
        !status.beacon.pending || !status.beacon.gps.fix ||
        status.beacon.wait == H9_BS_RETRY)
        return;
    unsigned result = h9_beacon_tx_try(&s->tx, &status);
    /* In particular, avoid borrowing the other VFO on every loop while it
     * keeps reporting a carrier. The pending event can be retried later. */
    s->last_probe_ms = NOW_MS;
    if (result == status.beacon.wait && result != H9_BS_TX_ERROR) return;
    s->feedback.generation = status.beacon.generation;
    s->feedback.pending_id = status.beacon.pending_id;
    s->feedback.completed_ms = s->last_probe_ms;
    s->feedback.course = status.beacon.gps.course;
    s->feedback.result = (uint8_t)result;
    FENCE();
    s->feedback.ready = 1;
}

static unsigned prepare_tx(const void *frequency) { return h9_beacon_tx_prepare(&STATE->tx, frequency); }
static unsigned emit_frame(void) { return h9_beacon_tx_emit(&STATE->tx); }
static unsigned airtime(void) { return h9_beacon_tx_airtime(&STATE->tx); }
static void stock_tx(const void *arg, unsigned size, h9_stock_action stock)
{
    h9_beacon_tx_stock(&STATE->tx, arg, size, stock);
}
#endif

static const struct h9_runtime_api api = {
    H9_RUNTIME_ABI, read_status, publish_settings,
#ifdef H9_RESIDENT_TX
    normal_tx, prepare_tx, emit_frame, airtime, stock_tx,
#endif
};

static void worker(void *context)
{
    (void)context;
    struct resident_state *s = STATE;
    void (*delay)(unsigned) = (void (*)(unsigned))0x02001F82u; /* 100 Hz task delay */
    uint32_t applied = 0;
    for (;;) {
        uint32_t now = NOW_MS;
        uint32_t requested = s->request_version;
        struct ts_settings next;
        ++s->status_version;
        FENCE();
        if (applied != requested &&
            copy_snapshot(&next, &s->requested, sizeof next, &s->request_version)) {
            struct ts_settings *prior = &s->live.settings;
            if (!next.armed) h9_bs_stop(&s->beacon);
            else if (!prior->armed || next.timed != prior->timed ||
                     next.minutes != prior->minutes || next.symbol != prior->symbol ||
                     next.tx_side != prior->tx_side || next.tx_frequency != prior->tx_frequency ||
                     next.tx_setting != prior->tx_setting)
                h9_bs_arm(&s->beacon, next.timed, next.minutes, next.symbol, now);
            *prior = next;
            s->live.store_result = TSP_OK;
            applied = requested;
#ifdef H9_RESIDENT_TX
            s->applied_version = applied;
#endif
        }
#ifdef H9_RESIDENT_TX
        unsigned acknowledged = s->feedback.ready;
        if (acknowledged) {
            FENCE();
            const struct tx_feedback *f = &s->feedback;
            if (f->result == H9_TX_SUBMITTED || f->result == H9_BS_TX_ERROR)
                h9_bs_submitted(&s->beacon, f->generation, f->pending_id,
                                f->result == H9_TX_SUBMITTED, f->completed_ms, f->course);
            else
                h9_bs_wait_for(&s->beacon, f->generation, f->pending_id, f->result);
        }
        now = NOW_MS;
#endif
        unsigned online = UART1 && DMA_STATE[0] == 0x10CFCu && DMA_STATE[1] == H9_MODEM_DMA_CAPACITY;
        h9_modem_poll(&s->modem, &s->beacon, online ? (const volatile uint8_t *)0x10CFCu : NULL,
                      DMA_STATE + 2, now);
        h9_bs_poll(&s->beacon, now);
        h9_bs_status(&s->beacon, &s->live.beacon);
        s->live.uart_online = (uint8_t)online;
        ++s->live.polls;
        FENCE();
        ++s->status_version;
#ifdef H9_RESIDENT_TX
        if (acknowledged) {
            FENCE();
            s->feedback.ready = 0;
        }
#endif
        delay(2);
    }
}

static int read_nor(void *ctx, uint32_t address, void *dst, unsigned size)
{
    (void)ctx;
    if (NOR_READY != 0x5AA5u) return 0;
    ((void (*)(void *, uint32_t, unsigned))0x01E4FF22u)(dst, address, size);
    return 1;
}

/* Called after the stock my_app initialization and after the app-side flash
 * verifier checked the far body. Arm restoration only uses a healthy pair. */
const struct h9_runtime_api *h9_runtime_start(void)
{
    struct resident_state *s = STATE;
    struct tsp_io io = { .read = read_nor, .sector = { 0x3FC000u, 0x3FD000u } };
    h9_bs_init(&s->beacon);
    enum tsp_result result = tsp_load(&io, &s->live.settings);
    s->live.store_result = result;
    s->prefix.mute = s->live.settings.mute;
    if (result != TSP_OK) s->live.settings.armed = 0;
    const struct ts_settings *settings = &s->live.settings;
    if (settings->armed)
        h9_bs_arm(&s->beacon, settings->timed, settings->minutes, settings->symbol, NOW_MS);
    h9_bs_status(&s->beacon, &s->live.beacon);
    typedef int (*create_task)(void (*)(void *), void *, unsigned, unsigned, unsigned, const char *);
    s->live.worker_ready = ((create_task)0x02003102u)(worker, NULL, 6, 0x400, 0, "h9_beacon") == 0;
    return &api;
}
