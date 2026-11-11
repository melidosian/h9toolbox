/* Host test for the APRS core: compiles aprs_core.c itself with the screen,
 * keys and chip read stubbed, and checks its parsers against APRS spec
 * formats, plus messages and the screen-switch mailbox.
 * Run: py -3 -I ../../test_modules.py  (gcc in Docker image h9-hostcc). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APRS_HOST_TEST
#include "h9mod.h"
#include "gfx.h"

/* The mailbox lives at a fixed RAM address on the radio; here, a variable. */
static struct h9_request host_request;
#undef H9_REQUEST
#define H9_REQUEST (&host_request)

void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
unsigned gfx_text(unsigned x, unsigned y, const char *t, unsigned max, unsigned s, uint16_t f, uint16_t b)
{ (void)y; (void)f; (void)b; unsigned n = 0; while (n < max && t[n]) n++; return x + n * 6 * s; }
void gfx_wait_release(void) {}
unsigned gfx_wait_key(void) { return 0; }
/* Scripted keys and a simulated stock tick for aprs_event(). */
static uint32_t host_tick;
static unsigned tick_step = 1, tick_div = 1, polls;
static const uint8_t *script;           /* key per poll; ends at 0xFF (then no key) */
void h9_delay(unsigned u) { (void)u; polls++; if (polls % tick_div == 0) host_tick += tick_step; }
unsigned h9_key(void) { return script && script[polls] != 0xFF ? script[polls] : 0; }

static uint8_t host_mycall[7] = { 'N', '0', 'C', 'A', 'L', 'L', 7 };
#define MYCALL host_mycall
static uint8_t host_side, host_route, host_rx_ch, host_tx_ch;
static unsigned rf_setups;
#define SIDE_BYTE host_side
#define ROUTE_BYTE host_route
#define RF_SETUP(x) (rf_setups++, (void)(x))
static unsigned af_mode = 9, speaker_on = 1, quiets;
static uint8_t host_spk = 1;
#define AF_SELECT(m) (af_mode = (m), quiets++)
#define SPEAKER(on) (speaker_on = (on))
static unsigned rf30;
#define RF_WRITE(r, v) ((r) == 0x30 ? (void)(rf30 = (v)) : (void)0)
static uint8_t host_asleep; static unsigned wakes;
#define RF_ASLEEP host_asleep
#define RF_WAKE() (wakes++, host_asleep = 0)
#define SPEAKER_STATE host_spk
#define RX_CH host_rx_ch
#define TX_CH host_tx_ch
#define TICK host_tick
static uint8_t host_ring[2048];
static uint16_t host_write, host_read;
#define RING host_ring
#define RING_WRITE host_write
#define RING_READ host_read
#include "aprs_core.c"
#include "aprs_stored.c"
#include "aprs_tx.c"
#include "aprs_text.c"
#include "aprs_edit.c"
struct aprs aprs_host;

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, tol) CHECK(((a) > (b) ? (a) - (b) : (b) - (a)) <= (tol))

static void reset(void) { memset(&aprs_host, 0xA5, sizeof aprs_host); aprs_reset(); }

static const struct station *find(const char *call)
{
    for (unsigned i = 0; i < A->count; i++) if (!strcmp(A->st[i].call, call)) return &A->st[i];
    return NULL;
}

/* Raw AX.25 address field: 6 shifted chars + SSID byte (last-address bit). */
static unsigned addr(uint8_t *out, const char *call, unsigned ssid, int last)
{
    for (unsigned i = 0; i < 6; i++) out[i] = (uint8_t)((i < strlen(call) ? call[i] : ' ') << 1);
    out[6] = (uint8_t)(0x60 | ssid << 1 | (last ? 1 : 0));
    return 7;
}

/* Send one packet through the serial framing exactly as the modem does:
 * C0 00 <ax25 with C0/DB escaped> C0. */
