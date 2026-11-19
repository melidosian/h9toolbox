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
 * Beaconing is OFF when the screen opens and only runs while it is open.
 * Keys: Up/Down choose MODE, EVERY or SYMBOL, Menu changes it; HOLD Menu (0.6 s)
 * starts or stops beaconing (a keypad glitch is far shorter, so it cannot start
 * or stop it); 5 sends one now; Back leaves (and stops). The settings are kept
 * for the session (A->beacon).
 * Unattended transmitting needs an amateur licence: you are responsible. */
#include "aprs_core.h"
#include "aprs_tx.h"
#include "beacon_core.h"
#include "fix_store.h"
#include "third_party/smartbeacon/smartbeacon.h"

enum { BTAP_TICKS = 3, BHOLD_TICKS = 60, QUIET_TICKS = 50, ROW = 11, TOP = BODY_Y, ROWS = 3, GUARD_MS = 60000 };
enum { BKEY_WAIT = 0xFE };

static const char SYMBOL[3] = { '[', 'b', '>' };
static const char *const SYMBOL_NAME[3] = { "PERSON", "BIKE", "CAR" };
static const uint8_t EVERY[6] = { 1, 2, 5, 10, 15, 30 };       /* minutes, TIMED mode */
static const char COMMENT[] = "H9 Toolbox";

#define B_TIMED (A->beacon & 1)
#define B_SYMBOL ((A->beacon >> 1) & 3)
#define B_EVERY ((A->beacon >> 3) & 7)

struct screen {
    struct beacon_gps g;
    struct beacon_feed feed;
    sb_state_t sb;
    uint32_t t0, last_ms, armed_ms, hold_until;
    unsigned sent;
    uint8_t armed, sel, result_ok;
    const char *result;          /* the last transmit's outcome */
};

static uint32_t now_ms(const struct screen *s) { return (TICK - s->t0) * 10u; }

/* Send one position report now. Returns why not, or 0. */
static const char *transmit(struct screen *s)
{
    char info[BEACON_INFO];
    uint8_t kiss[APRS_TX_KISS_MAX];
    if (!A->mycall[0]) return "NO CALLSIGN SET";
    if (!s->g.lat[0]) return "NO POSITION YET";
    struct beacon_gps g = s->g;
    if (!g.fix) g.knots10 = 0;                          /* no course or speed without a fix */
    beacon_info(info, &g, SYMBOL[B_SYMBOL], g.fix ? COMMENT : "H9 Toolbox, last known");
    aprs_side(aprs_tx_side());                          /* the TX check reads this side */
    const char *why = 0;
    unsigned len = 0;
    if (TX_FREQ < TX_LOW || TX_FREQ > TX_HIGH) why = "TX NOT ON 2M";
    else if (!(len = aprs_tx_kiss(kiss, A->mycall, APRS_TX_DEST, APRS_TX_PATH, info))) why = "BAD CALLSIGN";
    else {
        ((void (*)(const uint8_t *, unsigned))STOCK_KISS_TX)(kiss, len);
        aprs_quiet();                                   /* stock's end of TX turns the speaker on */
    }
    aprs_side(aprs_rx_side());
    if (why) return why;
    s->sent++;
    s->last_ms = now_ms(s);
    s->sb.last_tx_ms = s->last_ms;
    s->sb.tx_course = s->g.course;
    return 0;
}

static void note(struct screen *s, const char *why)
{
    s->result_ok = why == 0;
    s->result = why ? why : "SENT";
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
    s->result = 0;
    if (!on) return;
    s->sent = 0;
    s->armed_ms = s->last_ms = now_ms(s);
    s->hold_until = 0;
    start_scheduler(s);
}

/* "45S", "3M": a short time span. */
static unsigned put_span(char *out, unsigned seconds)
{
    unsigned n = put_uint(out, seconds < 90 ? seconds : (seconds + 30) / 60);
    out[n++] = seconds < 90 ? 'S' : 'M';
    out[n] = 0;
    return n;
}

