/* APRS BEACON (module "APRS BEACON"): beacons your position from your callsign.
 *
 * MODE SMART: Tomasz Kuehn's smartbeacon scheduler (MIT, third_party/smartbeacon/).
 * Moving, a position goes out about every 300 m of travel (60 s at 18 km/h and
 * up, 10 min parked); a corner sends at once; one more goes out after 30 s
 * stopped, and one when a fix appears. MODE TIMED: a fixed interval (1-30 min).
 * It is fed once a second with the GPS speed and course from the APRS modem's
 * NMEA sentences (the modem ring, as the other APRS screens read them).
 * Without a live fix it beacons the last position it knows: this session's last
 * fix, else the fix the loader saved on the chip (fix_store.h); the packet then
 * says "last known" and carries no course or speed.
 *
 * Packets go out like APRS SEND's: through stock's own KISS transmit routine
 * 0x01E5DBCC (it keys, sends, waits the airtime and unkeys with stock's checks),
 * on the APRS TX CH side, only on the 2 m band, with your callsign.
 * Back leaves an armed beacon running on the radio screen. Opening
 * Toolbox pauses it. Reopen BEACON to resume, or hold Menu there to stop it.
 * Keys: Up/Down choose MODE, EVERY or SYMBOL, Menu changes it; HOLD Menu (0.6 s)
 * starts or stops beaconing (a keypad glitch is far shorter, so it cannot start
 * or stop it); 5 sends one now; Back leaves. The settings are kept
 * for the session in the ABI 3 protected snapshot.
 * Unattended transmitting needs an amateur licence: you are responsible. */
#include "aprs_core.h"
#include "aprs_tx.h"
#include "beacon_core.h"
#include "fix_store.h"
#include "third_party/smartbeacon/smartbeacon.h"
#include <stddef.h>

enum { BTAP_TICKS = 3, BHOLD_TICKS = 60, QUIET_TICKS = 50, ROW = 11, TOP = BODY_Y, ROWS = 3, GUARD_MS = 60000 };
enum { BKEY_WAIT = 0xFE, GPS_STALE_MS = 10000 };
enum { BW_NONE, BW_INTERVAL, BW_SIDE, BW_KEY, BW_FREQ, BW_SETTING, BW_BUSY, BW_RETRY, BW_NO_POSITION, BW_NO_CALL, BW_TX_INVALID };
enum { BP_DUE = 1, BP_RETRY = 2 };

/* The existing loader dispatches the resident entry from the main radio loop
 * and clears its magic before loading another Toolbox module. */
struct resident_entry {
    uint32_t magic, entry, lencrc, tick, reserved[3], flags;
};
#ifndef BEACON_RESIDENT
#define BEACON_RESIDENT ((volatile struct resident_entry *)0x2B8B4u)
#endif
#ifndef BEACON_RADIO_BUSY
/* The stock periodic beacon (app+0x08747C) uses these mode and activity
 * flags before it starts. Also exclude a sleeping or already keyed RF chip. */
#define BEACON_RADIO_BUSY() ((*(const volatile uint8_t *)0x11B01u) || \
    (*(const volatile uint8_t *)0x11AB0u) > 1 || (*(const volatile uint8_t *)0x11AC6u) != 3 || \
    (*(const volatile uint8_t *)0x11AFBu) || (*(const volatile uint8_t *)0x11AFEu) || \
    (*(const volatile uint8_t *)0x11D28u) || \
    (((unsigned (*)(unsigned))0x01E505FEu)(0x30) & 0x4000u))
#endif
#ifndef BEACON_ACTIVE_SIDE
#define BEACON_ACTIVE_SIDE (*(const volatile uint8_t *)0x11B50u >> 7)
#endif
#ifndef BEACON_TX_SETTING
#define BEACON_TX_SETTING (*(const volatile uint8_t *)0xC533u)
#endif
#ifndef BEACON_REDRAW_RADIO
#define BEACON_REDRAW_RADIO() ((void (*)(unsigned))0x01E53306u)(0)
#endif
enum { BEACON_RESIDENT_MAGIC = 0x54525343, BEACON_STATE_BYTES = 0x180,
       BEACON_HANDOFF_READY = 2 };

static const char SYMBOL[3] = { '[', 'b', '>' };
static const char *const SYMBOL_NAME[3] = { "PERSON", "BIKE", "CAR" };
static const uint8_t EVERY[6] = { 1, 2, 5, 10, 15, 30 };       /* minutes, TIMED mode */
static const char COMMENT[] = "H9 Toolbox";

