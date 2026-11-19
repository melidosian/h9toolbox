/* Host test of the APRS BEACON screen's behaviour: key handling (a keypad glitch must not start or stop it;
 * a hold does), what it transmits with and without a GPS fix, the saved-position fallback, timed mode.
 *   cc -std=c11 -fsanitize=address,undefined -Wno-missing-braces -I. -I../common test_beacon_screen.c \
 *      beacon_core.c third_party/smartbeacon/smartbeacon.c && ASAN_OPTIONS=detect_leaks=0 ./a.out */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APRS_HOST_TEST
#include "h9mod.h"
#include "app_theme.h"

static struct h9_request host_request;
#undef H9_REQUEST
#define H9_REQUEST (&host_request)

static char on_air[16];                                   /* the title's "ARMED" / "OFF" */
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
unsigned gfx_text(unsigned x, unsigned y, const char *t, unsigned max, unsigned s, uint16_t f, uint16_t b)
{ (void)s; (void)f; (void)b; if (x == 112 && y == UI_TITLE_Y) { strncpy(on_air, t, sizeof on_air - 1); } unsigned n = 0; while (n < max && t[n]) n++; return x + n * 6; }
void gfx_wait_release(void) {}
unsigned gfx_wait_key(void) { return 0; }

static uint32_t host_tick;
static jmp_buf done;
static unsigned limit_ticks;
/* A key script: (from, to, key) in ticks since the screen opened. */
struct press { unsigned from, to, key; };
static const struct press *script;
static unsigned open_tick;
unsigned h9_key(void)
{
    unsigned t = host_tick - open_tick;
    for (const struct press *p = script; p && p->key; p++) if (t >= p->from && t < p->to) return p->key;
    return 0;
}
static void on_tick(void);
void h9_delay(unsigned u) { (void)u; host_tick += 1; on_tick(); if (host_tick - open_tick > limit_ticks) longjmp(done, 1); }
void h9_led(unsigned on) { (void)on; }

static uint8_t host_mycall[7] = { 'N', '0', 'C', 'A', 'L', 'L', 7 };
#define MYCALL host_mycall
static uint8_t host_side, host_route, host_rx_ch = 1, host_tx_ch;
#define SIDE_BYTE host_side
#define ROUTE_BYTE host_route
#define RF_SETUP(x) ((void)(x))
static uint8_t host_spk = 1;
#define AF_SELECT(m) ((void)(m))
#define SPEAKER(on) ((void)(on))
#define RF_WRITE(r, v) ((void)(r), (void)(v))
static uint8_t host_asleep;
static unsigned host_busy;
#define BEACON_RADIO_BUSY() (host_busy || host_asleep)
#define BEACON_ACTIVE_SIDE (host_side >> 7)
#define BEACON_TX_SETTING host_tx_ch
#define BEACON_REDRAW_RADIO() ((void)0)
#define RF_ASLEEP host_asleep
#define RF_WAKE() (host_asleep = 0)
#define SPEAKER_STATE host_spk
#define RX_CH host_rx_ch
#define TX_CH host_tx_ch
#define TICK host_tick
static uint32_t host_tx_freq = 14439000;
#define TX_FREQ host_tx_freq
static uint8_t host_ring[2048];
static uint16_t host_write, host_read;
#define RING host_ring
#define RING_WRITE host_write
#define RING_READ host_read

/* The stock KISS transmit: keep what was sent (unescaped AX.25 info text). */
static unsigned tx_count, tx_tick;
static char tx_info[100];
static void host_kiss_tx(const uint8_t *kiss, unsigned len)
{
    tx_count++;
    if (tx_count == 1) tx_tick = host_tick - open_tick;
    unsigned n = 0;
    for (unsigned i = 2; i + 1 < len; i++) {            /* skip C0 00 and the final C0; no escapes occur in these tests */
        if (i >= 2 + 14 + 7 + 2) tx_info[n++] = (char)kiss[i];   /* dest, source, digi (21 bytes), control, PID */
    }
    tx_info[n] = 0;
}
#define STOCK_KISS_TX ((uintptr_t)host_kiss_tx)