static void draw_setting(unsigned i, const struct screen *s)
{
    char b[28];
    unsigned n, y = TOP + 2 * ROW + 4 + i * 12;
    const char *label = i == 0 ? "MODE   " : i == 1 ? "EVERY  " : "SYMBOL ";
    n = put_str(b, label);
    if (i == 0) put_str(b + n, B_TIMED ? "TIMED" : "SMART");
    else if (i == 1) { if (B_TIMED) { n += put_uint(b + n, EVERY[B_EVERY]); put_str(b + n, " MIN"); } else put_str(b + n, "AUTO"); }
    else put_str(b + n, SYMBOL_NAME[B_SYMBOL]);
    int sel = i == s->sel;
    gfx_fill(0, y, H9_WIDTH, 12, sel ? GFX_YELLOW : GFX_BLACK);
    gfx_text(4, y + 2, b, 25, 1, sel ? GFX_BLACK : (i == 1 && !B_TIMED ? GFX_GREY : GFX_WHITE), sel ? GFX_YELLOW : GFX_BLACK);
}

static void draw(const struct screen *s)
{
    char b[28];
    unsigned n, y = TOP;
    gfx_fill(0, 0, H9_WIDTH, TITLE_H, GFX_BLUE);
    gfx_text(4, 1, "BEACON", 8, 2, GFX_WHITE, GFX_BLUE);
    gfx_text(112, 5, s->armed ? "ON AIR" : "OFF", 8, 1, s->armed ? GFX_GREEN : GFX_WHITE, GFX_BLUE);
    gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, GFX_BLACK);
    n = put_str(b, A->mycall[0] ? A->mycall : "NO CALL");
    b[n++] = ' ';
    n += aprs_tx_mhz(b + n, TX_FREQ);
    put_str(b + n, " MHZ");
    aprs_line(y, b, !A->mycall[0] ? GFX_RED : TX_FREQ >= TX_LOW && TX_FREQ <= TX_HIGH ? GFX_WHITE : GFX_YELLOW, GFX_BLACK); y += ROW;
    if (s->g.fix) {
        n = put_str(b, "GPS "); n += put_uint(b + n, (s->g.kmh10 + 5u) / 10u);
        n += put_str(b + n, " KM/H  COURSE "); put_uint(b + n, s->g.course);
        aprs_line(y, b, GFX_GREEN, GFX_BLACK);
    } else if (!s->g.lat[0]) {
        aprs_line(y, "NO FIX, NO POSITION YET", GFX_RED, GFX_BLACK);
    } else {
        aprs_line(y, "NO FIX: LAST POSITION", GFX_YELLOW, GFX_BLACK);
    }
    for (unsigned i = 0; i < ROWS; i++) draw_setting(i, s);
    y = TOP + 2 * ROW + 4 + ROWS * 12 + 3;
    aprs_line(y, s->armed ? "BEACON ON  HOLD MENU:STOP" : "BEACON OFF HOLD MENU:START", s->armed ? GFX_GREEN : GFX_GREY, GFX_BLACK); y += ROW;
    n = put_str(b, "SENT "); n += put_uint(b + n, s->sent);
    if (s->sent) { n += put_str(b + n, "  LAST "); n += put_span(b + n, (now_ms(s) - s->last_ms) / 1000u); put_str(b + n, " AGO"); }
    aprs_line(y, b, GFX_WHITE, GFX_BLACK); y += ROW;
    if (s->result) aprs_line(y, s->result, s->result_ok ? GFX_GREEN : GFX_RED, GFX_BLACK);
    else if (s->armed) {                                /* when the next one is due at the latest */
        uint32_t gap = !B_TIMED ? s->sb.cfg.park_s * 1000u : s->sent ? EVERY[B_EVERY] * 60000u : 15000u, since = now_ms(s) - s->last_ms;
        n = put_str(b, B_TIMED ? "NEXT IN " : "SMART: WITHIN ");
        put_span(b + n, since >= gap ? 0 : (gap - since) / 1000u);
        aprs_line(y, b, GFX_GREY, GFX_BLACK);
    }
    aprs_line(FOOT_Y, "UP/DN ROW MENU SET 5 NOW", GFX_GREY, GFX_BLACK);
}

