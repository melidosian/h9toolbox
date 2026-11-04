#include "h9_main_clock_timing.h"

#include <stddef.h>

/* Stock app+0x0C5194 returns the first 8-bit index whose quotient is at
 * most the selected record limit, or max_index + 1 for max_index 7. */
static bool choose_divider(uint32_t value, uint32_t limit,
                           uint16_t max_index, uint16_t *chosen)
{
    uint16_t index;
    for (index = 0u; index <= max_index; ++index) {
        if (value / ((uint32_t)index + 1u) <= limit) {
            *chosen = index;
            return true;
        }
    }
    if (max_index == 7u) {
        *chosen = 8u;
        return true;
    }
    return false;
}

void h9_main_clock_timing_bind_target(H9MainClockTiming *timing)
{
    if (timing == NULL) return;
    timing->record_words_da08 = (volatile uint32_t *)(uintptr_t)0x0000DA08u;
    timing->band_d9d4 = (volatile uint8_t *)(uintptr_t)0x0000D9D4u;
    timing->stage_clock_d9d8 = (volatile uint32_t *)(uintptr_t)0x0000D9D8u;
    timing->timebase_d9dc = (volatile uint32_t *)(uintptr_t)0x0000D9DCu;
    timing->divider_d9e8 = (volatile uint8_t *)(uintptr_t)0x0000D9E8u;
    timing->divider_d9ec = (volatile uint8_t *)(uintptr_t)0x0000D9ECu;
    timing->rate_d9f0 = (volatile uint8_t *)(uintptr_t)0x0000D9F0u;
    timing->divider_d9f4 = (volatile uint16_t *)(uintptr_t)0x0000D9F4u;
    timing->divider_d9f8 = (volatile uint8_t *)(uintptr_t)0x0000D9F8u;
    timing->intermediate_da04 = (volatile uint32_t *)(uintptr_t)0x0000DA04u;
}

bool h9_main_clock_timing_calculate(H9MainClockTiming *timing,
                                    uint32_t source_hz,
                                    uint32_t target_hz)
{
    volatile uint32_t *record;
    uint32_t first_clock, second_clock, final_clock;
    uint32_t ratio;
    uint16_t first, third, fourth, fifth;
    uint8_t rate;

    if (timing == NULL || timing->record_words_da08 == NULL ||
        timing->band_d9d4 == NULL || timing->stage_clock_d9d8 == NULL ||
        timing->timebase_d9dc == NULL || timing->divider_d9e8 == NULL ||
        timing->divider_d9ec == NULL || timing->rate_d9f0 == NULL ||
        timing->divider_d9f4 == NULL || timing->divider_d9f8 == NULL ||
        timing->intermediate_da04 == NULL || *timing->band_d9d4 >= 4u ||
        source_hz == 0u || source_hz == UINT32_MAX || target_hz == 0u)
        return false;

    record = timing->record_words_da08 + 5u * *timing->band_d9d4;
    if (!choose_divider(source_hz, record[0], 7u, &first)) return false;
    first_clock = source_hz / ((uint32_t)first + 1u);
    if (!choose_divider(first_clock, record[3], 255u, &third) ||
        !choose_divider(first_clock, record[1], 255u, &fourth))
        return false;

    /* The stock minimum is (first_clock / target_hz - 1) when that quotient
     * is nonzero. Its 8-bit truncation is preserved for the comparison. */
    ratio = first_clock / target_hz;
    if ((ratio & 0xFFu) != 0u) {
        uint16_t minimum = (uint8_t)(ratio - 1u);
        if (fourth < minimum) fourth = minimum;
    }
    second_clock = first_clock / ((uint32_t)fourth + 1u);
    if (!choose_divider(second_clock, record[2], 7u, &fifth))
        return false;
    final_clock = second_clock / ((uint32_t)fifth + 1u);
    rate = (uint8_t)(((first_clock / 1000000u) * 30u) / 1000u);

    *timing->divider_d9e8 = (uint8_t)first;
    *timing->rate_d9f0 = rate;
    *timing->intermediate_da04 = first_clock;
    *timing->divider_d9ec = (uint8_t)third;
    *timing->divider_d9f4 = fourth;
    *timing->stage_clock_d9d8 = second_clock;
    *timing->divider_d9f8 = (uint8_t)fifth;
    *timing->timebase_d9dc = final_clock;
    return true;
}
