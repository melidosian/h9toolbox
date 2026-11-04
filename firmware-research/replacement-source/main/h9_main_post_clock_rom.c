#include "h9_main_post_clock_rom.h"

#include <stddef.h>

static uint32_t target_read_selector(void *context, uint32_t selector)
{
    typedef uint32_t (*RomRead)(uint32_t);
    (void)context;
    return ((RomRead)(uintptr_t)0x02000058u)(selector);
}

static void target_set_control(void *context, uint32_t group, uint32_t value)
{
    typedef void (*RomSet)(uint32_t, uint32_t);
    (void)context;
    ((RomSet)(uintptr_t)0x020008FEu)(group, value);
}

void h9_main_post_clock_rom_bind_target(H9MainPostClockRom *rom)
{
    if (rom == NULL) return;
    rom->context = NULL;
    rom->read_selector = target_read_selector;
    rom->set_control = target_set_control;
}

bool h9_main_post_clock_rom_run(H9MainPostClockRom *rom,
                                H9MainPostClockWords *words)
{
    uint32_t first, second;
    if (rom == NULL || words == NULL || rom->read_selector == NULL ||
        rom->set_control == NULL) return false;

    first = rom->read_selector(rom->context, 49u);
    if ((first & 0x80u) == 0u) {
        second = rom->read_selector(rom->context, 51u);
        if ((second & 0x80u) != 0u) {
            rom->set_control(rom->context, 17u, 1u);
        }
    }
    words->first_word = UINT32_MAX;
    words->second_word = UINT32_MAX;
    return true;
}
