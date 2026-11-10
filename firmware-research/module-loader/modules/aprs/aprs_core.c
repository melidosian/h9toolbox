/* APRS client core: see aprs_core.h. No globals (the loader copies code
 * only); everything lives in the shared block A. */
#include "aprs_core.h"

#ifndef RING
#define RING ((volatile uint8_t *)0x16634)
#define RING_WRITE (*(volatile uint16_t *)0x16E34)
#define RING_READ (*(volatile uint16_t *)0x16E36)
#endif
#define RING_MASK 2047u
/* Stock timer-3 counter: 100 Hz, measured on hardware with a stopwatch and
 * running while a module has the CPU (resource-flash-log.md, 2026-11-09). */
#ifndef TICK
#define TICK (*(const volatile uint32_t *)0x1D8B0)
#endif
#ifndef SIDE_BYTE
#define SIDE_BYTE (*(volatile uint8_t *)0x11B50)        /* bit 7: logical side B */
#define ROUTE_BYTE (*(volatile uint8_t *)0x11AEC)       /* Dual Watch route side */
#define RF_SETUP(x) ((void (*)(unsigned))0x01E58E3Eu)(x)
/* Stock AF selector app+0x0592E4 (RF reg 0x47; mode 1 = receive audio, as
 * stock receive setup uses) and speaker writer app+0x052228 (GPIO 3, state
 * in 0x11B4D bit 0). Stock's squelch handler 0x01E7B2A4 normally drives
 * both from the main loop, which a module pauses. */
#define AF_SELECT(m) ((void (*)(unsigned))0x01E593E4u)(m)
#define SPEAKER(on) ((void (*)(unsigned))0x01E52328u)(on)
/* Stock RF register write app+0x05060A (reg, 16-bit value). Register 0x30
 * bit 9 enables the RF chip's audio DAC: receive setup writes 0xBDF1 (off),
 * stock's squelch handler writes 0xBFF5 when squelch opens (0x01E7B7C6) and
 * 0xBDF5 when it closes. Without it AF mode 1 alone gives no audio. */
#define RF_WRITE(r, v) ((void (*)(unsigned, unsigned))0x01E5070Au)((r), (v))
/* Battery save: stock 0x01E7BAF8 sets 0x11B01 = 1 and powers the RF chip
 * down (0x30 = 0, 0x37 = 0x1D00); 0x01E5969C clears it, powers up
 * (0x37 = 0x1D0F) and redoes receive setup. The main loop that wakes it is
 * paused while a module runs. */
#define RF_ASLEEP (*(volatile uint8_t *)0x11B01)
#define RF_WAKE() ((void (*)(void))0x01E5969Cu)()
#define SPEAKER_STATE (*(volatile uint8_t *)0x11B4D)
#endif
/* APRS settings (CHIRP offsets + 0x941C, h9-firmware-research APRS_CODE_MAP.md):
 * RX CH 0 Off, 1 CH-A, 2 CH-B; TX CH 0 A-Lock, 1 B-Lock, 2 A-Auto, 3 B-Auto, 4 A+B. */
#ifndef RX_CH
#define RX_CH (*(const volatile uint8_t *)0xC532)
#define TX_CH (*(const volatile uint8_t *)0xC533)
#endif
#ifndef MYCALL
#define MYCALL ((const volatile uint8_t *)0xC4A1)
#endif

enum { INFO_MAX = 256 };
/* Hold Menu 0.6 s = a hold; the RX counter shows green for 1 s per packet. */
enum { HOLD_TICKS = 60, HELD = 0xFFFF, RX_FLASH_TICKS = 100 };
/* last_key after opening: the key that got us here may still be down. */
enum { KEY_WAIT = 0xFE, TAP_TICKS = 3 };

_Static_assert(APRS_BYTES <= APRS_SLOT_BYTES, "shared APRS state must fit below the mailbox");

/* ---- helpers --------------------------------------------------------------- */

unsigned put_uint(char *out, unsigned v)
{
    char tmp[10];
    unsigned n = 0, i = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) out[i++] = tmp[--n];
    out[i] = 0;
    return i;
}

