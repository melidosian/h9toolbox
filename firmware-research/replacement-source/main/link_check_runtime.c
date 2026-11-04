#include <stddef.h>
#include <stdint.h>

#include "h9_main_boot_record.h"
#include "h9_main_boot_validator.h"
#include "h9_main_board_markers.h"
#include "h9_main_clock_probe.h"
#include "h9_main_clock_apply.h"
#include "h9_main_clock_rom_mode.h"
#include "h9_main_clock_timing.h"
#include "h9_main_heap_bitmap.h"
#include "h9_main_memory_window.h"
#include "h9_main_post_clock_rom.h"

#define H9_MAIN_RETAINED_BOOT_RECORD 0x0002FF88u
#define H9_MAIN_BOOT_RESULT_WORD 0x00011C3Cu

/* Diagnostic link shims. This entry initializes C memory and early board
 * markers, but not the complete TD-H9 board or production runtime. */
extern unsigned char __data_load_start[];
extern unsigned char __data_start[];
extern unsigned char __data_end[];
extern unsigned char __bss_start[];
extern unsigned char __bss_end[];
extern unsigned char __ram_code_load_start[];
extern unsigned char __ram_code_start[];
extern unsigned char __ram_code_end[];
extern unsigned char __stock_clear_start[];
extern unsigned char __stock_clear_end[];
extern void h9_main_interrupt_config(void);
void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *to = (unsigned char *)destination;
    const unsigned char *from = (const unsigned char *)source;
    size_t index;
    for (index = 0; index < length; ++index) to[index] = from[index];
    return destination;
}

void *memset(void *destination, int value, size_t length)
{
    unsigned char *to = (unsigned char *)destination;
    size_t index;
    for (index = 0; index < length; ++index) to[index] = (unsigned char)value;
    return destination;
}

