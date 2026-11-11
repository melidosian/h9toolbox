/* Host harness for maze.c: runs the game against a fake LCD, clock and keys
 * (a random walker), checks it plays through a game without trouble and
 * writes frames as PPM.   cc ... render_maze.c ../common/gfx.c  &&  ./a.out out.ppm seconds seed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "h9mod.h"

static volatile uint32_t host_tick;
#define TICK host_tick
/* The sound driver against fake stock calls: every sample "sent to the DAC" is kept. */
static int16_t host_pcm[1 << 22];
static unsigned host_pcm_n, host_gate_on, host_started, host_rate;
#define SND_HOST
#define SND_TICK host_tick
#define DAC_HANDLE ((void *)0)
#define DAC_RATE(h, r) (host_rate = (r))
#define DAC_START(h) (host_started = 1)
#define DAC_WRITE(p) do { if (host_pcm_n + 64 <= sizeof host_pcm / 2) { memcpy(host_pcm + host_pcm_n, (p), 128); host_pcm_n += 64; } } while (0)
#define SPEAKER_GATE(source, on) (host_gate_on = (on))
#define PA3_ON() do { } while (0)
#include "snd.c"
#include "maze.c"

static uint16_t fb[160 * 128];
static unsigned wx0, wy0, wx1, wy1, wcur;
static unsigned limit, seed = 1, shot_every;
static const char *out;
static unsigned shots;

void h9_lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
    if (x1 >= 160 || y1 >= 128 || x0 > x1 || y0 > y1) { fprintf(stderr, "bad window %u %u %u %u\n", x0, y0, x1, y1); abort(); }
    wx0 = x0; wy0 = y0; wx1 = x1; wy1 = y1; wcur = 0;
}
void h9_lcd_write(const void *buffer, unsigned length)
{
    const uint8_t *p = buffer;
    for (unsigned i = 0; i + 1 < length; i += 2, wcur++) {
        unsigned w = wx1 - wx0 + 1, x = wx0 + wcur % w, y = wy0 + wcur / w;
        if (y > wy1) { fprintf(stderr, "write past window\n"); abort(); }
        fb[y * 160 + x] = (uint16_t)(p[i] << 8 | p[i + 1]);
    }
}
static void shot(const char *name)
{
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6\n160 128\n255\n");
    for (unsigned i = 0; i < 160 * 128; i++) {
        unsigned c = fb[i];
        fputc((int)((c >> 11) * 255 / 31), f); fputc((int)(((c >> 5) & 63) * 255 / 63), f); fputc((int)((c & 31) * 255 / 31), f);
    }
    fclose(f);
}
static unsigned rng_state = 12345;
static unsigned rnd_host(void) { rng_state = rng_state * 1103515245u + 12345u; return rng_state >> 16; }
static unsigned phase_key = 0;
unsigned h9_key(void)
{
    if (host_tick >= limit) return (host_tick & 1) ? H9_KEY_BACK : H9_KEY_NONE;   /* a fresh press each time */
    if (host_tick & 1) return H9_KEY_NONE;   /* taps, so every press is new */
    if (host_tick % 25 == 0) phase_key = rnd_host() % 5;
    static const unsigned keys[5] = { H9_KEY_2, H9_KEY_4, H9_KEY_5 + 1, H9_KEY_5 + 3, H9_KEY_UP };
    return keys[phase_key];
}
void h9_delay(unsigned units)
{
    host_tick += 1;
    (void)units;
    if (shot_every && host_tick % shot_every == 0) { char n[256]; snprintf(n, sizeof n, "%s.%u.ppm", out, shots++); shot(n); }
}
void h9_led(unsigned on) { (void)on; }

int main(int argc, char **argv)
{
    out = argc > 1 ? argv[1] : "maze";
    limit = argc > 2 ? (unsigned)atoi(argv[2]) * 100 : 6000;
    seed = argc > 3 ? (unsigned)atoi(argv[3]) : 1;
    shot_every = argc > 4 ? (unsigned)atoi(argv[4]) : 0;
    rng_state = seed;
    host_tick = 1000 + seed;
    /* the title screen waits for a key: gfx_wait_key sees the change below */
    unsigned r = module_main(0, 0);
    printf("returned %u at tick %u\n", r, (unsigned)host_tick);
    char n[256]; snprintf(n, sizeof n, "%s.final.ppm", out); shot(n);
    /* sound: rate, amplifier released, samples written, the first tone's frequency */
    unsigned first = 0, nonzero = 0, peak = 0;
    for (unsigned i = 0; i < host_pcm_n; i++) { if (host_pcm[i]) { if (!nonzero) first = i; nonzero++; } unsigned a = (unsigned)(host_pcm[i] < 0 ? -host_pcm[i] : host_pcm[i]); if (a > peak) peak = a; }
    unsigned cross = 0, span = 800;
    for (unsigned i = first + 1; i < first + span && i < host_pcm_n; i++) if ((host_pcm[i] < 0) != (host_pcm[i - 1] < 0)) cross++;
    printf("sound: rate %u started %u gate %u samples %u (%.1f s) nonzero %u peak %u first tone ~%u Hz\n", host_rate, host_started, host_gate_on,
           host_pcm_n, host_pcm_n / 11025.0, nonzero, peak, cross * 11025 / 2 / span);
    snprintf(n, sizeof n, "%s.wav", out);
    FILE *f = fopen(n, "wb");
    uint32_t bytes = host_pcm_n * 2, rate = 11025, br = 22050, riff = 36 + bytes, fmt = 16; uint16_t pcm = 1, ch = 1, ba = 2, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
    fwrite(host_pcm, 2, host_pcm_n, f); fclose(f);
    return 0;
}
