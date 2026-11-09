// Host tool: render an H9MU song with the radio's player (port/td_music.c) to
// a 16-bit mono 11025 Hz WAV, to hear exactly what the TD-H9 will play.
//     music_wav <song.h9mu> <out.wav> [seconds]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "td_music.h"

static unsigned char *song;
static long song_size;

static void read_song(uint32_t address, void *dst, uint32_t n)
{
    if (address + n > (uint32_t)song_size) {
        memset(dst, 0xFF, n);
        if (address < (uint32_t)song_size)
            memcpy(dst, song + address, song_size - address);
        return;
    }
    memcpy(dst, song + address, n);
}

static void le(FILE *f, uint32_t v, int bytes)
{
    for (int i = 0; i < bytes; i++)
        fputc((int)(v >> (8 * i)) & 0xFF, f);
}

int main(int argc, char **argv)
{
    if (argc < 3)
        return 2;
    FILE *in = fopen(argv[1], "rb");
    if (!in)
        return 1;
    fseek(in, 0, SEEK_END);
    song_size = ftell(in);
    rewind(in);
    song = malloc(song_size);
    if (fread(song, 1, song_size, in) != (size_t)song_size)
        return 1;
    fclose(in);
    int seconds = argc > 3 ? atoi(argv[3]) : 140;
    if (td_music_start(read_song, 0, (uint32_t)song_size)) {
        fprintf(stderr, "not an H9MU song\n");
        return 1;
    }
    uint32_t chunks = (uint32_t)seconds * 11025u / TD_MUSIC_CHUNK, samples = chunks * TD_MUSIC_CHUNK;
    FILE *out = fopen(argv[2], "wb");
    fwrite("RIFF", 1, 4, out); le(out, 36 + samples * 2, 4); fwrite("WAVEfmt ", 1, 8, out);
    le(out, 16, 4); le(out, 1, 2); le(out, 1, 2); le(out, 11025, 4); le(out, 22050, 4); le(out, 2, 2); le(out, 16, 2);
    fwrite("data", 1, 4, out); le(out, samples * 2, 4);
    int32_t peak = 0, clipped = 0;
    for (uint32_t c = 0; c < chunks; c++) {
        int32_t mix[TD_MUSIC_CHUNK] = { 0 };
        td_music_chunk(mix);
        for (int i = 0; i < TD_MUSIC_CHUNK; i++) {
            int32_t v = mix[i];
            if (abs(v) > peak) peak = abs(v);
            if (v > 32767) { v = 32767; clipped++; }
            if (v < -32768) { v = -32768; clipped++; }
            le(out, (uint32_t)(int16_t)v & 0xFFFF, 2);
        }
    }
    fclose(out);
    printf("%s: %u samples, peak %d, clipped %d\n", argv[2], samples, peak, clipped);
    return 0;
}
