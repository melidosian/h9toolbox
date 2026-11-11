/* Host harness: renders one page of the GPS app from made-up NMEA.
 *   cc ... render_gps.c ../common/gfx.c && ./a.out out.ppm PAGE   (0 position, 1 sky, 2 signals, 3 info) */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char host_ring[2048];
static unsigned short host_write, host_read;
static unsigned host_tick = 100000;
static unsigned char host_tz_byte = 7;   /* UTC-5 */
#define GPS_HOST_TEST
#define RING host_ring
#define RING_WRITE host_write
#define RING_READ host_read
#define TICK host_tick

#include "h9mod.h"
#include "gfx.h"
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
static unsigned polls, page = 0;
static jmp_buf done;
/* One Menu press per page (several agreeing readings), then a pause for the redraw. */
unsigned h9_key(void) { return polls < page * 8 && polls % 8 >= 2 && polls % 8 < 6 ? H9_KEY_MENU : 0; }
void h9_delay(unsigned u) { (void)u; polls++; host_tick += 1; if (polls > page * 8 + 30) longjmp(done, 1); }
int fix_load_micro(void (*r)(void *, uint32_t, uint32_t), int32_t *a, int32_t *b) { (void)r; (void)a; (void)b; return 0; }
#define module_main gps_main
#include "gps.c"

static void fake_read(void *d, uint32_t a, uint32_t n) { (void)a; memset(d, 0xFF, n); }
static void frame(const char *const *lines, unsigned n)
{
    host_ring[host_write++ & 2047] = 0xC0; host_ring[host_write++ & 2047] = 0x10;
    for (unsigned i = 0; i < n; i++) {
        for (const char *p = lines[i]; *p; p++) host_ring[host_write++ & 2047] = (unsigned char)*p;
        host_ring[host_write++ & 2047] = '\r'; host_ring[host_write++ & 2047] = '\n';
    }
    host_ring[host_write++ & 2047] = 0xC0;
}

int main(int argc, char **argv)
{
    page = argc > 2 ? (unsigned)atoi(argv[2]) : 0;
    const char *a[] = { "$GNRMC,201945.00,A,4152.6800,N,08737.8000,W,12.4,275.5,101126,,,A*00",
                        "$GNGGA,201945.00,4152.6800,N,08737.8000,W,1,11,0.8,192.3,M,-33.0,M,,*00",
                        "$GNGSA,A,3,02,05,12,25,29,31,,,,,,,1.5,0.8,1.3*00" };
    frame(a, 3);
    const char *b[] = { "$GPGSV,3,1,10,02,45,120,38,05,10,300,20,12,80,010,45,25,30,200,32*00",
                        "$GPGSV,3,2,10,29,60,040,41,31,05,250,15,15,22,070,28,18,55,330,36*00",
                        "$GPGSV,3,3,10,21,35,150,30,26,12,210,19*00",
                        "$GLGSV,2,1,06,70,20,100,33,71,40,200,28,72,65,300,40,77,10,030,17*00",
                        "$GLGSV,2,2,06,78,30,250,26,85,50,120,35*00",
                        "$GAGSV,1,1,03,04,55,170,36,09,25,260,31,13,70,090,39*00" };
    frame(b, 6);
    struct h9_api fake = { .read = fake_read };
    if (!setjmp(done)) gps_main(&fake, 0);
    FILE *f = fopen(argc > 1 ? argv[1] : "out.ppm", "wb");
    fprintf(f, "P6\n160 128\n255\n");
    for (unsigned i = 0; i < 160 * 128; i++) {
        unsigned c = fb[i];
        fputc((int)((c >> 11) * 255 / 31), f); fputc((int)(((c >> 5) & 63) * 255 / 63), f); fputc((int)((c & 31) * 255 / 31), f);
    }
    fclose(f);
    return 0;
}
