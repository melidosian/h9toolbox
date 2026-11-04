#include <stddef.h>
#include <stdint.h>

#include "h9_main_boot_record.h"
#include "h9_main_boot_validator.h"
#include "h9_main_board_markers.h"
#include "h9_main_clock_probe.h"
#include "h9_main_heap_bitmap.h"
#include "h9_main_memory_window.h"

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