#define B_TIMED (A->beacon & 1)
#define B_SYMBOL ((A->beacon >> 1) & 3)
#define B_EVERY ((A->beacon >> 3) & 7)

struct screen {
    struct beacon_gps g;
    sb_state_t sb;
    uint32_t t0, last_ms, retry_started_ms, gps_ms, background_freq;
    unsigned sent;
    uint16_t cursor;
    uint8_t armed, background_side, background_setting, pending, waiting;
    struct beacon_feed feed;
    uint8_t sel;
};

#include "beacon_state.h"
#define BEACON_SNAPSHOT_BYTES offsetof(struct screen, feed)
_Static_assert(sizeof(struct screen) <= BEACON_STATE_BYTES, "resident beacon state exceeds reserved RAM");
_Static_assert(BEACON_SNAPSHOT_BYTES + 3 <= BEACON_STATE_BYTES, "beacon snapshot exceeds reserved RAM");

/* ABI 3 reserves this memory below the swappable module slot. */
static int snapshot(struct screen *s, int restore)
{
    uint8_t *state = (uint8_t *)s;
    uint16_t check = 0xA15A;
    for (unsigned i = 0; i < BEACON_SNAPSHOT_BYTES; i++) {
        if (restore) state[i] = BEACON_HANDOFF[i];
        else BEACON_HANDOFF[i] = state[i];
        check = (uint16_t)((check << 1 | check >> 15) ^ state[i]);
    }
    volatile uint8_t *end = BEACON_HANDOFF + BEACON_SNAPSHOT_BYTES;
    if (!restore) end[2] = A->beacon;
    check ^= end[2];
    if (restore) {
        if (check != (end[0] | end[1] << 8) || s->armed > 1 || TICK < s->t0) return 0;
        A->beacon = end[2];
        return 1;
    }
    end[0] = (uint8_t)check;
    end[1] = (uint8_t)(check >> 8);
    return 1;
}

static uint32_t now_ms(const struct screen *s) { return (TICK - s->t0) * 10u; }

/* Send one position report now. Returns why not, or 0. */
static unsigned transmit(struct screen *s, int background)
{
#ifndef APRS_HOST_TEST
#ifdef APRS_BEACON_RESIDENT
    background = 1;
#else
    background = 0;
#endif
#endif
    char info[BEACON_INFO];
    uint8_t kiss[APRS_TX_KISS_MAX];
    if (!A->mycall[0]) return BW_NO_CALL;
    if (!s->g.lat[0]) return BW_NO_POSITION;
    struct beacon_gps g = s->g;
    if (!g.fix) g.knots10 = 0;                          /* no course or speed without a fix */
    beacon_info(info, &g, SYMBOL[B_SYMBOL], g.fix ? COMMENT : "H9 Toolbox, last known");
    if (!background) aprs_side(aprs_tx_side());         /* the TX check reads this side */
    unsigned why = 0;
    unsigned len = 0;
    if (TX_FREQ < TX_LOW || TX_FREQ > TX_HIGH) why = BW_TX_INVALID;
    else if (!(len = aprs_tx_kiss(kiss, A->mycall, APRS_TX_DEST, APRS_TX_PATH, info))) why = BW_NO_CALL;
    else {
        ((void (*)(const uint8_t *, unsigned))STOCK_KISS_TX)(kiss, len);
        if (!background) aprs_quiet();                 /* stock's end of TX turns the speaker on */
    }
    if (!background) aprs_side(aprs_rx_side());
    if (why) return why;
    s->pending = 0;
    s->sent++;
    s->last_ms = now_ms(s);
    s->sb.last_tx_ms = s->last_ms;
    s->sb.tx_course = s->g.course;
    return 0;
}

#ifndef APRS_BEACON_RESIDENT
static void note(struct screen *s, unsigned why)
{
    s->waiting = (uint8_t)(why ? why : BW_INTERVAL);
}

/* The smart scheduler starts afresh (a new start, or back to SMART). */
static void start_scheduler(struct screen *s)
{
    sb_config_t cfg = SB_DEFAULTS;
    cfg.boot_s = 0;                                     /* you just started it */
    sb_init(&s->sb, &cfg);
    s->sb.last_tx_ms = now_ms(s);                       /* the 15 s guard counts from now */
    s->sb.fix_pend = 1;                                 /* a first position as soon as the guard allows */
    if (s->g.fix) sb_on_rmc(&s->sb, now_ms(s), 1, s->g.kmh10, s->g.course);
}

