#include "h9_main_post_clock_rom.h"
#include "h9_main_timer3_irq.h"

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
    result->strap_index = index;
    result->channel_flag = channel;
    result->selector_148_masked = 0u;
    result->secondary_status = 0u;
    result->mode = 0u;
    if (index <= 9u && (340u & (1u << index)) != 0u) mode = 2u;
    else if (index <= 9u && (680u & (1u << index)) != 0u)
        mode = 4u;
    else mode = 1u;

    selector &= 0xFFFFFF01u;
    *classifier->mode_11b43 = mode;
    if (channel != 1u) channel = 0u;
    result->channel_flag = channel;
    result->selector_148_masked = selector;
    result->secondary_status = *classifier->secondary_status_1f0500;
    result->mode = mode;
    return true;
}

H9MainPostClockBitmapBranch h9_main_post_clock_bitmap_step(
    H9MainPostClockRom *rom, H9MainPostClockWords *words,
    const H9MainPostClockClassification *classification,
    H9MainPostClockRouteFlags *flags)
{
    uint32_t index;
    if (rom == NULL || words == NULL || classification == NULL ||
        flags == NULL ||
        rom->read_selector == NULL) return H9_MAIN_POST_CLOCK_BITMAP_ERROR;
    flags->r6_flag = classification->channel_flag == 1u;
    flags->r9_flag = flags->r6_flag;
    if (classification->selector_148_masked == 0u)
        return H9_MAIN_POST_CLOCK_BITMAP_CONTINUE;

    index = rom->read_selector(rom->context, 192u);
    if (index == 56u) flags->r6_flag = true;
    else if (index == 57u) flags->r9_flag = true;
    if (index > 55u) return H9_MAIN_POST_CLOCK_BITMAP_NO_CLEAR;
    if (index < 32u) words->first_word &= ~(1u << index);
    else words->second_word &= ~(1u << (index - 32u));
    return H9_MAIN_POST_CLOCK_BITMAP_CONTINUE;
}

static uint32_t target_transform_mask(void *context, uint32_t mask)
{
    typedef uint32_t (*RomTransform)(uint32_t);
    (void)context;
    return ((RomTransform)(uintptr_t)0x02000A78u)(mask);
}

static uint64_t target_query_pair(void *context, uint32_t r0, uint32_t r1)
{
    typedef uint64_t (*RomQuery)(uint32_t, uint32_t);
    (void)context;
    return ((RomQuery)(uintptr_t)0x02000D18u)(r0, r1);
}

void h9_main_post_clock_handoff_bind_target(H9MainPostClockHandoff *handoff)
{
    if (handoff == NULL) return;
    handoff->context = NULL;
    handoff->transform_mask = target_transform_mask;
    handoff->query_pair = target_query_pair;
}

bool h9_main_post_clock_handoff_run(
    H9MainPostClockHandoff *handoff, uint32_t secondary_status,
    H9MainPostClockHandoffResult *result)
{
    uint64_t pair;
    uint32_t mask, transformed;
    if (handoff == NULL || result == NULL ||
        handoff->transform_mask == NULL || handoff->query_pair == NULL)
        return false;
    mask = (secondary_status & 1u) != 0u ? 0xFFFFFE3Bu : UINT32_MAX;
    transformed = handoff->transform_mask(handoff->context, mask);
    pair = handoff->query_pair(handoff->context, transformed, transformed);
    result->input_mask = mask;
    result->transformed_mask = transformed;
    result->result_r0 = (uint32_t)pair;
    result->result_r1 = (uint32_t)(pair >> 32u);
    return true;
}

static volatile uint32_t *target_lookup_descriptor(void *context,
                                                    uint32_t index)
{
    (void)context;
    if (index > 56u) return NULL;
    /* Stock app+0xBA indexes a table at app+0x554 containing 0x1E5000,
     * 0x1E5040, 0x1E5080, and 0x1E50C0. */
    return (volatile uint32_t *)(uintptr_t)
        (0x001E5000u + ((index >> 4u) * 0x40u));
}

void h9_main_post_clock_descriptor_route_bind_target(
    H9MainPostClockDescriptorRoute *route)
{
    if (route == NULL) return;
    route->context = NULL;
    route->lookup_descriptor = target_lookup_descriptor;
}

static void clear_descriptor_bits(
    const H9MainPostClockDescriptorRoute *route, uint32_t index,
    uint32_t mask)
{
    volatile uint32_t *first =
        route->lookup_descriptor(route->context, index);
    volatile uint32_t *second;
    if (first != NULL) first[4] &= ~mask;
    second = route->lookup_descriptor(route->context, 53u);
    if (second != NULL) second[4] &= ~0x20u;
    if (first != NULL) first[5] &= ~mask;
    if (second != NULL) second[5] &= ~0x20u;
}

