#include "h9_main_adc_mode.h"

#include <stddef.h>

static void target_rom_configure(void *context, uint32_t group,
                                  uint32_t selector, uint32_t mask,
                                  uint32_t value)
{
    typedef void (*RomConfigure)(uint32_t, uint32_t, uint32_t, uint32_t);
    (void)context;
    ((RomConfigure)(uintptr_t)0x020000E2u)(group, selector, mask, value);
}

void h9_main_adc_mode_bind_target(H9MainAdcMode *selection)
{
    if (selection == NULL) return;
    selection->ready_11b98 = (volatile uint16_t *)(uintptr_t)0x00011B98u;
    selection->register_1e19ec =
        (volatile uint32_t *)(uintptr_t)0x001E19ECu;
    selection->register_1e0044 =
        (volatile uint32_t *)(uintptr_t)0x001E0044u;
    selection->register_1e1964 =
        (volatile uint32_t *)(uintptr_t)0x001E1964u;
    selection->register_1e1910 =
        (volatile uint32_t *)(uintptr_t)0x001E1910u;
    selection->adc_1e3100 = (volatile uint32_t *)(uintptr_t)0x001E3100u;
    selection->context = NULL;
    selection->rom_configure = target_rom_configure;
}

static void clear_bits(volatile uint32_t *register_word, uint32_t keep)
{
#if defined(__PI32V2__)
    __asm__ volatile ("[%0+0] &= %1" ::
                      "r"(register_word), "r"(keep) : "memory");
#else
    *register_word &= keep;
#endif
}

static void set_bits(volatile uint32_t *register_word, uint32_t mask)
{
#if defined(__PI32V2__)
    __asm__ volatile ("[%0+0] |= %1" ::
                      "r"(register_word), "r"(mask) : "memory");
#else
    *register_word |= mask;
#endif
}

bool h9_main_adc_mode_select(H9MainAdcMode *selection, uint32_t mode,
                              uint16_t *previous_ready)
{
    uint16_t previous;
    uint32_t clock_word;
    if (selection == NULL || selection->ready_11b98 == NULL ||
        selection->register_1e19ec == NULL ||
        selection->register_1e0044 == NULL ||
        selection->register_1e1964 == NULL ||
        selection->register_1e1910 == NULL ||
        selection->adc_1e3100 == NULL ||
        selection->rom_configure == NULL ||
        (mode != 0x50000Fu && mode != 0xFu)) return false;

    previous = *selection->ready_11b98;
    *selection->ready_11b98 = UINT16_MAX;
    selection->rom_configure(selection->context, 4u, 0u, 1u, 0u);
    clock_word = *selection->register_1e19ec;
    clock_word = (clock_word & 0x80FFFFFFu) | 0x73000000u;
    *selection->register_1e19ec = clock_word;
    clear_bits(selection->register_1e0044, 0xFFFBFFFFu);
    clear_bits(selection->register_1e1964, 0xFFF7FFFFu);
    clear_bits(selection->register_1e1910, 0xFFFFFFBFu);
    /* Both requested modes have low 20 bits 0xF: jump-table case zero. */
    selection->rom_configure(selection->context, 4u, 4u, 1u, 0u);
    selection->rom_configure(selection->context, 4u, 0u, 1u, 1u);
    selection->rom_configure(selection->context, 4u, 1u, 3u,
                              mode >> 20u);
    *selection->adc_1e3100 = 0xFF6Eu;
    set_bits(selection->adc_1e3100, 0x10u);
    set_bits(selection->adc_1e3100, 0x40u);
    if (previous_ready != NULL) *previous_ready = previous;
    return true;
}