/* One second of scheduling: returns 1 if a position is due. */
static int due(struct screen *s)
{
    if (!s->armed || now_ms(s) < s->hold_until) return 0;
    if (B_TIMED) return beacon_timed_due(now_ms(s), s->last_ms, s->sent, EVERY[B_EVERY]);
    return sb_tick(&s->sb, now_ms(s), 0);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    struct screen s;
    unsigned key_now = BKEY_WAIT, since = 0, handled = 0, second = 0xFFFFu, quiet_until = 0;
    (void)self;
    for (unsigned i = 0; i < sizeof s; i++) ((uint8_t *)&s)[i] = 0;
    aprs_open();
    RING_READ = RING_WRITE;                             /* start from now */
    s.t0 = TICK;
    {
        int32_t lat, lon;
        if (fix_load_micro(api->read, &lat, &lon)) beacon_from_micro(&s.g, lat, lon);     /* the saved fix, until a live one arrives */
    }
    for (;;) {
        unsigned read = RING_READ & RING_MASK, write = RING_WRITE & RING_MASK, redraw = 0;
        while (read != write) {
            if (beacon_feed(&s.feed, RING[read], &s.g)) {
                if (s.armed && !B_TIMED) sb_on_rmc(&s.sb, now_ms(&s), s.g.fix, s.g.kmh10, s.g.course);
                redraw = 1;
            }
            read = (read + 1) & RING_MASK;
        }
        RING_READ = (uint16_t)read;

        /* Keys. The keypad reads Menu for a few ms when the speaker amp starts and after a transmit, so: taps count
         * after BTAP_TICKS, starting and stopping needs a BHOLD_TICKS hold, and keys are ignored for a moment after a TX. */
        unsigned key = h9_key(), tick = TICK;
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
            if (key == H9_KEY_UP) { s.sel = s.sel ? s.sel - 1 : ROWS - 1; redraw = 1; }
            else if (key == H9_KEY_DOWN) { s.sel = s.sel + 1 < ROWS ? s.sel + 1 : 0; redraw = 1; }
            else if (key == H9_KEY_5) {
                note(&s, transmit(&s));
                quiet_until = TICK + QUIET_TICKS;
                gfx_fill(0, TITLE_H, H9_WIDTH, FOOT_Y - TITLE_H, GFX_BLACK);
                redraw = 1;
            }
        } else if (key == H9_KEY_MENU && !handled && tick - since >= BHOLD_TICKS) {   /* a hold: start or stop */
            handled = 1;
            set_armed(&s, !s.armed);
            redraw = 1;
        } else if (key == H9_KEY_BACK && !handled && tick - since >= BTAP_TICKS) {
            if (aprs_go(api, APRS_LIST)) return 0;                                   /* the list restores the radio when it leaves */
            aprs_side(A->home_side); SPEAKER_ON(A->home_speaker); return 0;
        }

        unsigned sec = (now_ms(&s) / 1000u) & 0xFFFFu;
        if (sec != second) {                            /* once a second */
            second = sec;
            if (due(&s)) {
                const char *why = transmit(&s);
                note(&s, why);
                if (why) s.hold_until = now_ms(&s) + GUARD_MS;   /* a refusal retries after a minute, not every second */
                quiet_until = TICK + QUIET_TICKS;
                key_now = BKEY_WAIT;
            }
            redraw = 1;
        }
        if (redraw) draw(&s);
        h9_delay(2);
    }
}