unsigned put_str(char *out, const char *s)
{
    unsigned i = 0;
    while (s[i]) { out[i] = s[i]; i++; }
    out[i] = 0;
    return i;
}

static int same(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

#ifndef APRS_NO_RX
static void copy(char *dst, const char *src, unsigned max)
{
    unsigned i = 0;
    for (; src[i] && i < max - 1; i++) dst[i] = src[i];
    dst[i] = 0;
}
#endif

static void clear(void *p, unsigned n)
{
    uint8_t *b = p;
    while (n--) *b++ = 0;
}

#ifndef APRS_NO_RX   /* receive parsing (left out of APRS SEND, which does not read the ring) */
static int digit(uint8_t c) { return c >= '0' && c <= '9'; }
static unsigned dval(uint8_t c) { return digit(c) ? c - '0' : 0; }  /* ambiguity spaces count as 0 */

static unsigned isqrt(unsigned v)
{
    unsigned r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
        bit >>= 2;
    }
    return r;
}

/* ---- positions ------------------------------------------------------------- */

/* "DDMM.mmN" / "DDDMM.mmW" (deg_digits 2 or 3) to hundredths of a minute. */
static int parse_coord(const uint8_t *p, unsigned deg_digits, int32_t *out)
{
    unsigned deg = 0;
    for (unsigned i = 0; i < deg_digits; i++) deg = deg * 10 + dval(p[i]);
    p += deg_digits;
    if (p[2] != '.') return 0;
    unsigned v = deg * 6000 + (dval(p[0]) * 10 + dval(p[1])) * 100 + dval(p[3]) * 10 + dval(p[4]);
    uint8_t h = p[5] | 0x20;
    if (h == 's' || h == 'w') *out = -(int32_t)v;
    else if (h == 'n' || h == 'e') *out = (int32_t)v;
    else return 0;
    return 1;
}

/* Position at p (uncompressed or compressed); returns bytes used, 0 if none. */
static unsigned parse_position(struct station *s, const uint8_t *p, unsigned left)
{
    if (left >= 19 && (digit(p[0]) || p[0] == ' ')) {
        if (!parse_coord(p, 2, &s->lat) || !parse_coord(p + 9, 3, &s->lon)) return 0;
        s->table = p[8]; s->symbol = p[18]; s->has_pos = 1;
        return 19;
    }
    if (left >= 13) {   /* compressed: table, 4 base-91 lat, 4 lon, symbol */
        unsigned y = 0, x = 0;
        for (unsigned i = 1; i <= 4; i++) {
            if (p[i] < 33 || p[i] > 124 || p[i + 4] < 33 || p[i + 4] > 124) return 0;
            y = y * 91 + (p[i] - 33);
            x = x * 91 + (p[i + 4] - 33);
        }
        /* lat = 90 - y/380926 deg, lon = -180 + x/190463 deg: whole degrees,
         * then the remainder (r * 6000 stays within 32 bits) */
        s->lat = 540000 - (int32_t)(y / 380926u * 6000u + y % 380926u * 6000u / 380926u);
        s->lon = (int32_t)(x / 190463u * 6000u + x % 190463u * 6000u / 190463u) - 1080000;
        s->table = p[0]; s->symbol = p[9]; s->has_pos = 1;
        return 13;
    }
    return 0;
}

