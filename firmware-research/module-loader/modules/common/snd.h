/* Sound for modules: square-wave effects through the radio's DAC (snd.c).
 * The state lives in the caller (modules have no globals). Experimental:
 * the recipe is the Doom firmware's (resource-flash-log.md, 2026-11-09, builds
 * 24-26); it has not been proven inside the module loader yet. */
#ifndef H9_SND_H
#define H9_SND_H

#include <stdint.h>

enum { SND_STEPS = 80 };

struct snd {
    uint8_t ready, build;        /* build: the effect being filled in was accepted */
    uint8_t prio, n, pos;        /* priority of the playing effect; steps loaded; step being played */
    uint16_t hz[SND_STEPS];      /* 0 = rest */
    uint8_t dur[SND_STEPS];      /* in 10 ms */
    uint32_t left, half, phase;  /* samples left in the step; half period and phase in 1/256 sample */
    int16_t level;
    int32_t written;             /* samples sent since t0 */
    uint32_t t0;
};

void snd_init(struct snd *s);                 /* amplifier, DAC rate and start; call once */
void snd_close(struct snd *s);                /* a little silence, release the amplifier */
void snd_pump(struct snd *s);                 /* keep the DAC fed: call every few milliseconds */
void snd_begin(struct snd *s, unsigned prio); /* start a new effect unless a higher one is playing; returns with n = 0 if refused */
void snd_note(struct snd *s, unsigned hz, unsigned tens_of_ms);
void snd_sweep(struct snd *s, unsigned hz0, unsigned hz1, unsigned tens_of_ms);
int snd_active(const struct snd *s);

#endif