/* The chip's saved fix. */
static int have_saved; static int32_t saved_lat, saved_lon;
#include "fix_store.h"
int fix_load_micro(fix_read_fn read, int32_t *lat, int32_t *lon) { (void)read; if (!have_saved) return 0; *lat = saved_lat; *lon = saved_lon; return 1; }
void fix_save(fix_read_fn read, int32_t lat, int32_t lon) { (void)read; (void)lat; (void)lon; }

#include "aprs_core.c"
#include "aprs_tx.c"
struct aprs aprs_host;
static struct screen background_state;
static uint8_t protected_snapshot[384];
#define BEACON_HANDOFF protected_snapshot
static struct resident_entry resident;
#define BEACON_BACKGROUND_STATE (&background_state)
#define BEACON_RESIDENT (&resident)
#undef SPEAKER_ON
#define SPEAKER_ON(on) ((void)(on))
#include "aprs_beacon.c"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
static void fake_read(void *d, uint32_t a, uint32_t n)
{
    struct h9_entry entry = { .size = 1024, .entry = 2, .crc = 0x1234 };
    memset(d, 0, n);
    if (!a && n == H9_BUNDLE_HEADER) ((uint8_t *)d)[10] = 2;
    if (a != H9_BUNDLE_HEADER + H9_BUNDLE_ENTRY) return;
    if (n == sizeof entry) memcpy(d, &entry, n);
    if (n == H9_NAME_BYTES) memcpy(d, ".APRS BG", sizeof ".APRS BG");
}
static unsigned api_version = 3, result;

static void rmc(void)   /* one GPS frame: 12.4 kn at 275 degrees */
{
    const char *s = "$GNRMC,141500.00,A,4152.6800,N,08737.8000,W,12.4,275.5,111126,,,A*00\r\n";
    host_ring[host_write++ & 2047] = 0xC0; host_ring[host_write++ & 2047] = 0x10;
    for (; *s; s++) host_ring[host_write++ & 2047] = (uint8_t)*s;
    host_ring[host_write++ & 2047] = 0xC0;
}
static unsigned feed_every, last_feed;   /* an RMC every feed_every ticks once the screen has run for 20 ticks */
static void on_tick(void)
{
    if (feed_every && host_tick - open_tick >= 20 && host_tick - last_feed >= feed_every) { last_feed = host_tick; rmc(); }
}

static void run_at(const struct press *keys, unsigned ticks, unsigned gps_every, int saved, uint32_t start)
{
    host_tick = start; open_tick = host_tick; script = keys; limit_ticks = ticks;
    feed_every = gps_every; last_feed = 0;
    have_saved = saved; saved_lat = 41878000; saved_lon = -87630000;
    tx_count = 0; tx_info[0] = 0; on_air[0] = 0;
    struct h9_api api = { .version = api_version, .load = 0x29F00, .read = fake_read };
    result = 0;
    if (!setjmp(done)) result = module_main(&api, 0);
}

static void run(const struct press *keys, unsigned ticks, unsigned gps_every, int saved)
{
    memset(protected_snapshot, 0, sizeof protected_snapshot);
    run_at(keys, ticks, gps_every, saved, 100000);
}