static void set_armed(struct screen *s, unsigned on)
{
    s->armed = (uint8_t)on;
    s->pending = 0;
    s->waiting = on ? BW_INTERVAL : BW_NONE;
    if (!on) return;
    s->sent = 0;
    s->last_ms = now_ms(s);
    start_scheduler(s);
}

static void draw_setting(unsigned i, const struct screen *s)
{
    char b[40];
    unsigned n, y = TOP + 2 * ROW + 4 + i * 12;
    const char *label = i == 0 ? "MODE   " : i == 1 ? "EVERY  " : "SYMBOL ";
    n = put_str(b, label);
    if (i == 0) put_str(b + n, B_TIMED ? "TIMED" : "SMART");
    else if (i == 1) { if (B_TIMED) { n += put_uint(b + n, EVERY[B_EVERY]); put_str(b + n, " MIN"); } else put_str(b + n, "AUTO"); }
    else put_str(b + n, SYMBOL_NAME[B_SYMBOL]);
    int sel = i == s->sel;
    aprs_line(y + 2, b, sel ? UI_BG : UI_TEXT, sel ? UI_ACCENT : UI_BG);
}

static void draw(const struct screen *s)
{
    char b[40];
    unsigned n, y = TOP;
    uint32_t frequency = TX_FREQ;
    ui_title("BEACON");
    gfx_label(112, UI_TITLE_Y, s->armed ? "ARMED" : "OFF", 48, 1, s->armed ? UI_OK : UI_TEXT, UI_BG);
    gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, UI_BG);
    n = put_str(b, A->mycall[0] ? A->mycall : "NO CALL");
    b[n++] = ' ';
    n += put_uint(b + n, frequency / 100000u);
    b[n++] = '.';
    for (unsigned digit = 10000; digit >= 10; digit /= 10) b[n++] = (char)('0' + frequency / digit % 10);
    put_str(b + n, " MHZ");
    aprs_line(y, b, !A->mycall[0] ? UI_DANGER : frequency >= TX_LOW && frequency <= TX_HIGH ? UI_TEXT : UI_ACCENT, UI_BG); y += ROW;
    if (s->g.fix) {
        n = put_str(b, "GPS "); n += put_uint(b + n, (s->g.kmh10 + 5u) / 10u);
        n += put_str(b + n, " KM/H  COURSE "); put_uint(b + n, s->g.course);
        aprs_line(y, b, UI_OK, UI_BG);
    } else if (!s->g.lat[0]) {
        aprs_line(y, "NO POSITION", UI_DANGER, UI_BG);
    } else {
        aprs_line(y, "LAST POSITION", UI_ACCENT, UI_BG);
    }
    for (unsigned i = 0; i < ROWS; i++) draw_setting(i, s);
    y = TOP + 2 * ROW + 4 + ROWS * 12 + 3;
    aprs_line(y, s->armed ? "HOLD MENU: STOP" : "HOLD MENU: START", UI_DIM, UI_BG); y += ROW;
    n = put_str(b, "SENT "); n += put_uint(b + n, s->sent);
    if (s->sent) { n += put_str(b + n, "  LAST "); n += put_uint(b + n, (now_ms(s) - s->last_ms) / 1000u); put_str(b + n, "S"); }
    aprs_line(y, b, UI_TEXT, UI_BG); y += ROW;
    static const char waiting[][6] = { "", "TIME", "SIDE", "KEY", "FREQ", "CFG", "BUSY", "RETRY", "POS", "CALL", "TX" };
    if (s->waiting < sizeof waiting / sizeof waiting[0])
        aprs_line(y, waiting[s->waiting], UI_ACCENT, UI_BG);
    aprs_line(FOOT_Y, s->armed ? "5 NOW BACK: BG" : "MENU SET 5 NOW", UI_DIM, UI_BG);
}
#endif

/* One second of scheduling: returns 1 if a position is due. */
static int due(struct screen *s)
{
    if (!s->armed) return 0;
    if (s->pending & BP_RETRY) {
        if (now_ms(s) - s->retry_started_ms < GUARD_MS) {
            s->waiting = BW_RETRY;
            return 0;
        }
        s->pending = BP_DUE;
    }
    if (s->pending) return 1;
    s->pending = (uint8_t)(B_TIMED ? beacon_timed_due(now_ms(s), s->last_ms, s->sent, EVERY[B_EVERY]) : sb_tick(&s->sb, now_ms(s), 0));
    if (!s->pending) s->waiting = BW_INTERVAL;
    return s->pending;
}

