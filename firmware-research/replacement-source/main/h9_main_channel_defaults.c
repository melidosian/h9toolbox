#include "h9_main_channel_defaults.h"

#include <stddef.h>

const uint8_t
    h9_main_channel_default_template[H9_MAIN_DEFAULT_TEMPLATE_SIZE] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0x48u, 0x42u, 0u, 0u, 0x48u, 0x42u,
    0u, 0u, 0xB4u, 0x42u, 0u, 0u, 0xB4u, 0x42u,
};

static void store16(volatile uint8_t *where, uint32_t value)
{
    where[0] = (uint8_t)value;
    where[1] = (uint8_t)(value >> 8);
}

static void store32(volatile uint8_t *where, uint32_t value)
{
    unsigned index;
    for (index = 0u; index < 4u; ++index)
        where[index] = (uint8_t)(value >> (index * 8u));
}

static void init_record(volatile uint8_t *record)
{
    unsigned index;
    store32(record, 0u);
    store16(record + 4u, 10u);
    store16(record + 6u, 300u);
    store32(record + 0x38u, 0u);
    store32(record + 0x3Cu, 0u);
    for (index = 0u; index < H9_MAIN_DEFAULT_TEMPLATE_SIZE; ++index)
        record[8u + index] = h9_main_channel_default_template[index];
}

static void init_state(volatile uint8_t *state, uint8_t mode)
{
    state[3] = mode;
    state[4] = 0x19u;
    state[5] = 0u;
    state[6] = 1u;
}

void h9_main_channel_defaults_bind_target(H9MainChannelDefaults *defaults)
{
    if (defaults == NULL) return;
    defaults->state_11ad0 = (volatile uint8_t *)(uintptr_t)0x00011AD0u;
    defaults->primary_143b0 = (volatile uint8_t *)(uintptr_t)0x000143B0u;
    defaults->secondary_144f8 = (volatile uint8_t *)(uintptr_t)0x000144F8u;
}

int h9_main_channel_defaults_init_prefix(const H9MainChannelDefaults *defaults)
{
    unsigned index;
    if (defaults == NULL || defaults->state_11ad0 == NULL ||
        defaults->primary_143b0 == NULL ||
        defaults->secondary_144f8 == NULL) return -1;
    for (index = 0u; index < 2u; ++index) {
        uint32_t offset = index * H9_MAIN_DEFAULT_RECORD_STRIDE;
        init_record(defaults->primary_143b0 + offset);
        init_state(defaults->state_11ad0 + offset, 0x20u);
    }
    for (index = 0u; index < 2u; ++index) {
        uint32_t offset = index * H9_MAIN_DEFAULT_RECORD_STRIDE;
        init_record(defaults->secondary_144f8 + offset);
        init_state(defaults->state_11ad0 + offset, 0x68u);
    }
    return 0;
}
