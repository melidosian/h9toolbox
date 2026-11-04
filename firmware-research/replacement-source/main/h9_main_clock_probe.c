#include "h9_main_clock_probe.h"

#include <stddef.h>

static void target_rom_write(void *context, uint32_t selector, uint32_t value)
{
    typedef void (*RomWrite)(uint32_t, uint32_t);
    (void)context;
    ((RomWrite)(uintptr_t)0x020000A0u)(selector, value);
}

static uint32_t target_rom_read(void *context, uint32_t selector)
{
    typedef uint32_t (*RomRead)(uint32_t);
    (void)context;
    return ((RomRead)(uintptr_t)0x02000058u)(selector);
}

static void target_rom_mask(void *context, uint32_t selector, uint32_t mask)
{
    typedef void (*RomMask)(uint32_t, uint32_t);
    (void)context;
    ((RomMask)(uintptr_t)0x02000A28u)(selector, mask);
}

static bool target_measurement_complete(void *context,
                                        volatile uint32_t *measurement)
{
    (void)context;
    return (measurement[0] & 0x8000u) != 0u;
}

void h9_main_clock_probe_bind_target(H9MainClockProbe *probe,
                                     uint32_t poll_limit)
{
    if (probe == NULL) return;
    probe->fuse_word = (volatile uint32_t *)(uintptr_t)0x001F0200u;
    probe->calibration_words = (volatile uint32_t *)(uintptr_t)0x0000DA08u;
    probe->measurement = (volatile uint32_t *)(uintptr_t)0x001E0400u;
    probe->context = NULL;
    probe->rom_write = target_rom_write;
    probe->rom_read = target_rom_read;
    probe->rom_mask = target_rom_mask;
    probe->measurement_complete = target_measurement_complete;
    probe->poll_limit = poll_limit;
}

bool h9_main_clock_probe_run(H9MainClockProbe *probe)
{
    uint32_t fuse_nibble;
    uint32_t channel;
    uint32_t poll;
    if (probe == NULL || probe->fuse_word == NULL ||
        probe->calibration_words == NULL || probe->measurement == NULL ||
        probe->rom_write == NULL || probe->rom_read == NULL ||
        probe->rom_mask == NULL || probe->measurement_complete == NULL ||
        probe->poll_limit == 0u) return false;

    /* Stock uextra/insert rotates this low nibble right one bit. */
    fuse_nibble = ((*probe->fuse_word >> 8) + 7u) & 0xFu;
    fuse_nibble = ((fuse_nibble >> 1) | (fuse_nibble << 3)) & 0xFu;
    if (fuse_nibble < 8u && ((0xE1u >> fuse_nibble) & 1u) != 0u) {
        for (channel = 0u; channel < H9_MAIN_CLOCK_PROBE_CHANNELS; ++channel)
            probe->calibration_words[3u + channel * 5u] = 50000000u;
    }

    for (channel = 0u; channel < H9_MAIN_CLOCK_PROBE_CHANNELS; ++channel) {
        probe->rom_write(probe->context, 0xB0u, 0x40u);
        probe->rom_write(probe->context, 0xB1u,
                         ((channel & 0x3Fu) << 2) | 0x82u);
        probe->measurement[0] = 0x4000u;
        probe->measurement[1] = 0u;
        probe->measurement[2] = 0x960u;
        probe->measurement[0] = 1u;
        for (poll = 0u; poll < probe->poll_limit; ++poll)
            if (probe->measurement_complete(probe->context,
                                            probe->measurement)) break;
        if (poll == probe->poll_limit) {
            probe->measurement[0] = 0x4000u;
            return false;
        }
        probe->measurement[0] = 0x4000u;
        (void)probe->rom_read(probe->context, 0xB2u);
        probe->rom_mask(probe->context, 0xB1u, 0xFDu);
    }
    return true;
}

void h9_main_clock_selector_bind_target(H9MainClockSelector *selector)
{
    if (selector == NULL) return;
    selector->clock_registers = (volatile uint32_t *)(uintptr_t)0x001E1920u;
    selector->flag_da58 = (volatile uint8_t *)(uintptr_t)0x0000DA58u;
    selector->flag_da5c = (volatile uint8_t *)(uintptr_t)0x0000DA5Cu;
}

bool h9_main_clock_selector_run(H9MainClockSelector *selector)
{
    uint32_t previous;
    uint32_t flag_bits = 0u;
    if (selector == NULL || selector->clock_registers == NULL ||
        selector->flag_da58 == NULL || selector->flag_da5c == NULL)
        return false;

    previous = selector->clock_registers[0];
    selector->clock_registers[0] = 0x331D524Du;
    if ((previous & 0x04000000u) != 0u)
        selector->clock_registers[0] |= 0x04000000u;
    selector->clock_registers[1] = 0x1102u;

    /* The tail-called stock helper first clears both fields, then inserts
     * one field for each nonzero byte at 0xDA58 and 0xDA5C. */
    selector->clock_registers[0] &= 0xFFF807FFu;
    if (*selector->flag_da58 != 0u) flag_bits |= 0x00005000u;
    if (*selector->flag_da5c != 0u) flag_bits |= 0x00050000u;
    selector->clock_registers[0] |= flag_bits;
    return true;
}

void h9_main_clock_band_bind_target(H9MainClockBand *band)
{
    if (band == NULL) return;
    band->threshold_words = (volatile uint32_t *)(uintptr_t)0x0000DA0Cu;
    band->enabled_d9d0 = (volatile uint8_t *)(uintptr_t)0x0000D9D0u;
    band->request_d9e0 = (volatile uint8_t *)(uintptr_t)0x0000D9E0u;
    band->band_d9d4 = (volatile uint8_t *)(uintptr_t)0x0000D9D4u;
}

bool h9_main_clock_band_select(H9MainClockBand *band, uint32_t frequency_hz)
{
    uint8_t selected = 3u;
    uint8_t index;
    if (band == NULL || band->threshold_words == NULL ||
        band->enabled_d9d0 == NULL || band->request_d9e0 == NULL ||
        band->band_d9d4 == NULL) return false;

    /* Stock caller writes this byte immediately before app+0x0C509C. */
    *band->request_d9e0 = 1u;
    if (*band->enabled_d9d0 != 0u) {
        for (index = 0u; index < H9_MAIN_CLOCK_PROBE_CHANNELS; ++index) {
            if (band->threshold_words[index * 5u] >= frequency_hz) {
                selected = index;
                break;
            }
        }
    }
    *band->band_d9d4 = selected;
    return true;
}
