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

void h9_main_post_clock_classifier_bind_target(
    H9MainPostClockClassifier *classifier, H9MainPostClockRom *rom)
{
    if (classifier == NULL) return;
    classifier->boot_flags_100100 =
        (volatile uint32_t *)(uintptr_t)0x00100100u;
    classifier->channel_status_1e511c =
        (volatile uint32_t *)(uintptr_t)0x001E511Cu;
    classifier->strap_status_1f0200 =
        (volatile uint32_t *)(uintptr_t)0x001F0200u;
    classifier->secondary_status_1f0500 =
        (volatile uint32_t *)(uintptr_t)0x001F0500u;
    classifier->mode_11b43 =
        (volatile uint8_t *)(uintptr_t)0x00011B43u;
    classifier->rom = rom;
}

bool h9_main_post_clock_classify(H9MainPostClockClassifier *classifier,
                                 H9MainPostClockClassification *result)
{
    uint32_t flags, channel, selector, strap;
    uint8_t index, mode;
    if (classifier == NULL || result == NULL ||
        classifier->boot_flags_100100 == NULL ||
        classifier->channel_status_1e511c == NULL ||
        classifier->strap_status_1f0200 == NULL ||
        classifier->secondary_status_1f0500 == NULL ||
        classifier->mode_11b43 == NULL || classifier->rom == NULL ||
        classifier->rom->read_selector == NULL) return false;

    flags = *classifier->boot_flags_100100;
    channel = UINT32_MAX;
    if ((flags & 1u) != 0u) {
        channel = (*classifier->channel_status_1e511c >> 21u) & 3u;
    }
    selector = classifier->rom->read_selector(classifier->rom->context, 148u);
    strap = *classifier->strap_status_1f0200;
    index = (uint8_t)((strap >> 8u) & 0xFu);
    result->accepted = false;
    result->strap_index = index;
    result->channel_flag = channel;
    result->selector_148_masked = 0u;
    result->secondary_status = 0u;
    result->mode = 0u;
    if (index > 9u) return true;
    if ((340u & (1u << index)) != 0u) mode = 2u;
    else if ((680u & (1u << index)) != 0u) mode = 4u;
    else return true;

    selector &= 0xFFFFFF01u;
    *classifier->mode_11b43 = mode;
    if (channel != 1u) channel = 0u;
    result->channel_flag = channel;
    result->selector_148_masked = selector;
    result->secondary_status = *classifier->secondary_status_1f0500;
    result->mode = mode;
    result->accepted = true;
    return true;
}
