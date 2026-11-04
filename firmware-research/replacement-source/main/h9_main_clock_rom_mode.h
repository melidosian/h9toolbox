#ifndef H9_MAIN_CLOCK_ROM_MODE_H
#define H9_MAIN_CLOCK_ROM_MODE_H

#include <stdbool.h>
#include <stdint.h>

/* Stock main app+0x0C5288, called immediately after first clock apply. */
typedef struct {
    volatile uint8_t *enabled_d9d0;
    volatile uint8_t *band_d9d4;
    void *context;
    void (*rom_mode)(void *context, uint32_t group, uint32_t zero,
                     uint32_t field, uint32_t value);
} H9MainClockRomMode;

void h9_main_clock_rom_mode_bind_target(H9MainClockRomMode *mode);
bool h9_main_clock_rom_mode_apply(H9MainClockRomMode *mode);

#endif