/* Mic-E: latitude and flags in the destination, longitude in the info. */
static int parse_mic_e(struct station *s, const char *dest, const uint8_t *info, unsigned len)
{
    unsigned d[6];
    if (len < 9) return 0;
    for (unsigned i = 0; i < 6; i++) {
        char c = dest[i];
        if (c >= '0' && c <= '9') d[i] = (unsigned)(c - '0');
        else if (c >= 'A' && c <= 'J') d[i] = (unsigned)(c - 'A');
        else if (c >= 'P' && c <= 'Y') d[i] = (unsigned)(c - 'P');
        else if (c == 'K' || c == 'L' || c == 'Z') d[i] = 0;
        else return 0;
    }
    int32_t lat = (int32_t)((d[0] * 10 + d[1]) * 6000 + (d[2] * 10 + d[3]) * 100 + d[4] * 10 + d[5]);
    int deg = info[1] - 28 + (dest[4] >= 'P' ? 100 : 0);
    if (deg >= 180 && deg <= 189) deg -= 80;
    else if (deg >= 190 && deg <= 199) deg -= 190;
    int min = info[2] - 28;
    if (min >= 60) min -= 60;
    int hun = info[3] - 28;
    if (deg < 0 || deg > 179 || min < 0 || hun < 0 || hun > 99) return 0;
    int32_t lon = deg * 6000 + min * 100 + hun;
    s->lat = dest[3] >= 'P' ? lat : -lat;
    s->lon = dest[5] >= 'P' ? -lon : lon;
    s->symbol = info[7]; s->table = info[8]; s->has_pos = 1;
    return 1;
}

/* ---- stations and messages --------------------------------------------- */

/* Call from a 7-byte address: raw AX.25 (chars shifted left) or stock's
 * decoded form (ASCII, 0-trimmed); SSID in bits 1-4 or 0-3. */
void aprs_call_of(const uint8_t *a, int raw, char *out)
{
    unsigned n = 0;
    for (unsigned i = 0; i < 6; i++) {
        uint8_t c = raw ? a[i] >> 1 : a[i];
        if (!c || c == ' ') break;
        out[n++] = (char)c;
    }
    unsigned ssid = (raw ? a[6] >> 1 : a[6]) & 15;
    if (ssid) {
        out[n++] = '-';
        if (ssid >= 10) { out[n++] = '1'; ssid -= 10; }
        out[n++] = (char)('0' + ssid);
    }
    out[n] = 0;
}

/* Find or make the station `call`. live: move it to the front (newest);
 * stored: append the first (newest) record, flag later ones as older. */
static struct station *station(const char *call, int live)
{
    unsigned at = A->count;
    for (unsigned i = 0; i < A->count; i++)
        if (same(A->st[i].call, call)) { at = i; break; }
    A->older = 0;
    if (!live) {
        if (at < A->count) { A->older = 1; return &A->st[at]; }
        if (A->count == STATIONS) return 0;
        struct station *s = &A->st[A->count++];
        clear(s, sizeof *s);
        copy(s->call, call, CALL);
        return s;
    }
    struct station keep;
    if (at < A->count) keep = A->st[at];
    else {
        clear(&keep, sizeof keep);
        copy(keep.call, call, CALL);
        if (A->count < STATIONS) A->count++;
        at = A->count - 1;                       /* drops the oldest when full */
    }
    for (unsigned i = at; i > 0; i--) A->st[i] = A->st[i - 1];
    A->st[0] = keep;
    /* keep the list highlight on the same station */
    if (A->list_sel == at) A->list_sel = 0;
    else if (A->list_sel < at) A->list_sel++;
    if (A->list_sel >= A->count) A->list_sel = A->count - 1;
    return &A->st[0];
}

/* ":ADDRESSEE:text{id" (acks and rejects skipped, retries merged). */
static void message(const char *from, const uint8_t *info, unsigned len, int live)
{
    char to[CALL], text[MSG_TEXT];
    unsigned n = 0;
    if (len < 12 || info[10] != ':') return;
    for (unsigned i = 1; i <= 9 && n < CALL - 1; i++) if (info[i] != ' ') to[n++] = (char)info[i];
    to[n] = 0;
    const uint8_t *t = info + 11;
    len -= 11;
    if (len >= 3 && ((t[0] == 'a' && t[1] == 'c' && t[2] == 'k') || (t[0] == 'r' && t[1] == 'e' && t[2] == 'j'))) return;
    for (n = 0; n < len && n < MSG_TEXT - 1 && t[n] != '{'; n++) text[n] = t[n] >= ' ' && t[n] <= '~' ? (char)t[n] : ' ';
    text[n] = 0;
    for (unsigned i = 0; i < A->msgs; i++)
        if (same(A->msg[i].from, from) && same(A->msg[i].text, text)) return;
    if (!live && A->msgs == MESSAGES) return;      /* stored: keep the newest */
    unsigned at = live ? 0 : A->msgs;
    if (live) for (unsigned i = MESSAGES - 1; i > 0; i--) A->msg[i] = A->msg[i - 1];
    if (A->msgs < MESSAGES) A->msgs++;
    struct message *m = &A->msg[at];
    copy(m->from, from, CALL);
    copy(m->to, to, CALL);
    copy(m->text, text, MSG_TEXT);
    m->live = (uint8_t)live;
    m->mine = A->mycall[0] && same(to, A->mycall);
    if (m->mine && live) A->unread++;
    if (live && A->msg_sel) A->msg_sel = A->msg_sel + 1 < A->msgs ? A->msg_sel + 1 : A->msgs - 1;
}

