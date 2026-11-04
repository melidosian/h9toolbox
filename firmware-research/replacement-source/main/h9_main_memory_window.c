#include "h9_main_memory_window.h"

#include <stddef.h>

static void write_word(volatile uint8_t *state, unsigned offset,
                       uint32_t value)
{
    unsigned byte;
    for (byte = 0u; byte < 4u; ++byte)
        state[offset + byte] = (uint8_t)(value >> (byte * 8u));
}

void h9_main_memory_window_bind_target(H9MainMemoryWindow *window)
{
    if (window == NULL) return;
    window->controller = (volatile uint32_t *)(uintptr_t)0x00100300u;
    window->window = (volatile uint8_t *)(uintptr_t)0x0002C000u;
    window->state = (volatile uint8_t *)(uintptr_t)0x0001D6F0u;
}

bool h9_main_memory_window_initialize(H9MainMemoryWindow *window)
{
    unsigned index;
    volatile uint32_t *bitmap;
    if (window == NULL || window->controller == NULL ||
        window->window == NULL || window->state == NULL) return false;
    window->controller[1] = 0x0002C000u;
    window->controller[2] = 0x0002D1FFu;
    window->controller[0] |= 0x10u;
    /* Stock uses ROM 0x021127B4(0x2C000, 0, 0x1200). */
    for (index = 0u; index < H9_MAIN_MEMORY_WINDOW_BYTES; ++index)
        window->window[index] = 0u;
    window->controller[0] &= ~0x10u;
    window->controller[0] |= 1u;

    write_word(window->state, 0x04u, 0x0002C000u);
    write_word(window->state, 0x08u, 0x00248000u);
    write_word(window->state, 0x50u, 0x00200000u);
    write_word(window->state, 0x54u, 0x0020F000u);
    write_word(window->state, 0x58u, 0x0000F000u);
    write_word(window->state, 0x5Cu, 0x01E00000u);
    window->state[0] = 1u;
    window->state[0x60u] = 0xE0u;
    window->state[0x61u] = 0x01u;
    /* Stock clears 0x79 bytes from 0x1D754, leaving the bitmap prefix. */
    for (index = 0u; index < 0x79u; ++index)
        window->state[0x64u + index] = 0u;
    bitmap = (volatile uint32_t *)(void *)(window->state + 0x14u);
    for (index = 0u; index < 0x1E0u; ++index)
        bitmap[index >> 5] |= 1u << (index & 31u);
    return true;
}
