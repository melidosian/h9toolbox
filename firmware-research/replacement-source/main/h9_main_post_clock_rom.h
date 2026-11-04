#ifndef H9_MAIN_POST_CLOCK_ROM_H
#define H9_MAIN_POST_CLOCK_ROM_H

#include <stdbool.h>
#include <stdint.h>

/* Board entry app+0x05FC24..0x05FC4C, immediately after the clock helper. */
typedef struct {
    void *context;
    uint32_t (*read_selector)(void *context, uint32_t selector);
    void (*set_control)(void *context, uint32_t group, uint32_t value);
} H9MainPostClockRom;

typedef struct {
    uint32_t first_word;
    uint32_t second_word;
} H9MainPostClockWords;

void h9_main_post_clock_rom_bind_target(H9MainPostClockRom *rom);
bool h9_main_post_clock_rom_run(H9MainPostClockRom *rom,
                                H9MainPostClockWords *words);

#endif