bool h9_main_post_clock_descriptor_route_run(
    const H9MainPostClockDescriptorRoute *route,
    const H9MainPostClockHandoffResult *handoff,
    H9MainPostClockDescriptorResult *result)
{
    bool secondary_zero;
    if (route == NULL || handoff == NULL || result == NULL ||
        route->lookup_descriptor == NULL) return false;
    secondary_zero = handoff->result_r1 == 0u;
    result->filtered_mask = handoff->input_mask;
    result->base_word = 0xFFFF0000u;
    if (handoff->result_r0 == 4u) {
        if (secondary_zero) {
            result->base_word = 0xFFFF0080u;
            result->control_word = 0xFFFF003Fu;
        } else {
            result->filtered_mask &= 0x1FFFu;
            result->control_word = 0xFFFF0033u;
        }
    } else if (secondary_zero) {
        clear_descriptor_bits(route, 23u, 0x80u);
        result->control_word = 0xFFFF001Fu;
    } else {
        clear_descriptor_bits(route, 15u, 0x8000u);
        result->filtered_mask &= 0x9FFFu;
        result->control_word = 0xFFFF0013u;
    }
    return true;
}

static void and_descriptor_word(const H9MainPostClockDescriptorRoute *route,
                                uint32_t index, uint32_t word, uint32_t mask)
{
    volatile uint32_t *descriptor =
        route->lookup_descriptor(route->context, index);
    if (descriptor != NULL) descriptor[word] &= mask;
}

bool h9_main_post_clock_apply_initial_masks(
    const H9MainPostClockDescriptorRoute *route,
    const H9MainPostClockWords *words,
    const H9MainPostClockDescriptorResult *profile)
{
    if (route == NULL || words == NULL || profile == NULL ||
        route->lookup_descriptor == NULL) return false;
    and_descriptor_word(route, 0u, 5u,
                        ~profile->filtered_mask | 0xFFFF0000u);
    and_descriptor_word(route, 0u, 8u,
                        ~(words->first_word & 0xFFFFu));
    and_descriptor_word(route, 16u, 5u, profile->base_word);
    and_descriptor_word(route, 16u, 8u,
                        ~((words->first_word >> 16u) & 0xFFFFu));
    return true;
}

void h9_main_post_clock_descriptor32_bind_target(
    H9MainPostClockDescriptor32Board *board)
{
    if (board == NULL) return;
    board->bank_status_1e0200 =
        (volatile uint32_t *)(uintptr_t)0x001E0200u;
    board->expected_descriptor32_1e5080 =
        (volatile uint32_t *)(uintptr_t)0x001E5080u;
    board->state_byte_d975 =
        (volatile uint8_t *)(uintptr_t)0x0000D975u;
    board->state_guard_d90c =
        (volatile uint32_t *)(uintptr_t)0x0000D90Cu;
    board->control_registers_1e19c0 =
        (volatile uint32_t *)(uintptr_t)0x001E19C0u;
}

static void apply_zero_state(
    const H9MainPostClockDescriptor32Board *board)
{
    volatile uint32_t *regs = board->control_registers_1e19c0;
    if (*board->state_byte_d975 == 0u) return;
    *board->state_byte_d975 = 0u;
    if (*board->state_guard_d90c == 0u) {
        regs[3] &= ~0x10000u;
        regs[0] &= ~3u;
    }
    regs[8] &= ~0x30000u;
    regs[10] &= ~0x4000u;
    regs[12] &= 0xFFEEFFFFu;
}

bool h9_main_post_clock_descriptor32_run(
    const H9MainPostClockDescriptorRoute *route,
    const H9MainPostClockDescriptor32Board *board,
    const H9MainPostClockWords *words,
    const H9MainPostClockDescriptorResult *profile)
{
    volatile uint32_t *descriptor;
    uint32_t previous;
    if (route == NULL || board == NULL || words == NULL || profile == NULL ||
        route->lookup_descriptor == NULL ||
        board->bank_status_1e0200 == NULL ||
        board->expected_descriptor32_1e5080 == NULL ||
        board->state_byte_d975 == NULL ||
        board->state_guard_d90c == NULL ||
        board->control_registers_1e19c0 == NULL) return false;
    descriptor = route->lookup_descriptor(route->context, 32u);
    if (descriptor != NULL) {
        previous = descriptor[3];
        if ((*board->bank_status_1e0200 & 0xFu) <= 2u &&
            descriptor == board->expected_descriptor32_1e5080) {
            descriptor[4] &= ~0x40u;
            descriptor[0] &= ~0x40u;
            descriptor[2] &= ~0x40u;
            apply_zero_state(board);
        }
        descriptor[3] = (previous & 0xFFFF0000u) | 0x40u;
    }
    and_descriptor_word(route, 32u, 8u,
                        ~(words->second_word & 0xFFFFu));
    and_descriptor_word(route, 48u, 5u, profile->control_word);
    and_descriptor_word(route, 48u, 8u,
                        ~((words->second_word >> 16u) & 0xFFFFu));
    return true;
}

