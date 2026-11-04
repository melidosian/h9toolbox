#include "h9_aprs_status.h"

#include <stddef.h>

void h9_aprs_status_encode(const H9AprsStatusSnapshot *snapshot,
                            uint8_t output[H9_APRS_STATUS_BYTES])
{
    uint16_t elapsed;
    size_t index;
    if (snapshot == NULL || output == NULL) return;
    for (index = 0u; index < 10u; ++index) {
        uint32_t value = snapshot->words[index];
        output[index * 4u] = (uint8_t)value;
        output[index * 4u + 1u] = (uint8_t)(value >> 8);
        output[index * 4u + 2u] = (uint8_t)(value >> 16);
        output[index * 4u + 3u] = (uint8_t)(value >> 24);
    }
    output[40] = snapshot->word_10_low;
    output[41] = (uint8_t)snapshot->serial_halfword;
    output[42] = (uint8_t)snapshot->serial_halfword;
    output[43] = (uint8_t)(snapshot->serial_halfword >> 8);
    output[44] = snapshot->serial_byte_a;
    output[45] = snapshot->serial_byte_b;
    elapsed = (uint16_t)(snapshot->current_tick - snapshot->words[8]);
    output[46] = (uint8_t)elapsed;
    output[47] = (uint8_t)(elapsed >> 8);
}
