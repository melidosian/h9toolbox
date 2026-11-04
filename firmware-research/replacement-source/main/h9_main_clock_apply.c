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

static void target_rom_finish(void *context)
{
    typedef void (*RomFinish)(void);
    (void)context;
    ((RomFinish)(uintptr_t)0x02001650u)();
}

void h9_main_clock_finish_bind_target(H9MainClockFinish *finish,
                                      H9MainClockRegisters *source,
                                      H9MainClockAuxFields *auxiliary)
{
    if (finish == NULL) return;
    finish->control_1e000c =
        (volatile uint32_t *)(uintptr_t)0x001E000Cu;
    finish->source = source;
    finish->auxiliary = auxiliary;
    finish->context = NULL;
    finish->delay_nops = target_delay_nops;
    finish->rom_finish = target_rom_finish;
}

bool h9_main_clock_finish_run(H9MainClockFinish *finish)
{
    volatile uint32_t *control;
    H9MainClockRegisters *source;
    H9MainClockAuxFields *aux;
    if (finish == NULL || finish->control_1e000c == NULL ||
        finish->source == NULL || finish->auxiliary == NULL ||
        finish->delay_nops == NULL || finish->rom_finish == NULL)
        return false;
    control = finish->control_1e000c;
    source = finish->source;
    aux = finish->auxiliary;
    if (source->control_1e0014 == NULL || source->source_d9fc == NULL ||
        source->divider_da00 == NULL || source->result_d9e4 == NULL ||
        aux->control_1e0008 == NULL || aux->field_d9e8 == NULL ||
        aux->rom_arg_d9f0 == NULL || aux->rom_arg_d9ec == NULL ||
        aux->field_d9f4 == NULL || aux->field_d9f8 == NULL ||
        aux->rom_apply_pair == NULL ||
        (uintptr_t)source->control_1e0014 != (uintptr_t)control + 8u ||
        (uintptr_t)aux->control_1e0008 + 4u != (uintptr_t)control)
        return false;

    /* Separate volatile writes retain the order of app+0x0C55AA..0x0C563A. */
    control[13] &= 0xFFFCFFFFu;
    control[13] = (control[13] & 0xFF3FFFFFu) | 0x00400000u;
    control[13] = (control[13] & 0xF8FFFFFFu) | 0x05000000u;
    control[13] = (control[13] & 0xC7FFFFFFu) | 0x10000000u;
    control[14] &= 0xFFFF7FFFu;
    control[14] |= 0x00300000u;
    control[14] = (control[14] & 0xFFFF8FFFu) | 0x00003000u;
    control[13] &= ~1u;
    control[13] &= ~2u;
    control[14] = (control[14] & 0xFF3FFFFFu) | 0x00800000u;
    control[13] &= 0xFFFFFE03u;
    control[14] = (control[14] & 0xFFFFF000u) | 0x26u;

    /* The stock source writer silently skips a UINT32_MAX clock. */
    (void)h9_main_clock_program_source(source);
    control[2] |= 0x40u;
    control[2] &= ~0x80u;
    if (!h9_main_clock_latch_source(source)) return false;
    control[13] |= 1u;
    finish->delay_nops(finish->context, 10u);
    control[13] |= 2u;
    finish->delay_nops(finish->context, 20u);
    if (!h9_main_clock_aux_program(aux)) return false;
    control[0] |= 0x1C0u;
    finish->rom_finish(finish->context);
    control[1] &= 0xFFFFF3FFu;
    control[1] &= 0xFFFFFFF3u;
    return true;
}
