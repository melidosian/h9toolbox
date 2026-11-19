/* Permanent passive scheduler on the pinned stock 1.0.38 application. */
#include "runtime.h"
#include "modem_input.h"
#include <stddef.h>

struct resident_state {
    struct h9_runtime_prefix prefix;
    struct h9_bs beacon;
    struct h9_modem_cursor modem;
    struct ts_settings requested;
    struct h9_runtime_status live;
    volatile uint32_t request_version, status_version;
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

static const struct h9_runtime_api api = { H9_RUNTIME_ABI, read_status, publish_settings };

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
        }
        unsigned online = UART1 && DMA_STATE[0] == 0x10CFCu && DMA_STATE[1] == H9_MODEM_DMA_CAPACITY;
        h9_modem_poll(&s->modem, &s->beacon, online ? (const volatile uint8_t *)0x10CFCu : NULL,
                      DMA_STATE + 2, now);
        h9_bs_poll(&s->beacon, now);
        h9_bs_status(&s->beacon, &s->live.beacon);
        s->live.uart_online = (uint8_t)online;
        ++s->live.polls;
        FENCE();
        ++s->status_version;
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
