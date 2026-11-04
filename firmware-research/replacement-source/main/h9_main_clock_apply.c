#include "h9_main_clock_apply.h"

#include <stddef.h>

static void target_rom_apply(void *context)
{
    typedef void (*RomApply)(void);
    (void)context;
    ((RomApply)(uintptr_t)0x02001634u)();
}

static void target_synchronize(void *context)
{
    (void)context;
#if defined(__PI32V2__)
    __asm__ volatile("csync" ::: "memory");
#endif
}

static void target_delay_nops(void *context, uint32_t count)
{
    (void)context;
    while (count-- != 0u) {
#if defined(__PI32V2__)
        __asm__ volatile("nop");
#endif
    }
}

void h9_main_clock_apply_bind_target(H9MainClockApply *apply)
{
    if (apply == NULL) return;
    apply->control_1e000c = (volatile uint32_t *)(uintptr_t)0x001E000Cu;
    apply->context = NULL;
    apply->rom_apply = target_rom_apply;
    apply->synchronize = target_synchronize;
    apply->delay_nops = target_delay_nops;
}

bool h9_main_clock_apply_first(H9MainClockApply *apply)
{
    if (apply == NULL || apply->control_1e000c == NULL ||
        apply->rom_apply == NULL || apply->synchronize == NULL ||
        apply->delay_nops == NULL) return false;
    apply->rom_apply(apply->context);
    apply->synchronize(apply->context);
    apply->control_1e000c[0] &= ~0xCu;
    apply->control_1e000c[0] |= 2u;
    apply->delay_nops(apply->context, 100u);
    apply->control_1e000c[0] &= ~0x1C0u;
    return true;
}