/* One packet: decoded destination (6 ASCII), source call, info field. */
void aprs_packet(const char *dest, const char *from, const uint8_t *info, unsigned len, int live)
{
    char call[CALL];
    unsigned at = 0, used = 0;
    struct station tmp;
    if (!len || len > INFO_MAX) return;
    copy(call, from, CALL);
    clear(&tmp, sizeof tmp);
    uint8_t type = info[0];
    if (type == ':') message(from, info, len, live);
    if (type == '!' || type == '=') used = parse_position(&tmp, info + 1, len - 1), at = 1;
    else if ((type == '/' || type == '@') && len > 8) used = parse_position(&tmp, info + 8, len - 8), at = 8;
    else if (type == ';' && len > 18) {             /* object: name[9] * or _, time[7] */
        unsigned n = 0;
        for (unsigned i = 1; i <= 9 && n < CALL - 1; i++) if (info[i] != ' ') call[n++] = (char)info[i];
        call[n] = 0;
        used = parse_position(&tmp, info + 18, len - 18), at = 18;
    } else if (type == ')') {                       /* item: name ! or _ */
        unsigned i = 1, n = 0;
        while (i < len && i < 11 && info[i] != '!' && info[i] != '_') { if (n < CALL - 1) call[n++] = (char)info[i]; i++; }
        call[n] = 0;
        if (i < len) used = parse_position(&tmp, info + i + 1, len - i - 1), at = i + 1;
    } else if (type == '`' || type == '\'' || type == 0x1c || type == 0x1d) {
        if (parse_mic_e(&tmp, dest, info, len)) used = 8, at = 1;
    }

    struct station *s = station(call, live);
    if (!s) return;
    A->dirty = 1;
    if (A->older) {                                 /* fill a missing position only */
        if (tmp.has_pos && !s->has_pos) { s->lat = tmp.lat; s->lon = tmp.lon; s->table = tmp.table; s->symbol = tmp.symbol; s->has_pos = 1; }
        return;
    }
    if (tmp.has_pos) { s->lat = tmp.lat; s->lon = tmp.lon; s->table = tmp.table; s->symbol = tmp.symbol; s->has_pos = 1; }
    s->live = (uint8_t)live;
    unsigned from_at = tmp.has_pos ? at + used : 0, n = 0;
    for (unsigned i = from_at; i < len && n < TEXT - 1; i++) {
        char c = (char)info[i];
        s->text[n++] = c >= ' ' && c <= '~' ? c : ' ';
    }
    s->text[n] = 0;
}

/* ---- live: ring, AX.25 frames, GPS ------------------------------------- */

static void ax25(const uint8_t *f, unsigned len)
{
    unsigned at = 14;
    char dest[7], from[CALL];
    if (len < 16) return;
    while (!(f[at - 1] & 1)) {                      /* up to 8 digipeaters */
        if (at + 7 > len || at >= 70) return;
        at += 7;
    }
    if (at + 2 > len) return;
    for (unsigned i = 0; i < 6; i++) dest[i] = (char)(f[i] >> 1);
    dest[6] = 0;
    aprs_call_of(f + 7, 1, from);
    at += 2;                                        /* control, PID */
    A->rx++;
    A->rx_flash = TICK | 1;                         /* nonzero: flashing since then */
    aprs_packet(dest, from, f + at, len - at > INFO_MAX ? INFO_MAX : len - at, 1);
    A->dirty = 1;
}