#define HOLD_MENU(from) { (from), (from) + 80, H9_KEY_MENU }
int main(void)
{
    aprs_reset();
    strcpy(A->mycall, "N0CALL-7");
    A->switching = 1;
    static const struct press start[] = { HOLD_MENU(100), { 0, 0, 0 } };

    /* 1. A keypad glitch (a few ticks of Menu) does not start it; a hold does, and it stays on. */
    static const struct press glitch[] = { { 100, 102, H9_KEY_MENU }, { 200, 203, H9_KEY_MENU }, { 0, 0, 0 } };
    run(glitch, 3000, 0, 1);
    CHECK(strcmp(on_air, "ARMED") != 0 && tx_count == 0);
    run(start, 1000, 0, 1);
    CHECK(!strcmp(on_air, "ARMED"));

    /* 2. No GPS at all but a saved fix: it beacons the saved position, marked last known, once the guard passes
     *    (15 s after the start), and it does not turn itself off after the transmit. */
    run(start, 2000, 0, 1);
    CHECK(tx_count == 1);
    CHECK(!strcmp(tx_info, "!4152.68N/08737.80W[ H9 Toolbox, last known"));
    CHECK(!strcmp(on_air, "ARMED"));

    /* 3. No GPS and nothing saved: it cannot beacon, stays on, and tries again after a minute. */
    run(start, 2000, 0, 0);
    CHECK(tx_count == 0 && !strcmp(on_air, "ARMED"));
    run(start, 9000, 0, 0);
    CHECK(tx_count == 0 && !strcmp(on_air, "ARMED"));

    /* 4. With a live fix: position, course and speed, no "last known". */
    run(start, 2000, 100, 0);
    CHECK(tx_count == 1 && !strcmp(tx_info, "!4152.68N/08737.80W[275/012 H9 Toolbox"));
    CHECK(!strcmp(on_air, "ARMED"));

    /* 5. A hold stops it again; a short Menu tap changes the selected setting (MODE) and nothing else. */
    static const struct press stop[] = { HOLD_MENU(100), HOLD_MENU(400), { 0, 0, 0 } };
    run(stop, 3000, 100, 0);
    CHECK(strcmp(on_air, "ARMED") != 0 && tx_count == 0);                      /* started at 100, stopped at 400: before the first TX */
    unsigned before = A->beacon;
    static const struct press tap[] = { { 100, 110, H9_KEY_MENU }, { 0, 0, 0 } };
    run(tap, 1000, 0, 0);
    CHECK((A->beacon & 1) != (before & 1) && strcmp(on_air, "ARMED") != 0);
    A->beacon = BEACON_DEFAULT;

    /* 6. Timed mode, every 1 minute: the first position 15 s after the start, then each minute. */
    A->beacon = (uint8_t)(1 | 0 << 3);
    run(start, 8300, 100, 0);
    CHECK(tx_count == 2);
    A->beacon = BEACON_DEFAULT;

    /* 7. Keys are ignored for a moment after a transmit (the keypad glitches then): a Menu tap right after one
     *    changes nothing, while the same tap well after it changes MODE. */
    run(start, 2500, 100, 0);
    unsigned first_tx = tx_tick;
    CHECK(tx_count == 1 && first_tx > 1000);
    struct press right_after[] = { HOLD_MENU(100), { first_tx + 5, first_tx + 15, H9_KEY_MENU }, { 0, 0, 0 } };
    run(right_after, first_tx + 200, 100, 0);
    CHECK(!strcmp(on_air, "ARMED") && (A->beacon & 1) == 0);
    struct press later[] = { HOLD_MENU(100), { first_tx + 100, first_tx + 110, H9_KEY_MENU }, { 0, 0, 0 } };
    run(later, first_tx + 300, 100, 0);
    CHECK(!strcmp(on_air, "ARMED") && (A->beacon & 1) == 1);
    A->beacon = BEACON_DEFAULT;

    /* 8. Back leaves (after a short hold, not on a one-tick glitch). */
    static const struct press back_glitch[] = { { 100, 101, H9_KEY_BACK }, { 0, 0, 0 } };
    host_request.magic = 0;
    run(back_glitch, 600, 0, 0);
    CHECK(host_request.magic == 0);                                              /* still running when the script ended */

    /* 9. Keep the scheduler after returning to stock, and let stock read the same NMEA. */
    static const struct press background[] = { HOLD_MENU(100), { 300, 320, H9_KEY_BACK }, { 0, 0, 0 } };
    api_version = 2;
    run(background, 600, 100, 0);
    CHECK(!result && !resident.magic);                                          /* loader API 1 cannot return resident */
    api_version = 3;
    run(background, 600, 100, 0);
    CHECK(!result && A->switching == BEACON_HANDOFF_READY && host_request.magic == H9_REQUEST_MAGIC);
    struct h9_api api = { .version = 3, .load = 0x29F00, .read = fake_read };
    CHECK(resident_main(&api, 1) == H9_RETURN_RADIO && resident.magic == BEACON_RESIDENT_MAGIC);
    CHECK(resident.entry == 0x29F02 && resident.lencrc == 0x04001234 && !resident.flags);
    CHECK(!tx_count);
    host_tick = open_tick + 1800;
    rmc();
    uint16_t stock_read = host_read;
    host_busy = 1;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(!tx_count && host_read == stock_read);
    CHECK(background_state.pending && background_state.waiting == BW_BUSY);
    host_busy = 0;
    host_tx_freq++;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(!tx_count);
    CHECK(background_state.pending && background_state.waiting == BW_FREQ);
    host_tx_freq--;
    host_side = 0x80;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(!tx_count);
    CHECK(background_state.pending && background_state.waiting == BW_SIDE);
    host_side = 0;
    host_tx_ch = 1;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(!tx_count);
    CHECK(background_state.pending && background_state.waiting == BW_SETTING);
    host_tx_ch = 0;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(tx_count == 1 && host_read == stock_read);
    CHECK(!background_state.pending && background_state.waiting == BW_INTERVAL);
    CHECK(!strcmp(tx_info, "!4152.68N/08737.80W[275/012 H9 Toolbox"));
    /* A due event that cannot construct a packet survives the retry guard.
     * The next tick must not call sb_tick again and discard the fix event. */
    host_tick += 6000;
    rmc();
    char saved_call[CALL]; strcpy(saved_call, A->mycall); A->mycall[0] = 0;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(tx_count == 1 && background_state.pending && background_state.waiting == BW_NO_CALL);
    strcpy(A->mycall, saved_call);
    host_tick += 50;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(tx_count == 1 && background_state.pending && background_state.waiting == BW_RETRY);
    host_tick += 6000;
    rmc();
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(tx_count == 2 && !background_state.pending);
    resident.magic = 0;                                                        /* the loader pauses before opening the menu */
    host_tick += 500;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(tx_count == 2);
    run_at(NULL, 600, 100, 0, host_tick);
    CHECK(!strcmp(on_air, "ARMED") && !tx_count);                              /* no duplicate first beacon */


    run(background, 600, 100, 0);
    CHECK(resident_main(&api, 1) == H9_RETURN_RADIO);
    resident.magic = 0;
    struct screen saved_state;
    CHECK(snapshot(&saved_state, 1));
    unsigned saved_sent = saved_state.sent;
    memset(A, 0xAD, sizeof *A);                                                          /* another screen reused the scratch */
    run_at(NULL, 200, 100, 0, host_tick);
    CHECK(!strcmp(on_air, "ARMED") && !tx_count);
    CHECK(A->beacon == BEACON_DEFAULT);
    CHECK(snapshot(&saved_state, 1) && saved_state.sent == saved_sent);

    run(background, 600, 100, 0);
    CHECK(resident_main(&api, 1) == H9_RETURN_RADIO);
    host_tick = 100;
    resident_main(NULL, H9_RETURN_RADIO);
    CHECK(!resident.magic && !tx_count);
    /* Foreground restore must not keep a stale fix while no new NMEA arrives.
     * The freshness block is unconditional in aprs_beacon.c, including the
     * production foreground compilation (no APRS_BEACON_RESIDENT define). */
    struct screen stale = {0};
    stale.t0 = host_tick;
    stale.g.fix = 1;
    strcpy(stale.g.lat, "4152.68N");
    stale.cursor = host_write & RING_MASK;
    CHECK(!read_gps(&stale) && stale.g.fix);
    host_tick += 1001;
    CHECK(read_gps(&stale) && !stale.g.fix);
    CHECK(stale.g.lat[0]); /* retain last-known position, not live-fix status */
    CHECK(!read_gps(&stale));
    rmc();
    CHECK(read_gps(&stale) && stale.g.fix);
    /* A refused position stays in its retry guard across the 32-bit ms wrap.
     * The pending event must become due once 60 seconds have elapsed. */
    struct screen rollover = {0};
    host_tick = UINT32_MAX / 10u;
    rollover.armed = 1;
    rollover.pending = BP_DUE | BP_RETRY;
    rollover.retry_started_ms = now_ms(&rollover);
    host_tick += 3000;
    CHECK(!due(&rollover) && rollover.waiting == BW_RETRY && rollover.pending);
    host_tick += 3000;
    CHECK(due(&rollover) && rollover.pending == BP_DUE);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    puts("beacon screen ok");
    return 0;
}
