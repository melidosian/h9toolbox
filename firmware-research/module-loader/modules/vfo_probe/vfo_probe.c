/* VFO PROBE (research, read-only): where does stock keep the tuned
 * frequencies?
 *
 * Static analysis of stock 1.0.38 (resource-flash-log.md, 2026-11-10):
 * the only writer of the RF chip's frequency registers 0x38/0x39 is
 * 0x01E59094, called by the receive tune 0x01E5942E (frequency from the u32
 * at 0x11AD0+0x1BC) and the transmit tune 0x01E5AD68 (from +0x1BC or
 * +0x1CC, then shifted by the offset at +0x1C8 in direction +0x789).
 * Frequencies are in 10 Hz units.
 *
 * Page 1 shows those fields, then stock's GPS fix (valid flag, latitude and
 * longitude in microdegrees from its NMEA parser; the loader stub records
 * it in the background). Page 2 scans RAM below the module slot for
 * 32-bit words that look like 2 m / 70 cm frequencies (136-174, 400-520 MHz)
 * and lists their addresses: set VFO A and B to distinct frequencies first
 * (e.g. 145.800 and 435.000), then open this module. Writes nothing.
 * Keys: Menu switches page, Up/Down scroll the scan, Back leaves. */
#include <stdint.h>

#include "gfx.h"
#include "h9mod.h"

#define BASE 0x11AD0u
#define RAM8(o) (*(const volatile uint8_t *)(BASE + (o)))
#define RAM32(o) (*(const volatile uint32_t *)(BASE + (o)))
#define SCAN_END 0x29F00u  /* the module slot starts here */

enum { MAX_HITS = 96, ROWS = 10 };

static char *put(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *num(char *d, uint32_t v, unsigned width)
{
    char t[10];
    unsigned n = 0;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
    while (n < width) t[n++] = '0';
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}
static char *hex(char *d, uint32_t v, unsigned digits)
{
    while (digits--) { unsigned x = (v >> (digits * 4)) & 15; *d++ = (char)(x < 10 ? '0' + x : 'A' + x - 10); }
    *d = 0;
    return d;
}
/* 10 Hz units -> "145.80000" (or the hex word if out of range) */
static char *mhz(char *d, uint32_t v)
{
    if (v > 99999999u) return hex(d, v, 8);
    d = num(d, v / 100000u, 1);
    *d++ = '.';
    return num(d, v % 100000u, 5);
}
/* signed microdegrees -> "-87.630000" */
static char *udeg(char *d, int32_t v)
{
    uint32_t m = (uint32_t)(v < 0 ? -v : v);
    if (v < 0) *d++ = '-';
    d = num(d, m / 1000000u, 1);
    *d++ = '.';
    return num(d, m % 1000000u, 6);
}
static int plausible(uint32_t v)
{
    return (v >= 13600000u && v <= 17400000u) || (v >= 40000000u && v <= 52000000u);
}

static void line(unsigned row, const char *text, uint16_t fg)
{
    gfx_text(0, 12 + row * 11, text, 26, 1, fg, GFX_BLACK);
}

static void fields(void)
{
    static const uint16_t words[] = { 0x1BC, 0x1C0, 0x1C4, 0x1C8, 0x1CC, 0x1D0 };
    char b[32], *d;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, GFX_BLACK);
    gfx_text(0, 0, "VFO PROBE 1/2 FIELDS", 20, 1, GFX_YELLOW, GFX_BLACK);
    for (unsigned i = 0; i < 6; i++) {
        d = put(b, "+"); d = hex(d, words[i], 3); d = put(d, " ");
        mhz(d, RAM32(words[i]));
        line(i, b, i == 0 ? GFX_GREEN : GFX_WHITE);
    }
    static const uint16_t bytes[] = { 0x1B, 0x1D, 0x789, 0x10CD, 0x10EA, 0x2A };
    d = b;
    for (unsigned i = 0; i < 6; i++) {
        if (i == 3) { *d = 0; line(6, b, GFX_WHITE); d = b; }
        d = hex(d, bytes[i], 3); *d++ = '='; d = hex(d, RAM8(bytes[i]), 2); *d++ = ' ';
    }
    *d = 0;
    line(7, b, GFX_WHITE);
    /* stock's GPS fix (0x11AD0 + 0xA80 valid, +0xA88 lat, +0xA90 lon) */
    d = num(b, RAM8(0xA80), 1); *d++ = ' ';
    d = udeg(d, (int32_t)RAM32(0xA88)); *d++ = ' ';
    udeg(d, (int32_t)RAM32(0xA90));
    line(8, b, GFX_YELLOW);
    line(9, "MENU SCAN  BACK EXIT", GFX_GREY);
}

static unsigned scan(uint32_t *hits)
{
    unsigned n = 0;
    for (uint32_t a = 0x100; a < SCAN_END && n < MAX_HITS; a += 4)
        if (plausible(*(const volatile uint32_t *)a)) hits[n++] = a;
    return n;
}

static void scan_page(const uint32_t *hits, unsigned n, unsigned first)
{
    char b[32], *d;
    gfx_fill(0, 0, H9_WIDTH, H9_HEIGHT, GFX_BLACK);
    d = put(b, "VFO PROBE 2/2 HITS ");
    d = num(d, n, 1);
    if (n == MAX_HITS) put(d, "+");
    gfx_text(0, 0, b, 26, 1, GFX_YELLOW, GFX_BLACK);
    for (unsigned r = 0; r < ROWS && first + r < n; r++) {
        uint32_t a = hits[first + r];
        d = hex(b, a, 5); d = put(d, " ");
        mhz(d, *(const volatile uint32_t *)a);
        line(r, b, a >= BASE && a < BASE + 0x1200 ? GFX_GREEN : GFX_WHITE);
    }
}

unsigned module_main(const struct h9_api *api, unsigned self)
{
    (void)api; (void)self;
    uint32_t hits[MAX_HITS];
    unsigned n = 0, first = 0, page = 0;
    fields();
    gfx_wait_release();
    for (;;) {
        unsigned k = gfx_wait_key();
        if (k == H9_KEY_BACK) return 0;
        if (k == H9_KEY_MENU) {
            page ^= 1;
            if (page) { n = scan(hits); first = 0; scan_page(hits, n, first); }
            else fields();
        } else if (page && k == H9_KEY_DOWN && first + ROWS < n) { first += ROWS; scan_page(hits, n, first); }
        else if (page && k == H9_KEY_UP && first) { first -= ROWS; scan_page(hits, n, first); }
        gfx_wait_release();
    }
}