/* $G?RMC,time,A,ddmm.mmmm,N,dddmm.mmmm,W,... */
static void nmea_line(void)
{
    const char *f[8];
    unsigned n = 0;
    char *p = A->nmea;
    A->gps_seen = 1;
    if (A->nmea_len < 6 || p[3] != 'R' || p[4] != 'M' || p[5] != 'C') return;
    for (unsigned i = 0; i < A->nmea_len && n < 8; i++)
        if (p[i] == ',') { p[i] = 0; f[n++] = p + i + 1; }
    if (n < 7) return;
    unsigned fix = f[1][0] == 'A';
    const char *la = f[2], *lo = f[4];
    if (fix && la[4] == '.' && lo[5] == '.') {      /* ddmm.mm, first two decimals */
        uint8_t lat_s[8] = { (uint8_t)la[0], (uint8_t)la[1], (uint8_t)la[2], (uint8_t)la[3], '.', (uint8_t)la[5], (uint8_t)la[6], (uint8_t)f[3][0] };
        uint8_t lon_s[9] = { (uint8_t)lo[0], (uint8_t)lo[1], (uint8_t)lo[2], (uint8_t)lo[3], (uint8_t)lo[4], '.', (uint8_t)lo[6], (uint8_t)lo[7], (uint8_t)f[5][0] };
        fix = parse_coord(lat_s, 2, &A->my_lat) && parse_coord(lon_s, 3, &A->my_lon);
    } else fix = 0;
    if (fix != A->my_fix) A->dirty = 1;
    A->my_fix = (uint8_t)fix;
}

static void gps_bytes(const uint8_t *p, unsigned len)
{
    for (unsigned i = 0; i < len; i++) {
        char c = (char)p[i];
        if (c == '$') A->nmea_len = 0;
        if (c == '\r' || c == '\n') { if (A->nmea_len) { A->nmea[A->nmea_len] = 0; nmea_line(); } A->nmea_len = 0; continue; }
        if (A->nmea_len < NMEA_MAX - 1) A->nmea[A->nmea_len++] = c;
    }
}

void aprs_feed(uint8_t b)
{
    if (b == 0xC0) {
        if (A->length) {
            if (A->frame[0] == 0x00) ax25(A->frame + 1, A->length - 1);
            else if (A->frame[0] == 0x10) gps_bytes(A->frame + 1, A->length - 1);
        }
        A->length = 0; A->escape = 0;
        return;
    }
    if (A->escape) { b = b == 0xDC ? 0xC0 : b == 0xDD ? 0xDB : b; A->escape = 0; }
    else if (b == 0xDB) { A->escape = 1; return; }
    if (A->length < FRAME_MAX) A->frame[A->length++] = b;
}

#endif /* APRS_NO_RX */

static void poll_ring(void)
{
#ifndef APRS_NO_RX
    unsigned write = RING_WRITE & RING_MASK, read = RING_READ & RING_MASK;
    while (read != write) { aprs_feed(RING[read]); read = (read + 1) & RING_MASK; }
    RING_READ = (uint16_t)read;
#endif
}

/* ---- open, events, switching ------------------------------------------ */

void aprs_reset(void)
{
    clear(A, sizeof *A);
    A->magic = APRS_MAGIC;
    A->map_zoom = 3;
}

void aprs_side(unsigned side)
{
    if (side > 1 || ((SIDE_BYTE >> 7) == side && ROUTE_BYTE == side)) return;
    SIDE_BYTE = (uint8_t)((SIDE_BYTE & 0x7F) | side << 7);
    ROUTE_BYTE = (uint8_t)side;
    RF_SETUP(0);
    aprs_quiet();
}

/* Squelch open (receive audio path on) and the speaker muted. Test builds
 * made with -DAPRS_SPEAKER_TEST leave the speaker on, so the open squelch
 * can be heard as hiss (build/test-aprs-speaker-bundle.bin). */
