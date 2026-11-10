/* APRS MONITOR: first step toward an onboard APRS client.
 *
 * The APRS modem (separate Cortex-M, APRS-1.0.15) sends decoded packets and
 * GPS NMEA to the main CPU over serial, framed C0 <cmd> <payload> C0 with
 * DB DC / DB DD escapes. Stock copies the bytes into a 2048-byte ring and
 * drains it from its main loop (evidence/stock/aprs-main-uart-ring.md,
 * aprs-main-wire-receiver.asm):
 *   ring    0x16634  (2048 bytes)
 *   write   h[0x16E34]  (producer, app+0x078EFE)
 *   read    h[0x16E36]  (consumer, app+0x087218)
 *
 * While a module runs, stock's consumer does not, so this module takes its
 * place: it drains the ring, deframes, and shows what arrives. Its first job
 * is to answer whether the producer keeps running while a module has the
 * main loop. If WRITE never moves while packets are heard on the stock
 * radio, the producer runs in the blocked loop and the client needs a
 * different hook.
 *
 * Command 0x00 = received AX.25 frame (FCS removed), 0x10 = GPS NMEA bytes.
 * Read-only apart from the read index. Back returns to the menu. */
#include "h9mod.h"
#include "gfx.h"

#define RING ((volatile uint8_t *)0x16634)
#define RING_WRITE (*(volatile uint16_t *)0x16E34)
#define RING_READ (*(volatile uint16_t *)0x16E36)
#define RING_MASK 2047u

enum { FRAME_MAX = 400, HEARD = 4, LINE = 26, STATE_BYTES = 0x800 };

struct heard { char call[10]; char info[LINE + 1]; };

struct state {
    unsigned frames, gps, other, bad, bytes, deframe, escape, length;
    uint8_t frame[FRAME_MAX];
    struct heard heard[HEARD];
    unsigned next;
    char nmea[LINE + 1];
    unsigned nmea_len, nmea_on;
};

static void number(char *out, unsigned value, unsigned digits)
{
    out[digits] = 0;
    for (unsigned i = digits; i--;) {
        unsigned d = 0;
        while (value >= 10) { value -= 10; d++; }   /* value % 10 into place */
        out[i] = (char)('0' + value);
        value = d;
    }
}

/* AX.25 address (7 bytes) to "CALL-SSID"; returns characters written. */
static unsigned address(const uint8_t *a, char *out)
{
    unsigned n = 0;
    for (unsigned i = 0; i < 6; i++) {
        char c = (char)(a[i] >> 1);
        if (c != ' ' && c) out[n++] = c;
    }
    unsigned ssid = (a[6] >> 1) & 15;
    if (ssid) {
        out[n++] = '-';
        if (ssid >= 10) { out[n++] = '1'; ssid -= 10; }
        out[n++] = (char)('0' + ssid);
    }
    out[n] = 0;
    return n;
}

static void packet(struct state *s, const uint8_t *f, unsigned len)
{
    /* destination, source, up to 8 digipeaters, then control + PID. */
    unsigned at = 14;
    if (len < 16) { s->bad++; return; }
    while (!(f[at - 1] & 1) && at + 7 <= len && at < 70) at += 7;
    if (!(f[at - 1] & 1) || at + 2 > len) { s->bad++; return; }
    at += 2;
    struct heard *h = &s->heard[s->next];
    s->next = (s->next + 1) % HEARD;
    address(f + 7, h->call);
    unsigned n = 0;
    for (; n < LINE && at + n < len; n++) {
        char c = (char)f[at + n];
        h->info[n] = c >= ' ' && c <= '~' ? c : '.';
    }
    h->info[n] = 0;
    s->frames++;
}

static void gps_bytes(struct state *s, const uint8_t *p, unsigned len)
{
    for (unsigned i = 0; i < len; i++) {
        char c = (char)p[i];
        if (c == '$') { s->nmea_on = 1; s->nmea_len = 0; }
        if (!s->nmea_on) continue;
        if (c == '\r' || c == '\n') { s->nmea_on = 0; s->gps++; continue; }
        if (s->nmea_len < LINE) { s->nmea[s->nmea_len++] = c; s->nmea[s->nmea_len] = 0; }
    }
}

static void frame_done(struct state *s)
{
    if (!s->length) return;
    uint8_t cmd = s->frame[0];
    if (cmd == 0x00) packet(s, s->frame + 1, s->length - 1);
    else if (cmd == 0x10) gps_bytes(s, s->frame + 1, s->length - 1);
    else s->other++;
}