void h9_main_post_clock_gate_bind_target(
    H9MainPostClockGateRegisters *registers)
{
    if (registers == NULL) return;
    registers->control_1e1800 =
        (volatile uint32_t *)(uintptr_t)0x001E1800u;
    registers->state_1e5100 =
        (volatile uint32_t *)(uintptr_t)0x001E5100u;
}

bool h9_main_post_clock_gate_run(
    const H9MainPostClockGateRegisters *registers,
    const H9MainPostClockRouteFlags *flags)
{
    volatile uint32_t *control;
    volatile uint32_t *state;
    if (registers == NULL || flags == NULL ||
        registers->control_1e1800 == NULL ||
        registers->state_1e5100 == NULL) return false;
    control = registers->control_1e1800;
    state = registers->state_1e5100;
    if (!flags->r6_flag) {
        control[0] = 0u;
        state[0] |= 0x900u;
        control[0] = 0u;
        control[1] = 0u;
        control[0] = 0u;
        state[0] |= 0x900u;
        state[0] |= 0x4u;
        state[0] &= ~0x10u;
        state[0] &= ~0x40u;
        state[0] &= ~0x200u;
        state[0] &= ~0x2000u;
    }
    if (!flags->r9_flag) {
        control[0] = 0u;
        state[0] |= 0x900u;
        control[0] = 0u;
        control[1] = 0u;
        control[0] = 0u;
        state[0] |= 0x900u;
        state[0] |= 0x8u;
        state[0] &= ~0x20u;
        state[0] &= ~0x80u;
        state[0] &= ~0x400u;
        state[0] &= ~0x4000u;
    }
    return true;
}

bool h9_main_post_clock_timer_stage_run(H9MainPostClockTimerStage *stage)
{
    uint32_t selector;
    volatile uint32_t discarded;
    if (stage == NULL || stage->rom_setup == NULL ||
        stage->install_timer_irq == NULL || stage->timer_irq_handler == NULL ||
        stage->clock_count_d9d8 == NULL ||
        stage->timer_registers_10f0ec == NULL ||
        stage->dummy_read_1e00c0 == NULL ||
        stage->selector_byte_11b40 == NULL || stage->rom == NULL ||
        stage->rom->read_selector == NULL ||
        stage->rom->set_control == NULL) return false;

    stage->rom_setup(stage->context, 0x809Bu, 0u);
    stage->install_timer_irq(stage->context, 3u, 1u,
                             stage->timer_irq_handler);
    stage->timer_registers_10f0ec[1] = 0u;
    stage->timer_registers_10f0ec[2] =
        (uint32_t)((int32_t)*stage->clock_count_d9d8 / 100 - 1);
    stage->timer_registers_10f0ec[0] = 0x41u;

    discarded = *stage->dummy_read_1e00c0;
    (void)discarded;
    selector = stage->rom->read_selector(stage->rom->context, 49u);
    if ((selector & 0x80u) != 0u) {
        stage->rom->set_control(stage->rom->context, 49u, 64u);
        selector = stage->rom->read_selector(stage->rom->context, 18u);
    }
    *stage->selector_byte_11b40 = (uint8_t)selector;
    return true;
}

static void target_timer_setup(void *context, uint32_t mode, uint32_t value)
{
    typedef void (*RomSetup)(uint32_t, uint32_t);
    (void)context;
    ((RomSetup)(uintptr_t)0x020000A0u)(mode, value);
}

static void target_install_timer_irq(void *context, uint32_t irq,
                                     uint32_t priority, void (*handler)(void))
{
    typedef void (*RomClock)(void);
    volatile uint32_t *vector =
        (volatile uint32_t *)(uintptr_t)0x0002BF00u;
    volatile uint32_t *control =
        (volatile uint32_t *)(uintptr_t)0x0010F100u;
    uint32_t value;
    (void)context;
    if (irq != 3u || priority != 1u || handler == NULL) return;
    /* IRQ-3 mapping byte at pinned stock app+0x0A2A25 is 1. */
    vector[3] = (uint32_t)(uintptr_t)handler;
    ((RomClock)(uintptr_t)0x02001634u)();
    value = *control;
    value &= ~0xF000u;
    value |= 0x3000u;
    *control = value;
    ((RomClock)(uintptr_t)0x02001650u)();
}

void h9_main_post_clock_timer_stage_bind_target(
    H9MainPostClockTimerStage *stage, H9MainPostClockRom *rom)
{
    if (stage == NULL) return;
    stage->context = NULL;
    stage->rom_setup = target_timer_setup;
    stage->install_timer_irq = target_install_timer_irq;
    stage->timer_irq_handler = h9_main_timer3_isr;
    stage->clock_count_d9d8 =
        (volatile uint32_t *)(uintptr_t)0x0000D9D8u;
    stage->timer_registers_10f0ec =
        (volatile uint32_t *)(uintptr_t)0x0010F0ECu;
    stage->dummy_read_1e00c0 =
        (volatile uint32_t *)(uintptr_t)0x001E00C0u;
    stage->selector_byte_11b40 =
        (volatile uint8_t *)(uintptr_t)0x00011B40u;
    stage->rom = rom;
}
