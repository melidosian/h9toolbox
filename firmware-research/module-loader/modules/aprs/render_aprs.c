/* Host harness: renders one APRS screen to a PPM from made-up stations and messages.
 *   cc -DSCREEN='"aprs_list.c"' ... render_aprs.c ../common/gfx.c && ./a.out out.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#define APRS_HOST_TEST
#include "h9mod.h"
#include "gfx.h"

static struct h9_request host_request;
#undef H9_REQUEST
#define H9_REQUEST (&host_request)
static uint16_t fb[160 * 128];
static unsigned wx0, wx1, wy0, wy1, wcur;
void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1) { wx0 = x0; wy0 = y0; wx1 = x1; wy1 = y1; wcur = 0; }
void h9_lcd_write(const void *buffer, unsigned length)
{
    const uint8_t *p = buffer;
    for (unsigned i = 0; i + 1 < length; i += 2, wcur++) {
        unsigned w = wx1 - wx0 + 1, x = wx0 + wcur % w, y = wy0 + wcur / w;
        if (x < 160 && y < 128) fb[y * 160 + x] = (uint16_t)(p[i] << 8 | p[i + 1]);
    }
}
static uint32_t host_tick = 500000;
static unsigned polls;
static jmp_buf done;
static void feed_rmc(void);
void h9_delay(unsigned u) { (void)u; polls++; host_tick += 1; if (polls == 3) feed_rmc(); if (polls > 60) longjmp(done, 1); }   /* the first frame is drawn: stop */
unsigned h9_key(void) { return 0; }
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
#define RF_ASLEEP host_asleep
#define RF_WAKE() (host_asleep = 0)
#define SPEAKER_STATE host_spk
#define RX_CH host_rx_ch
#define TX_CH host_tx_ch
#define TICK host_tick
static uint8_t host_ring[2048];
static uint16_t host_write, host_read;
static uint32_t host_tx_freq = 14439000;   /* 144.3900 MHz in 10 Hz units */
#define TX_FREQ host_tx_freq
#define RING host_ring
#define RING_WRITE host_write
#define RING_READ host_read
#include "aprs_core.c"
#include "aprs_text.c"
struct aprs aprs_host;
#define module_main screen_main
#include SCREEN
#undef module_main

static void feed_rmc(void)   /* a GPS frame once the screen is running (BEACON starts reading the ring from now) */
{
    const char *rmc = "$GNRMC,141500.00,A,4152.6800,N,08737.8000,W,12.4,275.5,111126,,,A*00\r\n";
    host_ring[host_write++ & 2047] = 0xC0; host_ring[host_write++ & 2047] = 0x10;
    for (const char *p = rmc; *p; p++) host_ring[host_write++ & 2047] = (uint8_t)*p;
    host_ring[host_write++ & 2047] = 0xC0;
}
static void fake_read(void *d, uint32_t a, uint32_t n) { (void)a; memset(d, 0xFF, n); }

static unsigned addr(uint8_t *out, const char *call, unsigned ssid, int last)
{
    for (unsigned i = 0; i < 6; i++) out[i] = (uint8_t)((i < strlen(call) ? call[i] : ' ') << 1);
    out[6] = (uint8_t)(0x60 | ssid << 1 | (last ? 1 : 0));
    return 7;
}
static void heard(const char *src, unsigned ssid, const char *dest, const char *info)
{
    uint8_t f[300];
    unsigned n = 0, len = (unsigned)strlen(info);
    n += addr(f + n, dest, 0, 0); n += addr(f + n, src, ssid, 1);
    f[n++] = 0x03; f[n++] = 0xF0;
    memcpy(f + n, info, len); n += len;
    aprs_feed(0xC0); aprs_feed(0x00);
    for (unsigned i = 0; i < n; i++) aprs_feed(f[i]);
    aprs_feed(0xC0);
}

int main(int argc, char **argv)
{
    aprs_reset();
    strcpy(A->mycall, "N0CALL-7");
    A->my_fix = 1; A->gps_seen = 1;
    A->my_lat = 41 * 6000 + 5268; A->my_lon = -(87 * 6000 + 3780);      /* 41 52.68 N, 87 37.80 W */
    A->rx = 47; A->switching = 1;
    static const struct { const char *call; unsigned ssid; const char *info; unsigned minutes; } S[] = {
        { "W9ABC", 9, "!4157.20N/08741.50W>Mobile on the Kennedy", 0 },
        { "K9XYZ", 0, "!4152.10N/08730.00W#Lakefront digipeater", 3 },
        { "N9QRP", 1, "=4148.50N/08744.20W[Hiking, 5 W QRP", 14 },
        { "WB9LNX", 0, "!4201.30N/08735.10W-Home station", 95 },
        { "KC9DEF", 0, ">Status only, no position", 200 },
        { "W9GPS", 7, "!4150.00N/08738.60Wb Bike commute", 1 },
        { "K9REP", 0, ";146.940-  *111111z4146.00N/08728.00W", 30 },
        { "AB9CD", 0, "!4146.00N/08728.00W>", 5 },
    };
    for (unsigned i = sizeof S / sizeof S[0]; i-- > 0;) {
        heard(S[i].call, S[i].ssid, "APRS", S[i].info);
        A->st[0].heard = (uint16_t)(now10() - S[i].minutes * 6);
    }
    heard("W1AW", 0, "APRS", ":N0CALL-7 :Meet at 3pm at the park?{12");
    heard("K9XYZ", 0, "APRS", ":W9ABC-9  :Cq cq test");
    A->msgs++; for (unsigned i = A->msgs - 1; i > 0; i--) A->msg[i] = A->msg[i - 1];
    memset(&A->msg[0], 0, sizeof A->msg[0]);
    strcpy(A->msg[0].from, "N0CALL-7"); strcpy(A->msg[0].to, "W9ABC-9"); strcpy(A->msg[0].text, "On my way");
    A->msg[0].live = 1; A->msg[0].id = 5; A->msg[0].acked = 1;
    A->unread = 1; A->list_sel = 1; A->msg_sel = 1; A->map_zoom = 4; A->map_sel = 1;
    for (unsigned i = 0; i < A->count; i++) if (!strcmp(A->st[i].call, "W9ABC-9")) A->list_sel = (uint8_t)i;
    A->map_sel = A->list_sel;
    struct h9_api fake = { .read = fake_read };
    if (!setjmp(done)) screen_main(&fake, 0);
    FILE *f = fopen(argc > 1 ? argv[1] : "out.ppm", "wb");
    fprintf(f, "P6\n160 128\n255\n");
    for (unsigned i = 0; i < 160 * 128; i++) {
        unsigned c = fb[i];
        fputc((int)((c >> 11) * 255 / 31), f); fputc((int)(((c >> 5) & 63) * 255 / 63), f); fputc((int)((c & 31) * 255 / 31), f);
    }
    fclose(f);
    return 0;
}
