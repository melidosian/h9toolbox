#include "h9_main_clock_rom_mode.h"

#include <stddef.h>

static const uint8_t stock_band_values[4] = { 7u, 8u, 10u, 14u };

static void target_rom_mode(void *context, uint32_t group, uint32_t zero,
                            uint32_t field, uint32_t value)
{
    typedef void (*RomMode)(uint32_t, uint32_t, uint32_t, uint32_t);
    (void)context;
    ((RomMode)(uintptr_t)0x020000E2u)(group, zero, field, value);
}

void h9_main_clock_rom_mode_bind_target(H9MainClockRomMode *mode)
{
    if (mode == NULL) return;
    mode->enabled_d9d0 = (volatile uint8_t *)(uintptr_t)0x0000D9D0u;
    mode->band_d9d4 = (volatile uint8_t *)(uintptr_t)0x0000D9D4u;
    mode->context = NULL;
    mode->rom_mode = target_rom_mode;
}

bool h9_main_clock_rom_mode_apply(H9MainClockRomMode *mode)
{
    uint32_t selected = 0u;
    bool enabled;
    if (mode == NULL || mode->enabled_d9d0 == NULL ||
        mode->band_d9d4 == NULL || mode->rom_mode == NULL) return false;
    enabled = *mode->enabled_d9d0 != 0u;
    if (enabled) {
        if (*mode->band_d9d4 >= 4u) return false;
        selected = stock_band_values[*mode->band_d9d4];
    }
    mode->rom_mode(mode->context, 9u, 0u, 4u, selected);
    mode->rom_mode(mode->context, 6u, 0u, 3u, enabled ? 7u : 0u);
    return true;
}