static void send(const char *src, unsigned ssid, const char *dest, const char *info, unsigned info_len)
{
    uint8_t f[400];
    unsigned n = 0;
    n += addr(f + n, dest, 0, 0);
    n += addr(f + n, src, ssid, 0);
    n += addr(f + n, "WIDE1", 1, 1);
    f[n++] = 0x03; f[n++] = 0xF0;
    memcpy(f + n, info, info_len); n += info_len;
    aprs_feed(0xC0); aprs_feed(0x00);
    for (unsigned i = 0; i < n; i++) {
        if (f[i] == 0xC0) { aprs_feed(0xDB); aprs_feed(0xDC); }
        else if (f[i] == 0xDB) { aprs_feed(0xDB); aprs_feed(0xDD); }
        else aprs_feed(f[i]);
    }
    aprs_feed(0xC0);
}
#define SEND(src, ssid, dest, info) send(src, ssid, dest, info, sizeof info - 1)

static void gps(const char *sentence)
{
    uint8_t f[120];
    unsigned n = 0;
    f[n++] = 0x10;
    for (const char *p = sentence; *p; p++) f[n++] = (uint8_t)*p;
    f[n++] = '\r'; f[n++] = '\n';
    aprs_feed(0xC0);
    for (unsigned i = 0; i < n; i++) aprs_feed(f[i]);
    aprs_feed(0xC0);
}

/* Fake resource chip for the stored Beacon List. */
static uint8_t chip[0x10000];
/* Fake bundle (for the switch lookup) below the Beacon List copy. */
static uint8_t bundle[16 + 32 * 8];
#define BUNDLE_AT 0xCC1000u
static void chip_read(void *dst, uint32_t a, uint32_t len)
{
    if (a >= BUNDLE_AT && a < BUNDLE_AT + sizeof bundle) memcpy(dst, bundle + (a - BUNDLE_AT), len);
    else memcpy(dst, chip + (a - BEACONS), len);
}
static void set_bundle(const char *const *names, unsigned n)
{
    memset(bundle, 0, sizeof bundle);
    bundle[10] = (uint8_t)n;
    for (unsigned i = 0; i < n; i++) strncpy((char *)bundle + 16 + 32 * i, names[i], 16);
}

static void put_record(unsigned slot, const char *src, unsigned ssid, const char *info)
{
    uint8_t *r = chip + 6 + slot * BEACON_RECORD;
    memset(r, 0, BEACON_RECORD);
    r[0] = BEACON_RECORD & 255; r[1] = BEACON_RECORD >> 8;
    uint8_t *d = r + 4;
    memcpy(d + R_DEST, "APRS", 4);
    memcpy(d + R_SRC, src, strlen(src)); d[R_SRC + 6] = (uint8_t)ssid;
    unsigned len = (unsigned)strlen(info);
    memcpy(d + R_INFO, info, len);
    d[R_LEN] = (uint8_t)len; d[R_LEN + 1] = (uint8_t)(len >> 8);
}