static int read_gps(struct screen *s)
{
    unsigned write = RING_WRITE & RING_MASK;
    int changed = 0;
    while (s->cursor != write) {
        if (beacon_feed(&s->feed, RING[s->cursor], &s->g)) {
            s->gps_ms = now_ms(s);
            if (s->armed && !B_TIMED) sb_on_rmc(&s->sb, s->gps_ms, s->g.fix, s->g.kmh10, s->g.course);
            changed = 1;
        }
        s->cursor = (s->cursor + 1) & RING_MASK;
    }
    if (s->g.fix && now_ms(s) - s->gps_ms >= GPS_STALE_MS) {
        s->g.fix = 0;
        if (s->armed && !B_TIMED) sb_on_rmc(&s->sb, now_ms(s), 0, 0, s->g.course);
        changed = 1;
    }
    return changed;
}

#if defined(APRS_BEACON_RESIDENT) || defined(APRS_HOST_TEST)
static void background_tick(void)
{
    if (BEACON_RESIDENT->magic != BEACON_RESIDENT_MAGIC) return;
    struct screen *s = BEACON_BACKGROUND_STATE;
    /* The stock timer starts over on reboot, even if the slot survives. */
    if (TICK < s->t0) { BEACON_RESIDENT->magic = 0; BEACON_HANDOFF[0] ^= 1; return; }
    read_gps(s);                                      /* stock retains its own read cursor */
    /* Latch the scheduler's decision before checking transient radio guards.
     * sb_tick consumes corner/fix/stop events; a blocked or refused TX must
     * remain pending rather than silently dropping that event. */
    if (due(s)) {
        if (BEACON_ACTIVE_SIDE != s->background_side) s->waiting = BW_SIDE;
        else if (h9_key() != H9_KEY_NONE) s->waiting = BW_KEY;
        else if (TX_FREQ != s->background_freq) s->waiting = BW_FREQ;
        else if (BEACON_TX_SETTING != s->background_setting) s->waiting = BW_SETTING;
        else if (BEACON_RADIO_BUSY()) s->waiting = BW_BUSY;
        else {
            unsigned why = transmit(s, 1);
            if (why) {
                s->waiting = (uint8_t)why;
                s->retry_started_ms = now_ms(s);
                s->pending |= BP_RETRY;
            } else s->waiting = BW_INTERVAL;
        }
    }
    snapshot(s, 0);
}

static unsigned resident_main(const struct h9_api *api, unsigned self)
{
    if (self == H9_RETURN_RADIO) { background_tick(); return 0; }
    if (api->version < 3 || A->switching != BEACON_HANDOFF_READY) return 0;
    A->switching = 0;
    unsigned side = aprs_tx_side();
    if (side > 1) side = A->home_side;
    aprs_side(side);
    uint32_t frequency = TX_FREQ;
    aprs_side(A->home_side);
    SPEAKER_ON(A->home_speaker);
    if (frequency < TX_LOW || frequency > TX_HIGH) {
        aprs_line(BODY_Y, "TX NOT ON 2M", UI_DANGER, UI_BG);
        h9_delay(1200);
        return 0;
    }
    struct h9_entry e;
    api->read(&e, api->bundle + H9_BUNDLE_HEADER + H9_BUNDLE_ENTRY * self, sizeof e);
    if (!e.size || e.size > 0x2000 || e.entry >= e.size) return 0;
    struct screen *s = BEACON_BACKGROUND_STATE;
    volatile uint8_t *state = (volatile uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; i++) state[i] = 0;
    if (!snapshot(s, 1)) return 0;
    BEACON_BACKGROUND_STATE->background_side = (uint8_t)side;
    BEACON_BACKGROUND_STATE->background_freq = frequency;
    BEACON_BACKGROUND_STATE->background_setting = BEACON_TX_SETTING;
    BEACON_RESIDENT->entry = api->load + e.entry;
    BEACON_RESIDENT->lencrc = e.size << 16 | e.crc;
    BEACON_RESIDENT->tick = TICK;
    BEACON_RESIDENT->flags = 0;
    BEACON_RESIDENT->magic = BEACON_RESIDENT_MAGIC;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, UI_BG);
    BEACON_REDRAW_RADIO();
    return H9_RETURN_RADIO;
}
#endif

