/* Host render of the SAT PASS list page to a PPM: a GPS fix and time are fed through the modem ring.
 *   render_satlist <out.ppm> <tz option>   (SATDATA=build/satdata.bin) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "h9mod.h"
#include "gfx.h"
#include "host_satdata.h"
static unsigned char host_tz = 12;   /* "UTC+0"; render_plot's 7th argument overrides it */
#define H9_TZ_BYTE host_tz
struct satshare;
extern struct satshare host_share;
#define SAT_SHARED (&host_share)
#include "satshare.h"
struct satshare host_share;

static struct h9_request host_request;
#undef H9_REQUEST
#define H9_REQUEST (&host_request)

static uint16_t fb[128][160];
void gfx_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c)
{ for (unsigned j = y; j < y + h && j < 128; j++) for (unsigned i = x; i < x + w && i < 160; i++) fb[j][i] = c; }
#include "font.h"
unsigned gfx_text(unsigned x, unsigned y, const char *t, unsigned max, unsigned scale, uint16_t fg, uint16_t bg)
{
    for (unsigned n = 0; t[n] && n < max; n++) {
        const uint8_t *g = 0;
        for (unsigned i = 0; FONT_CHARS[i]; i++) if (FONT_CHARS[i] == t[n]) g = FONT[i];
        for (unsigned r = 0; r < 8; r++) for (unsigned c = 0; c < 6; c++) {
            int on = g && r < 7 && c < 5 && (g[r] & (0x10 >> c));
            gfx_fill(x + n * 6 * scale + c * scale, y + r * scale, scale, scale, on ? fg : bg);
        }
    }
    return x + 6 * scale * max;
}
void gfx_wait_release(void) {}
static unsigned host_keys[8], host_nkeys, host_ikey;
unsigned gfx_wait_key(void) { return host_ikey < host_nkeys ? host_keys[host_ikey++] : H9_KEY_BACK; }


/* The modem ring and the stock tick, as satpass.c's overrides expect. */
static uint8_t host_ring[2048];
static uint16_t host_write, host_read;
static uint32_t host_tick = 100000;
#define RING host_ring
#define RING_WRITE host_write
#define RING_READ host_read
#define TICK host_tick
#include "fix_store.h"
int fix_load_micro(fix_read_fn read, int32_t *lat, int32_t *lon) { (void)read; (void)lat; (void)lon; return 0; }   /* nothing saved on the chip */
void fix_save(fix_read_fn read, int32_t lat, int32_t lon) { (void)read; (void)lat; (void)lon; }
void h9_delay(unsigned u) { (void)u; host_tick += 10; }
static unsigned polls;
unsigned h9_key(void) { return ++polls > 12 ? H9_KEY_BACK : 0; }
#define module_main list_main
#include "satpass.c"

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    host_tz = (unsigned char)atoi(argv[2]);
    /* one RMC frame: 2026-11-11 14:15:00 UTC, 41 52.68 N, 87 37.80 W */
    const char *rmc = "$GNRMC,141500.00,A,4152.6800,N,08737.8000,W,0.0,0.0,111126,,,A*00\r\n";
    host_ring[host_write++ & 2047] = 0xC0; host_ring[host_write++ & 2047] = 0x10;
    for (const char *p = rmc; *p; p++) host_ring[host_write++ & 2047] = (uint8_t)*p;
    host_ring[host_write++ & 2047] = 0xC0;
    list_main(host_satdata_api(getenv("SATDATA") ? getenv("SATDATA") : "build/satdata.bin"), 0);
    FILE *f = fopen(argv[1], "wb");
    fprintf(f, "P6\n160 128\n255\n");
    for (int j = 0; j < 128; j++) for (int i = 0; i < 160; i++) {
        uint16_t c = fb[j][i];
        fputc((c >> 11) << 3, f); fputc(((c >> 5) & 63) << 2, f); fputc((c & 31) << 3, f);
    }
    fclose(f);
    return 0;
}