int main(void)
{
    /* Uncompressed position (APRS 1.0.1 ch. 8 example). */
    reset();
    SEND("N0CALL", 9, "APRS", "!4903.50N/07201.75W-Test 001234");
    const struct station *s = find("N0CALL-9");
    CHECK(s && s->has_pos);
    if (s) {
        CHECK(s->lat == 49 * 6000 + 3 * 100 + 50);
        CHECK(s->lon == -(72 * 6000 + 1 * 100 + 75));
        CHECK(s->table == '/' && s->symbol == '-');
        CHECK(!strcmp(s->text, "Test 001234"));
        CHECK(s->live);
    }

    /* Timestamped, and a byte needing C0/DB escapes in the comment. */
    SEND("K1ABC", 0, "APRS", "@092345z4903.50N/07201.75W>\xc0\xdb ok");
    s = find("K1ABC");
    CHECK(s && s->has_pos && s->symbol == '>');
    if (s) CHECK(!strcmp(s->text, "   ok"));          /* non-printables shown as spaces */
    CHECK(A->st[0].call[0] == 'K');                     /* newest first */

    /* Compressed (ch. 9 example): 49.5 N, 72.75 W. */
    SEND("W2XYZ", 0, "APRS", "=/5L!!<*e7>7P[");
    s = find("W2XYZ");
    CHECK(s && s->has_pos);
    if (s) { NEAR(s->lat, 297000, 2); NEAR(s->lon, -436500, 2); CHECK(s->symbol == '>'); }

    /* Mic-E: 33 25.64 N, 112 07.74 W, encoded per ch. 10:
     * destination digits 3 3 2 5 6 4 with N (P-Z at 4th), +100 (P-Z at 5th),
     * W (P-Z at 6th) = "332UVT"; longitude 112-100+28 '(', 7+88 '_', 74+28 'f'. */
    SEND("KD7MIC", 7, "332UVT", "`(_fn\"Oj/]comment");
    s = find("KD7MIC-7");
    CHECK(s && s->has_pos);
    if (s) {
        CHECK(s->lat == 33 * 6000 + 25 * 100 + 64);
        CHECK(s->lon == -(112 * 6000 + 7 * 100 + 74));
        CHECK(s->symbol == 'j' && s->table == '/');
        CHECK(!strcmp(s->text, "]comment"));
    }

    /* Object: name replaces the sender's call. */
    SEND("N0CALL", 0, "APRS", ";LEADER   *092345z4903.50N/07201.75W>088/036");
    s = find("LEADER");
    CHECK(s && s->has_pos && s->lat == 294350);

    /* Status (no position) keeps an earlier position. */
    SEND("N0CALL", 9, "APRS", ">On the air");
    s = find("N0CALL-9");
    CHECK(s && s->has_pos && !strcmp(s->text, ">On the air"));
    CHECK(A->st[0].call[0] == 'N' && !strcmp(A->st[0].call, "N0CALL-9"));

    /* Selection follows its station when another moves to the top. */
    A->list_sel = 2;
    char sel[CALL]; strcpy(sel, A->st[2].call);
    SEND("W2XYZ", 0, "APRS", ">moved");
    CHECK(!strcmp(A->st[A->list_sel].call, sel));

    /* GPS RMC fix, then distance and bearing. */
    reset();
    CHECK(!A->my_fix);
    gps("$GPRMC,123519,V,,,,,,,230394,,*6A");
    CHECK(A->gps_seen && !A->my_fix);
    gps("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
    CHECK(A->my_fix && A->my_lat == 48 * 6000 + 7 * 100 + 3 && A->my_lon == 11 * 6000 + 31 * 100);
    gps("$GNRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A");
    CHECK(A->my_fix);
    struct station t = { .has_pos = 1 };
    unsigned km; const char *pt;
    t.lat = A->my_lat + 6000; t.lon = A->my_lon;               /* 1 degree north */
    CHECK(aprs_range(&t, &km, &pt)); NEAR(km, 1111u, 15); CHECK(!strcmp(pt, "N"));
    t.lat = A->my_lat; t.lon = A->my_lon + 6000;               /* 1 degree east at 48 N */
    CHECK(aprs_range(&t, &km, &pt)); NEAR(km, 744u, 15); CHECK(!strcmp(pt, "E"));
    t.lat = A->my_lat - 3000; t.lon = A->my_lon - 4500;        /* south-west */
    CHECK(aprs_range(&t, &km, &pt)); CHECK(!strcmp(pt, "SW"));
    t.lat = A->my_lat + 600; t.lon = A->my_lon - 200;          /* mostly north, a little west */
    CHECK(aprs_range(&t, &km, &pt)); CHECK(!strcmp(pt, "NNW"));
    t.lat = A->my_lat - 6000 * 40; t.lon = A->my_lon + 6000 * 100;   /* far: no overflow */
    CHECK(aprs_range(&t, &km, &pt)); CHECK(km > 50000 && km < 200000);
    char buf[24];
    t.lat = A->my_lat + 30; t.lon = A->my_lon;
    put_range(buf, &t); CHECK(!strcmp(buf, "0.5KM N"));

    /* Stored Beacon List: newest first, one row per call, older fills position. */
    reset();
    memset(chip, 0xFF, sizeof chip);
    chip[0] = 3; chip[1] = 0; chip[2] = 3; chip[3] = 0;     /* count 3, next 3 */
    put_record(0, "OLD", 0, "!4903.50N/07201.75W-old");
    put_record(1, "AB1CD", 5, "!4000.00N/07400.00W>first");
    put_record(2, "AB1CD", 5, ">status only");
    struct h9_api api = { .read = chip_read, .bundle = BUNDLE_AT };
    A->dirty = 1; A->held = HELD; A->last_key = KEY_WAIT; A->list_sel = 0; A->nmea_len = 0;
    aprs_load_stored(&api);
    /* Records are bigger than A->frame: the state after it must survive
     * (an overrun cleared the redraw flag and faked a Menu tap on hardware). */
    CHECK(A->dirty == 1 && A->held == HELD && A->last_key == KEY_WAIT && A->list_sel == 0 && A->nmea_len == 0);
    CHECK(A->count == 2);
    CHECK(!strcmp(A->st[0].call, "AB1CD-5") && !strcmp(A->st[1].call, "OLD"));
    CHECK(!strcmp(A->st[0].text, ">status only") && A->st[0].has_pos && A->st[0].lat == 240000);
    CHECK(!A->st[0].live);
    SEND("AB1CD", 5, "APRS", ">live now");
    CHECK(A->count == 2 && A->st[0].live);

    /* Erased chip: nothing loaded. */
    reset();
    memset(chip, 0xFF, sizeof chip);
    aprs_load_stored(&api);
    CHECK(A->count == 0);

    /* Full list drops the oldest live entry. */
    reset();
    char call[8];
    for (unsigned i = 0; i < STATIONS + 3; i++) { sprintf(call, "T%u", i); send(call, 0, "APRS", ">x", 2); }
    sprintf(call, "T%u", STATIONS + 2); CHECK(A->count == STATIONS && !strcmp(A->st[0].call, call) && !find("T0"));

    /* Garbage frames are ignored safely. */
    reset();
    aprs_feed(0xC0); aprs_feed(0x00); aprs_feed(0x41); aprs_feed(0xC0);
    aprs_feed(0xC0); aprs_feed(0x55); aprs_feed(0x41); aprs_feed(0xC0);
    for (unsigned i = 0; i < 600; i++) aprs_feed(0x7E);
    aprs_feed(0xC0);
    CHECK(A->count == 0);


    /* Messages: to me (green, counted), to others, acks skipped, retries merged. */
    reset();
    strcpy(A->mycall, "N0CALL-7");
    SEND("W1AW", 0, "APRS", ":N0CALL-7 :Hello there{12");
    SEND("W1AW", 0, "APRS", ":N0CALL-7 :Hello there{12");      /* retry */
    SEND("K2BBB", 0, "APRS", ":W3CCC    :Not for you");
    SEND("N0CALL", 7, "APRS", ":W1AW     :ack12");
    CHECK(A->msgs == 2 && A->unread == 1);
    CHECK(!strcmp(A->msg[0].from, "K2BBB") && !strcmp(A->msg[0].to, "W3CCC") && !A->msg[0].mine);
    CHECK(!strcmp(A->msg[1].text, "Hello there") && A->msg[1].mine && A->msg[1].live);
    for (unsigned i = 0; i < MESSAGES + 2; i++) { char t[32]; sprintf(t, ":X        :m%u", i); send("Z9", 0, "APRS", t, strlen(t)); }
    CHECK(A->msgs == MESSAGES && !strcmp(A->msg[0].text, "m7"));

    /* Delivery: an ack from the recipient, addressed to me, with the id I sent marks the message. */
    reset();
    strcpy(A->mycall, "N0CALL-7");
    A->msgs = 1;
    strcpy(A->msg[0].from, "N0CALL-7"); strcpy(A->msg[0].to, "W1AW"); strcpy(A->msg[0].text, "hi");
    A->msg[0].id = 12; A->msg[0].live = 1; A->msg[0].mine = 0; A->msg[0].acked = 0;
    SEND("K2BBB", 0, "APRS", ":N0CALL-7 :ack12");                 /* not the recipient */
    SEND("W1AW", 0, "APRS", ":N0CALL-7 :ack13");                  /* another message's id */
    SEND("W1AW", 0, "APRS", ":W3CCC    :ack12");                  /* to someone else */
    CHECK(!A->msg[0].acked && A->msgs == 1);
    SEND("W1AW", 0, "APRS", ":N0CALL-7 :ack12");
    CHECK(A->msg[0].acked && A->msgs == 1 && A->dirty);
    SEND("W1AW", 0, "APRS", ":N0CALL-7 :rej12");                  /* a reject is not an ack (and adds nothing) */
    CHECK(A->msgs == 1);

    /* Heard age: NOW, minutes, hours; stored stations have none. */
    reset();
    host_tick = 1000000;
    SEND("K1ABC", 0, "APRS", ">hi");
    char age[8];
    CHECK(put_age(age, &A->st[0]) == 3 && !strcmp(age, "NOW"));
    host_tick += 100 * 125;                                      /* 125 s */
    CHECK(!strcmp((put_age(age, &A->st[0]), age), "2M"));
    host_tick += 100 * 3600 * 3;
    CHECK(!strcmp((put_age(age, &A->st[0]), age), "3H"));
    host_tick += 100 * 3600 * 200;
    CHECK(!strcmp((put_age(age, &A->st[0]), age), "99H") || age[0] == 'N' || age[1] == 'M' || age[2] == 'H' || age[1] == 'H');   /* capped, or wrapped */
    A->st[0].live = 0;
    CHECK(put_age(age, &A->st[0]) == 0 && !age[0]);

    /* Tab keys: a digit names a screen; the current one and missing ones do nothing. */
    reset();
    const char *const names6[] = { "MENU", "APRS", "APRS MAP", "APRS MSG", "APRS SEND", "APRS BEACON" };
    set_bundle(names6, 6);
    struct h9_api api3 = { .read = chip_read, .bundle = BUNDLE_AT };
    host_request.magic = 0;
    CHECK(!aprs_tab(&api3, EV_DIGIT + 1, TAB_LIST) && host_request.magic == 0);     /* already there */
    CHECK(!aprs_tab(&api3, EV_DIGIT + 6, TAB_LIST) && !aprs_tab(&api3, EV_UP, TAB_LIST) && host_request.magic == 0);
    CHECK(aprs_tab(&api3, EV_DIGIT + 5, TAB_LIST) && !strcmp(host_request.name, "APRS BEACON"));
    CHECK(aprs_tab(&api3, EV_DIGIT + 2, TAB_LIST) && !strcmp(host_request.name, "APRS MAP"));
    const char *const names2[] = { "MENU", "APRS" };
    set_bundle(names2, 2);
    host_request.magic = 0;
    CHECK(!aprs_tab(&api3, EV_DIGIT + 3, TAB_LIST) && host_request.magic == 0);   /* not installed */

    /* Screen switch: request names the next installed screen. */
    reset();
    const char *const names3[] = { "MENU", "APRS", "APRS MSG" };     /* no map installed */
    set_bundle(names3, 3);
    struct h9_api api2 = { .read = chip_read, .bundle = BUNDLE_AT };
    host_request.magic = 0;
    CHECK(!aprs_go(&api2, APRS_MAP) && host_request.magic == 0 && !A->switching);
    CHECK(aprs_go(&api2, APRS_MSG));
    CHECK(host_request.magic == H9_REQUEST_MAGIC && !strcmp(host_request.name, "APRS MSG") && A->switching);
    const char *const names1[] = { "MENU", "APRS" };
    set_bundle(names1, 2);
    host_request.magic = 0;
    CHECK(!aprs_go(&api2, APRS_MSG));                             /* not installed */
    CHECK(host_request.magic == 0);

    /* Open: a switch keeps state and the partial frame; a fresh start reloads. */
    reset();
    memset(chip, 0xFF, sizeof chip);
    SEND("K1ABC", 0, "APRS", ">hi");
    A->switching = 1; A->length = 5;
    CHECK(!aprs_open());
    CHECK(A->count == 1 && A->length == 5 && !A->switching);
    CHECK(aprs_open());
    CHECK(A->count == 1 && A->length == 0);
    memset(&aprs_host, 0, sizeof aprs_host);                     /* not initialised */
    CHECK(aprs_open());
    CHECK(A->magic == APRS_MAGIC && A->count == 0);
    CHECK(!strcmp(A->mycall, "N0CALL-7"));                      /* from RAM 0xC4A1 / 0xC4A7 */
    host_mycall[6] = 0;
    A->switching = 0;
    aprs_open();
    CHECK(!strcmp(A->mycall, "N0CALL"));
    CHECK(APRS_BYTES <= APRS_SLOT_BYTES);

    /* Keys: a tap is Menu, 0.6 s of Menu is a hold (stock tick ~100/s). */
    {
        static uint8_t keys[2000];
        memset(keys, 0, sizeof keys);
        for (unsigned i = 0; i < 20; i++) keys[i] = H9_KEY_MENU;          /* 0.2 s tap */
        for (unsigned i = 100; i < 300; i++) keys[i] = H9_KEY_MENU;       /* 2 s hold */
        keys[400] = keys[401] = H9_KEY_UP;
        keys[1999] = 0xFF;
        reset(); script = keys; polls = 0; tick_step = 1;
        A->last_key = 0; A->held = HELD; A->dirty = 0;
        CHECK(aprs_event() == EV_MENU && polls >= 20 && polls < 25);
        CHECK(aprs_event() == EV_HOLD && polls >= 160 && polls < 165);
        CHECK(aprs_event() == EV_UP);                               /* no Menu after the hold */
        /* A tap after opening with Menu still down (from a hold) does nothing until released. */
        memset(keys, 0, sizeof keys);
        for (unsigned i = 0; i < 50; i++) keys[i] = H9_KEY_MENU;
        keys[60] = keys[61] = H9_KEY_DOWN;
        keys[1999] = 0xFF;
        polls = 0; A->last_key = H9_KEY_MENU; A->held = HELD;
        CHECK(aprs_event() == EV_DOWN);
        /* Polls much faster than the tick (2 ms poll, 10 ms tick): a 0.3 s
         * tap is still a tap, a 0.7 s hold a hold. */
        memset(keys, 0, sizeof keys);
        for (unsigned i = 0; i < 150; i++) keys[i] = H9_KEY_MENU;        /* 150 polls = 0.3 s */
        for (unsigned i = 200; i < 550; i++) keys[i] = H9_KEY_MENU;      /* 0.7 s */
        keys[1999] = 0xFF;
        reset(); polls = 0; host_tick = 0; A->last_key = 0; A->held = HELD; A->dirty = 0;
        tick_step = 1; tick_div = 5;                                /* 1 tick per 5 polls */
        CHECK(aprs_event() == EV_MENU && polls >= 150 && polls < 155);
        CHECK(aprs_event() == EV_HOLD && polls >= 500 && polls < 506); /* 60 ticks after poll 200 */
        tick_div = 1;
        /* A Menu reading of a few ms (ADC glitch) is not a tap. */
        memset(keys, 0, sizeof keys);
        keys[10] = H9_KEY_MENU; keys[30] = keys[31] = H9_KEY_MENU;   /* 1 and 2 samples */
        keys[50] = H9_KEY_UP; keys[51] = H9_KEY_UP;
        keys[1999] = 0xFF;
        reset(); polls = 0; host_tick = 0; A->last_key = 0; A->held = HELD; A->dirty = 0;
        CHECK(aprs_event() == EV_UP && polls >= 50 && polls < 53);
        /* Battery-save sleep: aprs_quiet wakes the RF chip, then opens audio. */
        host_asleep = 1; wakes = 0; rf30 = 0;
        aprs_quiet();
        CHECK(wakes == 1 && !host_asleep && rf30 == 0xBFF5);
        /* Just opened: keys ignored until all are released (a Menu still down
         * from the menu, read as nothing for one sample, is not a tap). */
        memset(keys, 0, sizeof keys);
        keys[0] = 0; for (unsigned i = 1; i < 40; i++) keys[i] = H9_KEY_MENU;
        keys[80] = keys[81] = H9_KEY_UP;
        keys[1999] = 0xFF;
        reset(); polls = 0; A->switching = 1; aprs_open(); A->dirty = 0;
        CHECK(aprs_event() == EV_UP);
        script = NULL; tick_step = 1;
    }


    /* Digit keys: EV_DIGIT + d on the press (stock code d + 2). */
    {
        static uint8_t keys[100];
        memset(keys, 0, sizeof keys);
        keys[3] = keys[4] = 2 + 7;                                  /* digit 7 */
        keys[99] = 0xFF;
        reset(); script = keys; polls = 0; A->last_key = 0; A->held = HELD; A->dirty = 0;
        CHECK(aprs_event() == EV_DIGIT + 7);
        script = NULL;
    }

    /* Multi-tap entry (APRS SEND). */
    {
        struct edit e;
        edit_init(&e, EDIT_MAX, 0, 0);
        edit_digit(&e, 2, 0);                                      /* A */
        edit_digit(&e, 2, 10);                                     /* B: same key within 1 s */
        edit_digit(&e, 2, 20);                                     /* C */
        edit_digit(&e, 2, 30);                                     /* 2 */
        edit_digit(&e, 2, 40);                                     /* back to A */
        CHECK(!strcmp(e.buf, "A"));
        edit_digit(&e, 2, 40 + EDIT_CYCLE_TICKS);                  /* after 1 s: a new A */
        edit_digit(&e, 0, 300);                                    /* space */
        edit_digit(&e, 7, 310); edit_digit(&e, 7, 320); edit_digit(&e, 7, 330); edit_digit(&e, 7, 340);
        CHECK(!strcmp(e.buf, "AA S"));
        edit_digit(&e, 1, 500);                                    /* . */
        CHECK(!strcmp(e.buf, "AA S."));
        edit_back(&e); edit_back(&e);
        CHECK(!strcmp(e.buf, "AA ") && e.len == 3);
        edit_back(&e);
        edit_digit(&e, 2, 600);                                    /* after a delete: new character */
        CHECK(!strcmp(e.buf, "AAA"));
        /* Callsign: only A-Z, 0-9, '-'; at most 9. */
        edit_init(&e, 9, 1, 0);
        edit_digit(&e, 0, 0);                                      /* "0", not a space */
        edit_digit(&e, 1, 200);                                    /* "-" */
        edit_digit(&e, 1, 210);                                    /* "1" */
        edit_digit(&e, 1, 220);                                    /* back to "-" */
        CHECK(!strcmp(e.buf, "0-"));
        for (unsigned i = 0; i < 12; i++) edit_digit(&e, 3, 1000 + i * 200);
        CHECK(e.len == 9 && !strcmp(e.buf, "0-DDDDDDD"));
        edit_digit(&e, 3, 3500);                                   /* full: ignored */
        CHECK(e.len == 9);
        edit_digit(&e, 3, 2210);                                   /* within 1 s of the last D: cycles it */
        CHECK(e.buf[8] == 'E');
        edit_init(&e, 9, 1, "KD9CRQ-7");
        CHECK(!strcmp(e.buf, "KD9CRQ-7") && e.len == 8);
        edit_init(&e, 4, 0, "TOO LONG");
        CHECK(!strcmp(e.buf, "TOO "));
    }

    /* Outgoing frames: N0CALL-1>APRS:>Hi (vector from test/kiss-tnc.test.mjs)
     * in KISS, then a path and rejected inputs. */
    {
        uint8_t k[APRS_TX_KISS_MAX];
        static const uint8_t want[] = { 0xC0, 0x00, 0x82,0xa0,0xa4,0xa6,0x40,0x40,0xe0,
            0x9c,0x60,0x86,0x82,0x98,0x98,0x63, 3,0xf0,0x3e,0x48,0x69, 0xC0 };
        unsigned n = aprs_tx_kiss(k, "N0CALL-1", "APRS", NULL, ">Hi");
        CHECK(n == sizeof want && !memcmp(k, want, n));
        n = aprs_tx_kiss(k, "ab1cd-15", APRS_TX_DEST, APRS_TX_PATH, ">t");
        CHECK(n == 2 + 21 + 2 + 2 + 1);
        CHECK(k[2 + 6] == 0xE0 && k[2 + 13] == (0x60 | 15 << 1));        /* dest C bit; src not last */
        CHECK(k[2 + 7] == ('A' << 1) && k[2 + 11] == ('D' << 1) && k[2 + 12] == (' ' << 1));
        CHECK(k[2 + 14] == ('W' << 1) && k[2 + 20] == (0x60 | 1 << 1 | 1)); /* WIDE1-1, last */
        CHECK(aprs_tx_kiss(k, "TOOLONG", "APRS", NULL, ">x") == 0);
        CHECK(aprs_tx_kiss(k, "AB1CD-16", "APRS", NULL, ">x") == 0);
        CHECK(aprs_tx_kiss(k, "AB1CD-", "APRS", NULL, ">x") == 0);
        CHECK(aprs_tx_kiss(k, "AB/CD", "APRS", NULL, ">x") == 0);
        CHECK(aprs_tx_kiss(k, "AB1CD", "APRS", NULL, "") == 0);
        CHECK(aprs_tx_kiss(k, "AB1CD", "APRS", NULL, "caf\xe9") == 0);
        char mhz[12];
        CHECK(aprs_tx_mhz(mhz, 14439000) == 8 && !strcmp(mhz, "144.3900"));
        CHECK(aprs_tx_mhz(mhz, 14400125) == 8 && !strcmp(mhz, "144.0012"));
        /* Round trip through the receive path: the KISS bytes, fed to the core
         * as if from the modem ring, come out as the sending station. */
        reset();
        n = aprs_tx_kiss(k, "AB1CD-9", APRS_TX_DEST, APRS_TX_PATH, ">H9 Toolbox test");
        for (unsigned i = 0; i < n; i++) aprs_feed(k[i]);
        CHECK(A->count == 1 && !strcmp(A->st[0].call, "AB1CD-9"));
    }


    /* APRS channel settings pick the side; stock RF setup runs only on a change. */
    {
        host_side = 0x05; host_route = 0; rf_setups = 0;
        host_rx_ch = 2; host_tx_ch = 1;                    /* RX CH-B, TX B-Lock */
        CHECK(aprs_rx_side() == 1 && aprs_tx_side() == 1);
        aprs_side(aprs_rx_side());
        CHECK(host_side == 0x85 && host_route == 1 && rf_setups == 1);
        aprs_side(1);
        CHECK(rf_setups == 1);                             /* already there */
        aprs_side(0);
        CHECK(host_side == 0x05 && host_route == 0 && rf_setups == 2);
        host_rx_ch = 0; host_tx_ch = 4;
        CHECK(aprs_rx_side() == 2 && aprs_tx_side() == 2);
        aprs_side(2);
        CHECK(rf_setups == 2 && host_side == 0x05);        /* "keep" changes nothing */
        host_tx_ch = 2; CHECK(aprs_tx_side() == 0);
        host_tx_ch = 3; CHECK(aprs_tx_side() == 1);
        host_rx_ch = 1; CHECK(aprs_rx_side() == 0);
    }


    /* Quiet listening: receive audio on, speaker off, after each side change too. */
    {
        af_mode = 9; speaker_on = 1; quiets = 0;
        aprs_quiet();
        CHECK(af_mode == 1 && speaker_on == 0 && quiets == 1 && rf30 == 0xBFF5);
        host_side = 0; host_route = 0; speaker_on = 1;
        aprs_side(1);
        CHECK(speaker_on == 0 && quiets == 2);
        aprs_side(1);
        CHECK(quiets == 2);                                /* no change, no writes */
    }

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
