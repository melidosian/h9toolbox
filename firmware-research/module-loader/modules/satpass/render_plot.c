/* Host render of .SAT PLOT to a PPM (see main for the arguments).
 * A development aid (no assertions): look at the picture. */
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

#define module_main plot_main
#include "satplot.c"

/* render_plot <lat udeg> <lon udeg> <out.ppm> <tz option> <keys: u/d/m> <sat:aos:los>... */
int main(int argc, char **argv)
{
    if (argc < 7) return 2;
    host_tz = (unsigned char)atoi(argv[4]);
    for (const char *k = argv[5]; *k; k++) host_keys[host_nkeys++] = *k == 'u' ? H9_KEY_UP : *k == 'd' ? H9_KEY_DOWN : H9_KEY_MENU;
    host_share.magic = SAT_MAGIC;
    host_share.lat = (int32_t)(((long long)atol(argv[1]) * 4294967296LL) / 360000000LL);
    host_share.lon = (int32_t)(((long long)atol(argv[2]) * 4294967296LL) / 360000000LL);
    for (int i = 6; i < argc && host_share.count < 5; i++) {
        unsigned sat; unsigned long aos, los;
        sscanf(argv[i], "%u:%lu:%lu", &sat, &aos, &los);
        host_share.aos[host_share.count] = (uint32_t)aos;
        host_share.dur_sat[host_share.count] = (uint16_t)(sat << 12 | (los - aos));
        host_share.count++;
    }
    plot_main(host_satdata_api(getenv("SATDATA") ? getenv("SATDATA") : "build/satdata.bin"), 0);
    printf("viewed pass %u, returns to: %.9s\n", host_share.sel + 1, host_request.name);
    FILE *f = fopen(argv[3], "wb");
    fprintf(f, "P6\n160 128\n255\n");
    for (int j = 0; j < 128; j++) for (int i = 0; i < 160; i++) {
        uint16_t c = fb[j][i];
        fputc((c >> 11) << 3, f); fputc(((c >> 5) & 63) << 2, f); fputc((c & 31) << 3, f);
    }
    fclose(f);
    return 0;
}