#ifdef APRS_BEACON_RESIDENT
unsigned module_main(const struct h9_api *api, unsigned self) { return resident_main(api, self); }
#else
static int start_background(const struct h9_api *api, struct screen *s)
{
    if (api->version < 3 || !aprs_go(api, ".APRS BG")) return 0;
    snapshot(s, 0);
    A->switching = BEACON_HANDOFF_READY;
    return 1;
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    struct screen s;
    struct h9_key_ramp keys = {0};
    unsigned key_now = BKEY_WAIT, since = 0, handled = 0, second = 0xFFFFu, quiet_until = 0;
    (void)self;
    if (api->version < 3) return 0;
    aprs_open();
    int resume = snapshot(&s, 1);
    for (unsigned i = resume ? BEACON_SNAPSHOT_BYTES : 0; i < sizeof s; i++) ((uint8_t *)&s)[i] = 0;
    RING_READ = RING_WRITE;                             /* start from now */
    s.cursor = RING_READ & RING_MASK;
    if (!resume) {
        s.t0 = TICK;
        int32_t lat, lon;
        if (fix_load_micro(api->read, &lat, &lon)) beacon_from_micro(&s.g, lat, lon);     /* the saved fix, until a live one arrives */
    }
    for (;;) {
        unsigned redraw = read_gps(&s);
        RING_READ = s.cursor;

        /* Keys. The keypad reads Menu for a few ms when the speaker amp starts and after a transmit, so: taps count
         * after BTAP_TICKS, starting and stopping needs a BHOLD_TICKS hold, and keys are ignored for a moment after a TX. */
        unsigned key = h9_key(), tick = TICK;
        unsigned arrow = h9_key_ramp(&keys, key_now == BKEY_WAIT || tick < quiet_until ? 0 : key, tick, 1);
        if (arrow == H9_KEY_UP) { s.sel = s.sel ? s.sel - 1 : ROWS - 1; redraw = 1; }
        else if (arrow == H9_KEY_DOWN) { s.sel = s.sel + 1 < ROWS ? s.sel + 1 : 0; redraw = 1; }
        if (key_now == BKEY_WAIT) { if (key == H9_KEY_NONE) key_now = key; }          /* wait for every key to be released */
        else if (tick < quiet_until) { key_now = BKEY_WAIT; }
        else if (key != key_now) {
            if (key_now == H9_KEY_MENU && !handled && tick - since >= BTAP_TICKS) {   /* a tap: change the selected setting */
                if (s.sel == 0) A->beacon ^= 1;
                else if (s.sel == 1) A->beacon = (uint8_t)((A->beacon & ~(7 << 3)) | ((B_EVERY + 1) % 6) << 3);
                else A->beacon = (uint8_t)((A->beacon & ~(3 << 1)) | ((B_SYMBOL + 1) % 3) << 1);
                if (s.armed && s.sel == 0 && !B_TIMED) start_scheduler(&s);
                redraw = 1;
            }
            key_now = key; since = tick; handled = 0;
            if (key == H9_KEY_5) {
                note(&s, transmit(&s, 0));
                quiet_until = TICK + QUIET_TICKS;
                redraw = 1;
            }
        } else if (key == H9_KEY_MENU && !handled && tick - since >= BHOLD_TICKS) {   /* a hold: start or stop */
            handled = 1;
            set_armed(&s, !s.armed);
            redraw = 1;
        } else if (key == H9_KEY_BACK && !handled && tick - since >= BTAP_TICKS) {
            if (s.armed && start_background(api, &s)) return 0;
            snapshot(&s, 0);
            if (aprs_go(api, APRS_LIST)) return 0;                                   /* the list restores the radio when it leaves */
            aprs_side(A->home_side); SPEAKER_ON(A->home_speaker); return 0;
        }

        unsigned sec = (now_ms(&s) / 1000u) & 0xFFFFu;
        if (sec != second) {                            /* once a second */
            second = sec;
            if (due(&s)) {
                unsigned why = transmit(&s, 0);
                note(&s, why);
                if (why) {                                     /* retry after a minute, including clock rollover */
                    s.retry_started_ms = now_ms(&s);
                    s.pending |= BP_RETRY;
                }
                quiet_until = TICK + QUIET_TICKS;
                key_now = BKEY_WAIT;
            }
            redraw = 1;
        }
        if (redraw) draw(&s);
        h9_delay(2);
    }
}
#endif