void aprs_quiet(void)
{
    if (RF_ASLEEP) RF_WAKE();
    AF_SELECT(1);
    RF_WRITE(0x30, 0xBFF5);                     /* audio DAC on: squelch open */
#ifdef APRS_SPEAKER_TEST
    SPEAKER(1);
#else
    SPEAKER(0);
#endif
}

unsigned aprs_rx_side(void) { return RX_CH == 1 ? 0 : RX_CH == 2 ? 1 : 2; }
unsigned aprs_tx_side(void) { return TX_CH < 4 ? TX_CH & 1 : 2; }

int aprs_open(void)
{
    int fresh = 0;
    if (A->magic != APRS_MAGIC) aprs_reset();
    if (A->switching) {
        A->switching = 0;
    } else {
        /* Fresh start: stock drained the ring meanwhile, so drop partial frames. */
        fresh = 1;
        A->length = A->escape = A->nmea_len = 0;
        A->home_side = SIDE_BYTE >> 7;
        A->home_speaker = SPEAKER_STATE & 1;
        aprs_side(aprs_rx_side());                  /* listen on the APRS RX channel */
        aprs_quiet();
        unsigned n = 0;
        for (unsigned i = 0; i < 6; i++) {
            uint8_t c = MYCALL[i];
            if (c <= ' ' || c > '~') break;
            A->mycall[n++] = (char)c;
        }
        unsigned ssid = MYCALL[6];
        if (n && ssid && ssid <= 15) {
            A->mycall[n++] = '-';
            if (ssid >= 10) { A->mycall[n++] = '1'; ssid -= 10; }
            A->mycall[n++] = (char)('0' + ssid);
        }
        A->mycall[n] = 0;
    }
    A->dirty = 1;
    A->last_key = KEY_WAIT;       /* ignore keys until all are released */
    A->held = HELD;
    return fresh;
}

unsigned aprs_event(void)
{
    for (;;) {
        poll_ring();
        if (A->rx_flash && (uint32_t)(TICK - A->rx_flash) >= RX_FLASH_TICKS) { A->rx_flash = 0; A->dirty = 1; }
        if (A->dirty) { A->dirty = 0; return EV_REDRAW; }
        h9_delay(2);
        /* The keypad is a resistor ladder read by the ADC once per call, and
         * a reading near 0 counts as Menu: a glitch (seen when the speaker
         * starts) is a few ms of Menu. A tap must last TAP_TICKS. */
        unsigned key = h9_key(), prev = A->last_key;
        if (prev == KEY_WAIT) { if (key == H9_KEY_NONE) A->last_key = key; continue; }
        A->last_key = key;
        if (key == H9_KEY_MENU) {
            if (prev != H9_KEY_MENU) { A->held = 0; A->press_tick = TICK; }
            if (A->held == HELD) continue;                         /* already handled */
            A->held++;
            if ((uint32_t)(TICK - A->press_tick) >= HOLD_TICKS) {
                A->held = HELD;
                return EV_HOLD;
            }
            continue;
        }
        if (prev == H9_KEY_MENU && A->held != HELD && (uint32_t)(TICK - A->press_tick) >= TAP_TICKS)
            return EV_MENU;                                        /* short press, released */
        if (key == prev || key == H9_KEY_NONE) continue;
        if (key == H9_KEY_UP) return EV_UP;
        if (key == H9_KEY_DOWN) return EV_DOWN;
        if (key == H9_KEY_BACK) return EV_BACK;
        if (key >= 2 && key <= 11) return EV_DIGIT + key - 2;
    }
}

int aprs_go(const struct h9_api *api, const char *name)
{
    uint8_t head[16];
    char entry[H9_NAME_BYTES + 1];
    api->read(head, api->bundle, sizeof head);
    unsigned count = head[10] | (unsigned)head[11] << 8;
    entry[H9_NAME_BYTES] = 0;
    for (unsigned i = 1; i < count && i < 64; i++) {
        api->read(entry, api->bundle + H9_BUNDLE_HEADER + H9_BUNDLE_ENTRY * i, H9_NAME_BYTES);
        if (!same(entry, name)) continue;
        volatile struct h9_request *rq = H9_REQUEST;
        for (unsigned k = 0; k < H9_NAME_BYTES; k++) rq->name[k] = entry[k];
        rq->magic = H9_REQUEST_MAGIC;
        A->switching = 1;
        return 1;
    }
    return 0;
}

