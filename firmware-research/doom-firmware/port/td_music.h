// SPDX-License-Identifier: GPL-2.0-or-later
// TD-H9 port of DMC100-Doom: chiptune music player (td_music.c).
#pragma once

#include <stdint.h>

enum { TD_MUSIC_CHUNK = 64 };  // samples per chunk at 11025 Hz; event times count chunks

typedef void (*td_music_read_t)(uint32_t address, void *dst, uint32_t n);

// Starts the H9MU song at address (size bytes). 0, or <0 when it is not a song.
int td_music_start(td_music_read_t read, uint32_t address, uint32_t size);
// Adds one chunk of music to mix (TD_MUSIC_CHUNK samples).
void td_music_chunk(int32_t *mix);
void td_music_volume(int level);  // 0 off .. 4 loud (default 3)
int td_music_playing(void);