/* Deframe one byte (C0 <cmd> <payload> C0, DB DC = C0, DB DD = DB). */
static void feed(struct state *s, uint8_t b)
{
    s->bytes++;
    if (b == 0xC0) { frame_done(s); s->length = 0; s->escape = 0; return; }
    if (s->escape) { b = b == 0xDC ? 0xC0 : b == 0xDD ? 0xDB : b; s->escape = 0; }
    else if (b == 0xDB) { s->escape = 1; return; }
    if (s->length < FRAME_MAX) s->frame[s->length++] = b;
}

static void draw(const struct state *s, unsigned write, unsigned read)
{
    char a[8], b[8];
    number(a, write, 4); number(b, read, 4);
    unsigned x = gfx_text(4, 26, "WRITE ", 6, 1, GFX_GREY, GFX_BLACK);
    x = gfx_text(x, 26, a, 4, 1, GFX_WHITE, GFX_BLACK);
    x = gfx_text(x, 26, "  READ ", 7, 1, GFX_GREY, GFX_BLACK);
    gfx_text(x, 26, b, 4, 1, GFX_WHITE, GFX_BLACK);

    number(a, s->frames, 3); number(b, s->gps, 4);
    x = gfx_text(4, 36, "PKT ", 4, 1, GFX_GREY, GFX_BLACK);
    x = gfx_text(x, 36, a, 3, 1, GFX_GREEN, GFX_BLACK);
    x = gfx_text(x, 36, " GPS ", 5, 1, GFX_GREY, GFX_BLACK);
    x = gfx_text(x, 36, b, 4, 1, GFX_WHITE, GFX_BLACK);
    number(a, s->other + s->bad, 3);
    x = gfx_text(x, 36, " ?", 2, 1, GFX_GREY, GFX_BLACK);
    gfx_text(x, 36, a, 3, 1, GFX_WHITE, GFX_BLACK);

    gfx_fill(0, 47, H9_WIDTH, 1, GFX_GREY);
    for (unsigned i = 0; i < HEARD; i++) {
        const struct heard *h = &s->heard[(s->next + HEARD - 1 - i) % HEARD];
        unsigned y = 50 + i * 16;
        unsigned e = gfx_text(4, y, h->call, 10, 1, GFX_YELLOW, GFX_BLACK);
        gfx_fill(e, y, H9_WIDTH - e, 8, GFX_BLACK);
        e = gfx_text(4, y + 8, h->info, LINE, 1, GFX_WHITE, GFX_BLACK);
        gfx_fill(e, y + 8, H9_WIDTH - e, 8, GFX_BLACK);
    }
    x = gfx_text(4, 116, s->nmea, LINE, 1, GFX_GREY, GFX_BLACK);
    gfx_fill(x, 116, H9_WIDTH - x, 8, GFX_BLACK);
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    /* State lives in the 2 KiB below the request mailbox at the top of the RAM slot (the code is far
     * smaller), not on the stock task's stack, whose size is unknown. */
    struct state *ps = (struct state *)(H9_SHARED_TOP - STATE_BYTES);
    uint8_t *raw = (uint8_t *)ps;
    (void)api; (void)self;
    for (unsigned i = 0; i < sizeof *ps; i++) raw[i] = 0;
#define s (*ps)

    gfx_fill(0, 0, H9_WIDTH, 22, GFX_BLUE);
    gfx_text(8, 3, "APRS MONITOR", 12, 2, GFX_WHITE, GFX_BLUE);
    gfx_fill(0, 22, H9_WIDTH, H9_HEIGHT - 22, GFX_BLACK);
    gfx_wait_release();

    unsigned last_key = H9_KEY_NONE, last_write = 0xFFFF, tick = 0;
    for (;;) {
        unsigned write = RING_WRITE & RING_MASK, read = RING_READ & RING_MASK;
        while (read != write) {
            feed(&s, RING[read]);
            read = (read + 1) & RING_MASK;
        }
        RING_READ = (uint16_t)read;
        if (write != last_write || !(tick & 15)) { draw(&s, write, read); last_write = write; }
        tick++;
        h9_delay(2);
        unsigned key = h9_key();
        if (key != last_key && key == H9_KEY_BACK) break;
        last_key = key;
    }
    gfx_wait_release();
    return 0;
#undef s
}

_Static_assert(sizeof(struct state) <= STATE_BYTES, "state must fit the top of the RAM slot");