/* ---- distance, bearing, drawing ---------------------------------------- */
#ifndef APRS_NO_RX

static const uint16_t COS10[10] = { 1024, 1008, 962, 887, 784, 658, 512, 350, 178, 0 };
static const char *const POINTS[16] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };

void aprs_offset(int32_t lat, int32_t lon, int32_t *east, int32_t *north)
{
    int32_t dlon = lon - A->my_lon;
    if (dlon > 1080000) dlon -= 2160000;
    if (dlon < -1080000) dlon += 2160000;
    unsigned alat = (unsigned)(A->my_lat < 0 ? -A->my_lat : A->my_lat) / 6000;   /* degrees */
    if (alat > 89) alat = 89;
    int32_t c = COS10[alat / 10] - (COS10[alat / 10] - COS10[alat / 10 + 1]) * (int32_t)(alat % 10) / 10;
    *east = dlon / 4 * c / 256;                      /* dlon * c / 1024 without overflow */
    *north = lat - A->my_lat;
}

/* Distance in tenths of a km, and a 16-point bearing; 0 if unknown. */
int aprs_range(const struct station *s, unsigned *tenths_km, const char **point)
{
    int32_t e, n;
    if (!s->has_pos || !A->my_fix) return 0;
    aprs_offset(s->lat, s->lon, &e, &n);
    unsigned ax = (unsigned)(e < 0 ? -e : e), ay = (unsigned)(n < 0 ? -n : n), x = ax, y = ay, shift = 0;
    while (x > 30000 || y > 30000) { x >>= 1; y >>= 1; shift++; }
    *tenths_km = (isqrt(x * x + y * y) << shift) * 1852u / 10000u;   /* 18.52 m per unit */
    /* angle from north in the quadrant: tan(11.25 deg) = 0.199, tan(33.75 deg) = 0.668 */
    unsigned sector;
    if (ax * 1024 < ay * 204) sector = 0;
    else if (ax * 1024 < ay * 684) sector = 1;
    else if (ay * 1024 < ax * 204) sector = 4;
    else if (ay * 1024 < ax * 684) sector = 3;
    else sector = 2;
    unsigned b = n >= 0 ? (e >= 0 ? sector : 16 - sector) : (e >= 0 ? 8 - sector : 8 + sector);
    *point = POINTS[b & 15];
    return 1;
}

unsigned put_range(char *out, const struct station *s)
{
    unsigned t;
    const char *pt;
    if (!aprs_range(s, &t, &pt)) return put_str(out, s->has_pos ? "" : "NO POS");
    unsigned n;
    if (t < 1000) { n = put_uint(out, t / 10); out[n++] = '.'; out[n++] = (char)('0' + t % 10); }
    else n = put_uint(out, t / 10);
    n += put_str(out + n, "KM ");
    return n + put_str(out + n, pt);
}

#endif /* APRS_NO_RX */

void aprs_line(unsigned y, const char *text, uint16_t fg, uint16_t bg)
{
    unsigned x = gfx_text(4, y, text, 26, 1, fg, bg);
    gfx_fill(0, y, 4, 8, bg);
    if (x < H9_WIDTH) gfx_fill(x, y, H9_WIDTH - x, 8, bg);
}

void aprs_title(const char *text)
{
    char b[12];
    gfx_fill(0, 0, H9_WIDTH, TITLE_H, GFX_BLUE);
    gfx_text(4, 1, text, 8, 2, GFX_WHITE, GFX_BLUE);
    unsigned n = put_str(b, "RX ");
    put_uint(b + n, A->rx);
    gfx_text(112, 1, b, 8, 1, A->rx_flash ? GFX_GREEN : GFX_WHITE, GFX_BLUE);
    gfx_text(112, 10, A->my_fix ? "GPS OK" : A->gps_seen ? "NO FIX" : "NO GPS", 8, 1,
             A->my_fix ? GFX_GREEN : GFX_YELLOW, GFX_BLUE);
}
