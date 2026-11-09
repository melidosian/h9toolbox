// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: chiptune music player for H9MU event lists
// (made from Freedoom's MIDI by doom-firmware/music.py).
//
// Seven square-wave voices and one drum voice (noise, or a low square for
// kicks and toms), 11025 Hz, in chunks of 64 samples: the event times count
// chunks. td_sound.c adds one chunk at a time under the sound effects. The
// same file builds on the host (test/music_wav.c) to render a WAV of exactly
// what the radio plays.
//
// Events are read from the resource chip through td_music_read, 256 bytes at
// a time; the song loops at its end event.

#include <stdint.h>

#include "td_music.h"

enum { VOICES = 8, BUFFER = 256 };

// Phase steps for MIDI notes 108..119 (2^32 * f / 11025); lower octaves shift right.
static const uint32_t top_octave[12] = {
    1630727614u, 1727695724u, 1830429858u, 1939272882u, 2054588048u, 2176760211u,
    2306197109u, 2443330725u, 2588618730u, 2742546010u, 2905626283u, 3078403812u,
};

typedef struct {
    uint32_t phase, step;
    int16_t amp;        // current amplitude
    int16_t fall;       // amplitude lost per chunk (drums)
    uint8_t noise;      // 1: noise instead of a square
} voice_t;

static voice_t voice[VOICES];
static td_music_read_t music_read;
static uint32_t base, length, loop_at;   // events in the chip; the first event
static uint32_t pos, buffered_at;
static uint8_t buffer[BUFFER];
static uint32_t wait;                    // chunks until the next event
static uint16_t lfsr = 0xACE1u;
static int playing;
static int volume = 3;                   // 0 off .. 4 loud

static uint32_t note_step(unsigned note)
{
    if (note > 119)
        note = 119;
    unsigned octave_down = note < 108 ? (108 - note + 11) / 12 : 0;
    unsigned index = (note + 12 * octave_down) - 108;
    return top_octave[index] >> octave_down;
}

static uint8_t next_byte(void)
{
    if (pos >= length)
        return 0xFF;
    if (pos < buffered_at || pos >= buffered_at + BUFFER) {
        buffered_at = pos & ~(uint32_t)(BUFFER - 1);
        uint32_t n = length - buffered_at < BUFFER ? length - buffered_at : BUFFER;
        music_read(base + buffered_at, buffer, n);
    }
    return buffer[pos++ - buffered_at];
}

static uint32_t next_delta(void)
{
    uint32_t value = 0;
    for (unsigned shift = 0; shift < 28; shift += 7) {
        uint8_t c = next_byte();
        value |= (uint32_t)(c & 0x7F) << shift;
        if (!(c & 0x80))
            break;
    }
    return value;
}

static void drum(voice_t *v, unsigned note, unsigned level)
{
    int16_t amp = (int16_t)(level * 260);
    unsigned chunks = 8;
    v->noise = 1;
    if (note == 35 || note == 36) {                     // kick: a low square
        v->noise = 0;
        v->step = note_step(31);
        chunks = 10;
    } else if (note == 41 || note == 43 || note == 45 || note == 47 || note == 48 || note == 50) {
        v->noise = 0;                                   // toms
        v->step = note_step(note);
        chunks = 12;
    } else if (note == 42 || note == 44 || note == 46) {
        chunks = 4;                                     // hi-hats
    } else if (note == 38 || note == 40) {
        chunks = 14;                                    // snares
    } else if (note == 49 || note == 51 || note == 52 || note == 55 || note == 57) {
        chunks = 40;                                    // cymbals
        amp = (int16_t)(amp / 2);
    }
    v->amp = amp;
    v->fall = (int16_t)(amp / (int)chunks + 1);
}

static void run_events(void)
{
    while (!wait) {
        uint8_t command = next_byte();
        if (command == 0xFF) {                          // end: loop
            pos = loop_at;
            for (unsigned i = 0; i < VOICES; i++)
                voice[i].amp = 0;
            wait = next_delta();
            if (!wait)
                wait = 1;
            return;
        }
        voice_t *v = &voice[command & 7];
        switch (command >> 4) {
        case 0:  // note off
            v->amp = 0;
            break;
        case 1: {  // note on
            uint8_t note = next_byte(), level = next_byte();
            v->noise = 0;
            v->fall = 0;
            v->step = note_step(note);
            v->amp = (int16_t)(level * 170);
            break;
        }
        case 2: {  // drum
            uint8_t note = next_byte(), level = next_byte();
            drum(v, note, level);
            break;
        }
        default:
            break;
        }
        wait = next_delta();
    }
}

int td_music_start(td_music_read_t read, uint32_t address, uint32_t size)
{
    uint8_t header[32];
    playing = 0;
    if (size < 33)
        return -1;
    read(address, header, sizeof header);
    if (header[0] != 'H' || header[1] != '9' || header[2] != 'M' || header[3] != 'U')
        return -2;
    uint32_t events = (uint32_t)header[8] | (uint32_t)header[9] << 8 | (uint32_t)header[10] << 16 |
                      (uint32_t)header[11] << 24;
    if (events + 32 > size)
        return -3;
    music_read = read;
    base = address + 32;
    length = events;
    loop_at = 0;
    pos = 0;
    buffered_at = ~(uint32_t)0 - BUFFER;
    for (unsigned i = 0; i < VOICES; i++)
        voice[i].amp = 0;
    wait = next_delta();
    playing = 1;
    return 0;
}

void td_music_volume(int level)
{
    volume = level;
}

int td_music_playing(void)
{
    return playing;
}

// Adds one chunk (TD_MUSIC_CHUNK samples) of music to mix.
void td_music_chunk(int32_t *mix)
{
    if (!playing || !volume)
        return;
    run_events();
    wait--;
    for (unsigned i = 0; i < VOICES; i++) {
        voice_t *v = &voice[i];
        if (v->amp <= 0)
            continue;
        int32_t amp = v->amp * volume / 4;
        if (v->noise) {
            for (unsigned s = 0; s < TD_MUSIC_CHUNK; s++) {
                if (!(s & 1)) {
                    unsigned bit = (lfsr ^ (lfsr >> 1)) & 1u;
                    lfsr = (uint16_t)(lfsr >> 1 | bit << 14);
                }
                mix[s] += lfsr & 1 ? amp : -amp;
            }
        } else {
            uint32_t phase = v->phase, step = v->step;
            for (unsigned s = 0; s < TD_MUSIC_CHUNK; s++) {
                mix[s] += phase & 0x80000000u ? amp : -amp;
                phase += step;
            }
            v->phase = phase;
        }
        if (v->fall) {
            v->amp = (int16_t)(v->amp - v->fall);
            if (v->amp < 0)
                v->amp = 0;
        }
    }
}
