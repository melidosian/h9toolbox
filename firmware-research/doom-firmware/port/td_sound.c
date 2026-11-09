// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: PC-speaker sound through the radio's DAC.
//
// DMC100's s_pcspk.c (Chocolate Doom semantics) hands tone bytes to the
// platform's 140 Hz stepper (platform.h: snd_play / snd_stop / snd_busy /
// snd_volume). DMC100's own stepper drives a PWM buzzer from SysTick; the
// TD-H9 has a speaker behind the BR23 DAC instead, so this stepper
// synthesises the square wave as 16-bit mono PCM at 11025 Hz and feeds it to
// the DAC through stock's driver (rate app+0x433C8, start app+0x43434, write
// app+0x43598: 128 bytes a call).
//
// There is no audio interrupt of ours: td_sound_pump() keeps the DAC about
// 200 ms ahead of the 100 Hz tick and is called from the main loop's frequent
// places (every strip the renderer sends, every input poll, I_UpdateSound).
//
// What the speaker needs (found on hardware, builds 14-24): the amplifier
// enable PA3 configured as an output and driven high. The radio app does that
// at start-up; stock's speaker writer (app+0x52228) only writes the output
// bit, so the Doom firmware sets the direction itself.

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "td.h"
#include "td_music.h"

enum { RATE = 11025, STEPS = 140, LEAD = RATE / 5, CHUNK = TD_MUSIC_CHUNK, RING = 4096 };
#define PIT_HZ 1193181u
#define DAC_HANDLE ((void *)(0x11AD0 + 6636))

// PIT divisor per tone byte (vanilla DOOM's PC speaker driver; table from
// Chocolate Doom src/i_pcsound.c, Copyright (C) 2005-2014 Simon Howard,
// GPL-2.0-or-later, as in DMC100's platform/buzzer.c).
static const uint16_t divisor[128] = {
    0, 6818, 6628, 6449, 6279, 6087, 5906, 5736,
    5575, 5423, 5279, 5120, 4971, 4830, 4697, 4554,
    4435, 4307, 4186, 4058, 3950, 3836, 3728, 3615,
    3519, 3418, 3323, 3224, 3131, 3043, 2960, 2875,
    2794, 2711, 2633, 2560, 2485, 2415, 2348, 2281,
    2213, 2153, 2089, 2032, 1975, 1918, 1864, 1810,
    1757, 1709, 1659, 1612, 1565, 1521, 1478, 1435,
    1395, 1355, 1316, 1280, 1242, 1207, 1173, 1140,
    1107, 1075, 1045, 1015, 986, 959, 931, 905,
    879, 854, 829, 806, 783, 760, 739, 718,
    697, 677, 658, 640, 621, 604, 586, 570,
    553, 538, 522, 507, 493, 479, 465, 452,
    439, 427, 415, 403, 391, 380, 369, 359,
    348, 339, 329, 319, 310, 302, 293, 285,
    276, 269, 261, 253, 246, 239, 232, 226,
    219, 213, 207, 201, 195, 190, 184, 179,
};

static uint8_t buf[SND_MAX];
static uint8_t pos, len;
static int vol = SND_HIGH;
static uint32_t step_acc;       // in 1/RATE steps; a tone step is RATE / STEPS samples
static uint32_t half, phase;    // half period and phase, in 1/256 samples
static int16_t level;
static bool ready;
static uint32_t t0;             // tick the sample clock counts from
static int32_t written;         // samples sent since t0

static void load_tone(void)
{
    half = 0;
    if (pos >= len) {
        len = 0;
        return;
    }
    uint8_t t = buf[pos++];
    if (t && t < 128) {
        // half period = divisor / PIT_HZ / 2 seconds = divisor * RATE / (2 PIT_HZ) samples
        half = (uint32_t)divisor[t] * RATE / (2u * PIT_HZ / 256u);
        if (half < 256)
            half = 256;  // above the Nyquist limit: toggle every sample
    }
}

static int16_t next_sample(void)
{
    if (len) {
        step_acc += STEPS;
        if (step_acc >= RATE) {
            step_acc -= RATE;
            load_tone();
        }
    }
    if (!half || vol == SND_OFF)
        return 0;
    phase += 256;
    if (phase >= half) {
        phase -= half;
        level = (int16_t)-level;
    }
    return level;
}

void snd_volume(int v)
{
    vol = v;
}

void snd_play(const uint8_t *tones, uint32_t n)
{
    if (n > SND_MAX)
        n = SND_MAX;
    for (uint32_t i = 0; i < n; i++)
        buf[i] = tones[i];
    pos = 0;
    len = (uint8_t)n;
    step_acc = 0;
    phase = 0;
    level = (int16_t)(vol == SND_HIGH ? 9000 : 3000);
    load_tone();
}

void snd_stop(void)
{
    len = 0;
    half = 0;
}

bool snd_busy(void)
{
    return len != 0;
}

void td_sound_init(void)
{
#ifndef TD_HOST
    volatile uint32_t *pa = (volatile uint32_t *)0x1E5000;
    pa[0] |= 0x8u;   // PA3 (speaker amplifier enable) high
    pa[2] &= ~0x8u;  // PA3 output
#endif
    h9_speaker_gate(1, 1);
    for (unsigned state = 0; state <= 4; state++)
        h9_audio_volume(state, 30);
    h9_dac_rate(DAC_HANDLE, RATE);
    h9_dac_start(DAC_HANDLE);
    t0 = td_ticks();
    written = 0;
    ready = true;
}

void td_sound_pump(void)
{
    if (!ready)
        return;
    uint32_t elapsed = td_ticks() - t0;
    if (elapsed >= 6000) {  // rebase every minute: keeps the products in 32 bits
        t0 += 6000;
        written -= 6000 * RATE / 100;
        elapsed -= 6000;
    }
    int32_t due = (int32_t)(elapsed * RATE / 100) + LEAD;
    if (due - written > RING)   // fell behind (a long load): restart the lead
        written = due - LEAD;
    // static: the pump runs inside the renderer's deep stack
    static int16_t chunk[CHUNK];
    static int32_t mix[CHUNK];
    for (unsigned n = 0; written + CHUNK <= due && n < 8; n++) {
        for (unsigned i = 0; i < CHUNK; i++)
            mix[i] = next_sample();
        td_music_chunk(mix);  // the music under the effects (td_music.c)
        for (unsigned i = 0; i < CHUNK; i++)
            chunk[i] = (int16_t)(mix[i] > 32767 ? 32767 : mix[i] < -32768 ? -32768 : mix[i]);
        h9_dac_write128(chunk);
        written += CHUNK;
    }
}