__attribute__((noreturn)) void h9_main_board_entry(void)
{
#if defined(__PI32V2__)
    H9MainHeapBitmap bitmap;
    H9MainMemoryWindow window;
    H9MainBoardMarkers markers;
    H9MainClockProbe clock_probe;
    H9MainClockSelector clock_selector;
    H9MainClockBand clock_band;
    H9MainClockCalculation clock_calculation;
    H9MainClockMatch clock_match;
    H9MainClockTiming clock_timing;
    H9MainClockApply clock_apply;
    H9MainClockRomMode clock_rom_mode;
    H9MainClockRegisters clock_registers;
    H9MainClockAuxFields clock_auxiliary;
    H9MainClockFinish clock_finish;
    H9MainPostClockRom post_clock_rom;
    H9MainPostClockWords post_clock_words;
    H9MainPostClockClassifier post_clock_classifier;
    H9MainPostClockClassification post_clock_classification;
    H9MainPostClockBitmapBranch post_clock_bitmap_branch;
    H9MainPostClockRouteFlags post_clock_route_flags;
    H9MainPostClockHandoff post_clock_handoff;
    H9MainPostClockHandoffResult post_clock_handoff_result;
    H9MainPostClockDescriptorRoute post_clock_descriptor_route;
    H9MainPostClockDescriptorResult post_clock_descriptor_result;
    H9MainPostClockDescriptor32Board post_clock_descriptor32_board;
    uint32_t calculated_clock;
    uint8_t first_initialization;
    typedef void (*RomSetup)(uint32_t, uint32_t);
    typedef void (*RomContinue)(void);
    /* Stock board entry starts with these two ROM calls, then builds the
     * allocation bitmap, memory window, and guard/flag markers. */
    ((RomSetup)(uintptr_t)0x020000A0u)(0x80u, 0u);
    ((RomContinue)(uintptr_t)0x02003174u)();
    h9_main_heap_bitmap_bind_target(&bitmap);
    first_initialization = *bitmap.initialized == 0u;
    if (!h9_main_heap_bitmap_initialize(&bitmap)) for (;;) { }
    if (first_initialization) {
        h9_main_memory_window_bind_target(&window);
        if (!h9_main_memory_window_initialize(&window)) for (;;) { }
    }
    h9_main_board_markers_bind_target(&markers);
    if (!h9_main_board_markers_initialize(&markers)) for (;;) { }
    h9_main_clock_probe_bind_target(&clock_probe, 100000u);
    if (!h9_main_clock_probe_run(&clock_probe)) for (;;) { }
    h9_main_clock_selector_bind_target(&clock_selector);
    if (!h9_main_clock_selector_run(&clock_selector)) for (;;) { }
    h9_main_clock_band_bind_target(&clock_band);
    if (!h9_main_clock_band_select(&clock_band, 240000000u)) for (;;) { }
    h9_main_clock_calculation_bind_target(&clock_calculation);
    calculated_clock = h9_main_clock_calculate(&clock_calculation,
                                                240000000u);
    *(volatile uint32_t *)(uintptr_t)0x0000D9E4u = calculated_clock;
    if (calculated_clock != UINT32_MAX) {
        h9_main_clock_match_bind_target(&clock_match);
        if (h9_main_clock_match(&clock_match, calculated_clock)) {
            h9_main_clock_timing_bind_target(&clock_timing);
            if (!h9_main_clock_timing_calculate(&clock_timing,
                                                calculated_clock,
                                                240000000u)) for (;;) { }
        }
    }
    h9_main_clock_apply_bind_target(&clock_apply);
    if (!h9_main_clock_apply_first(&clock_apply)) for (;;) { }
    h9_main_clock_rom_mode_bind_target(&clock_rom_mode);
    if (!h9_main_clock_rom_mode_apply(&clock_rom_mode)) for (;;) { }
    if (!h9_main_clock_selector_run(&clock_selector)) for (;;) { }
    h9_main_clock_registers_bind_target(&clock_registers);
    h9_main_clock_aux_bind_target(&clock_auxiliary);
    h9_main_clock_finish_bind_target(&clock_finish, &clock_registers,
                                     &clock_auxiliary);
    if (!h9_main_clock_finish_run(&clock_finish)) for (;;) { }
    h9_main_post_clock_rom_bind_target(&post_clock_rom);
    if (!h9_main_post_clock_rom_run(&post_clock_rom, &post_clock_words))
        for (;;) { }
    h9_main_post_clock_classifier_bind_target(&post_clock_classifier,
                                              &post_clock_rom);
    if (!h9_main_post_clock_classify(&post_clock_classifier,
                                     &post_clock_classification)) for (;;) { }
    post_clock_bitmap_branch = h9_main_post_clock_bitmap_step(
        &post_clock_rom, &post_clock_words, &post_clock_classification,
        &post_clock_route_flags);
    if (post_clock_bitmap_branch == H9_MAIN_POST_CLOCK_BITMAP_ERROR)
        for (;;) { }
    h9_main_post_clock_handoff_bind_target(&post_clock_handoff);
    if (!h9_main_post_clock_handoff_run(
            &post_clock_handoff, post_clock_classification.secondary_status,
            &post_clock_handoff_result)) for (;;) { }
    h9_main_post_clock_descriptor_route_bind_target(&post_clock_descriptor_route);
    if (!h9_main_post_clock_descriptor_route_run(
            &post_clock_descriptor_route, &post_clock_handoff_result,
            &post_clock_descriptor_result)) for (;;) { }
    if (!h9_main_post_clock_apply_initial_masks(
            &post_clock_descriptor_route, &post_clock_words,
            &post_clock_descriptor_result)) for (;;) { }
    h9_main_post_clock_descriptor32_bind_target(&post_clock_descriptor32_board);
    if (!h9_main_post_clock_descriptor32_run(
            &post_clock_descriptor_route, &post_clock_descriptor32_board,
            &post_clock_words, &post_clock_descriptor_result)) for (;;) { }
#endif
    for (;;) { }
}

__attribute__((noreturn)) void h9_main_runtime_init(void)
{
    unsigned char *to;
    unsigned char *from = __data_load_start;
    H9MainBootValidator validator;
    for (to = __stock_clear_start; to < __stock_clear_end; ++to) *to = 0;
    for (to = __data_start; to < __data_end; ++to) *to = *from++;
    from = __ram_code_load_start;
    for (to = __ram_code_start; to < __ram_code_end; ++to) *to = *from++;
    h9_main_interrupt_config();
    /* Stock's first post-copy call is a one-instruction return. The second
     * consumes the retained 80-byte record before board initialization. */
    if (!h9_main_boot_validator_bind_target(&validator)) {
        for (;;) { }
    }
    *(volatile uint32_t *)(uintptr_t)H9_MAIN_BOOT_RESULT_WORD =
        h9_main_consume_boot_record(
            (volatile uint8_t *)(uintptr_t)H9_MAIN_RETAINED_BOOT_RECORD,
            h9_main_boot_validate, &validator);
    h9_main_board_entry();
}
