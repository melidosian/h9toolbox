#include "h9_main_post_map_gate.h"

#include <stddef.h>

bool h9_main_post_map_gate_run(H9MainPostMapGate *gate)
{
    volatile uint32_t *config;
    volatile uint32_t *words;
    if (gate == NULL || gate->handshake == NULL ||
        gate->fill_prefix_ones == NULL || gate->config_101040 == NULL ||
        gate->words_101050 == NULL) return false;
    config = gate->config_101040;
    words = gate->words_101050;

    if (!gate->fill_prefix_ones(gate->context)) return false;
    if (!h9_main_clock_wait(gate->handshake, true)) return false;
    *config &= ~0x400u;
    words[1] = 0u;
    words[0] = 0x66BFu;
    *config |= 0x800u;
    *config |= 0x400u;
    if (!h9_main_clock_wait(gate->handshake, false)) return false;

    if (!h9_main_clock_wait(gate->handshake, true)) return false;
    *config &= ~0x100u;
    words[3] = 0u;
    words[2] = 0x66BFu;
    *config |= 0x200u;
    *config |= 0x100u;
    return h9_main_clock_wait(gate->handshake, false);
}

static bool target_fill_prefix_ones(void *context)
{
    (void)context;
#if defined(__PI32V2__)
    extern void h9_main_vector_prefix_ones(void);
    h9_main_vector_prefix_ones();
    return true;
#else
    return false;
#endif
}

void h9_main_post_map_gate_bind_target(H9MainPostMapGate *gate,
                                        H9MainClockHandshake *handshake)
{
    if (gate == NULL) return;
    gate->context = NULL;
    gate->handshake = handshake;
    gate->fill_prefix_ones = target_fill_prefix_ones;
    gate->config_101040 =
        (volatile uint32_t *)(uintptr_t)0x00101040u;
    gate->words_101050 =
        (volatile uint32_t *)(uintptr_t)0x00101050u;
}
