/* Square-wave effects through the stock DAC driver (see snd.h). Samples are
 * 11025 Hz 16-bit mono, written 64 at a time (128 bytes) through stock's DAC
 * write, about 200 ms ahead of the 100 Hz tick. Stock addresses are those
 * of the Doom firmware's shims (build.py there): app + offset + 0x100. */
#include "snd.h"

#ifndef SND_HOST
#define SND_TICK (*(volatile uint32_t *)0x1d8b0)
#define DAC_HANDLE ((void *)(0x11AD0 + 6636))
#define DAC_RATE(h, r) ((void (*)(void *, unsigned))0x01e434c8)(h, r)
#define DAC_START(h) ((void (*)(void *))0x01e43534)(h)
#define DAC_WRITE(p) ((void (*)(const void *))0x01e43698)(p)   /* the buffer only: no handle argument */
#define SPEAKER_GATE(source, on) ((void (*)(unsigned, unsigned))0x01e5234c)(source, on)
#define PA3_ON() do { volatile uint32_t *pa = (volatile uint32_t *)0x1E5000; pa[0] |= 0x8u; pa[2] &= ~0x8u; } while (0)
#endif

enum { RATE = 11025, CHUNK = 64, LEAD = RATE / 5, RING = 4096, AMPLITUDE = 9000 };

void snd_init(struct snd *s)
{
    s->build = 0; s->prio = 0; s->n = 0; s->pos = 0; s->left = 0; s->half = 0; s->phase = 0; s->level = AMPLITUDE;
    PA3_ON();                       /* the amplifier enable: output, high (builds 14-24: this was the missing step) */
    SPEAKER_GATE(1, 1);
    DAC_RATE(DAC_HANDLE, RATE);
    DAC_START(DAC_HANDLE);
    s->t0 = SND_TICK;
    s->written = 0;
    s->ready = 1;
}

static int16_t next_sample(struct snd *s)
{
    if (s->n) {
        while (s->left == 0) {                    /* the next step, or the end of the effect */
            if (s->pos >= s->n) { s->n = 0; s->prio = 0; s->half = 0; break; }
            unsigned hz = s->hz[s->pos];
            s->left = s->dur[s->pos] * 110u;
            s->half = hz ? 1411200u / hz : 0;     /* RATE * 256 / 2 / hz */
            if (hz && s->half < 256) s->half = 256;
            s->pos++;
        }
    }
    if (!s->n || !s->half) return 0;
    if (s->left) s->left--;
    s->phase += 256;
    if (s->phase >= s->half) { s->phase -= s->half; s->level = (int16_t)-s->level; }
    return s->level;
}

void snd_pump(struct snd *s)
{
    if (!s->ready) return;
    uint32_t elapsed = SND_TICK - s->t0;
    if (elapsed >= 6000) { s->t0 += 6000; s->written -= 6000 * (RATE / 100); elapsed -= 6000; }
    int32_t due = (int32_t)(elapsed * (RATE / 100)) + LEAD;
    if (due - s->written > RING) s->written = due - LEAD;      /* fell behind: restart the lead */
    int16_t chunk[CHUNK];
    unsigned most = due - s->written > LEAD / 2 ? 8 : 2;
    for (unsigned n = 0; s->written + CHUNK <= due && n < most; n++) {
        for (unsigned i = 0; i < CHUNK; i++) chunk[i] = next_sample(s);
        DAC_WRITE(chunk);
        s->written += CHUNK;
    }
}

void snd_close(struct snd *s)
{
    if (!s->ready) return;
    s->n = 0; s->half = 0;
    for (unsigned i = 0; i < 12; i++) {           /* about 70 ms of silence so the DAC does not hang on a tone */
        int16_t chunk[CHUNK];
        for (unsigned j = 0; j < CHUNK; j++) chunk[j] = 0;
        DAC_WRITE(chunk);
    }
    SPEAKER_GATE(1, 0);
    s->ready = 0;
}

void snd_begin(struct snd *s, unsigned prio)
{
    s->build = 0;
    if (s->n && s->prio > prio) return;                     /* a more important effect is playing */
    s->build = 1;
    s->prio = (uint8_t)prio; s->n = 0; s->pos = 0; s->left = 0; s->half = 0; s->phase = 0;
}

int snd_active(const struct snd *s) { return s->n != 0; }

void snd_note(struct snd *s, unsigned hz, unsigned tens_of_ms)
{
    if (!s->build || s->n >= SND_STEPS) return;   /* refused by snd_begin, or full */
    s->hz[s->n] = (uint16_t)hz; s->dur[s->n] = (uint8_t)tens_of_ms; s->n++;
}

void snd_sweep(struct snd *s, unsigned hz0, unsigned hz1, unsigned tens_of_ms)
{
    for (unsigned i = 0; i < tens_of_ms; i++) {
        int hz = (int)hz0 + ((int)hz1 - (int)hz0) * (int)i / (int)tens_of_ms;
        snd_note(s, (unsigned)hz, 1);
    }
}
