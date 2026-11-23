#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "radio_settings_catalog.h"

static uint8_t theme, focus_state_ram, live_layout;
static uint32_t band_state;
static uint8_t band_region;
static unsigned redraws;
static void mark_redraw(unsigned force) { assert(force == 0); ++redraws; }

#define THEME_STATE theme
#define FOCUS_STATE focus_state_ram
#define DISPLAY_LAYOUT live_layout
#define DISPLAY_REDRAW mark_redraw
#define BAND_STATE band_state
#define BAND_REGION band_region
#include "radio_storage.c"

static uint8_t native_layout, vm[8];
static unsigned vm_length, vm_reads, vm_writes, partition_writes;
static int fail_read, fail_write, fail_verify, corrupt_native_once;

int radio_vm_read(unsigned key, void *dst, unsigned length)
{
    assert(key == FOCUS_VM_KEY && length == sizeof focus_on);
    ++vm_reads;
    if (fail_read) return -250;
    if (!vm_length) return VM_MISSING;
    if (length > vm_length) length = vm_length;
    memcpy(dst, vm, length);
    return length;
}

int radio_vm_write(unsigned key, const void *src, unsigned length)
{
    assert(key == FOCUS_VM_KEY && length == sizeof focus_on - 1);
    ++vm_writes;
    if (fail_write) return -250;
    memcpy(vm, src, length);
    vm_length = length;
    if (fail_verify) fail_read = 1;
    return length;
}

int radio_partition_read(unsigned partition, unsigned offset, void *dst, unsigned length)
{
    assert(partition == 4 && offset == RADIO_DISPLAY_LAYOUT_OFFSET && length == 1);
    *(uint8_t *)dst = native_layout;
    return RADIO_OK;
}

int radio_partition_write(unsigned partition, unsigned offset, const void *src, unsigned length)
{
    assert(partition == 4 && offset == RADIO_DISPLAY_LAYOUT_OFFSET && length == 1);
    ++partition_writes;
    native_layout = *(const uint8_t *)src;
    assert(native_layout <= RADIO_DISPLAY_CLASSIC);
    if (corrupt_native_once) {
        native_layout ^= 1;
        corrupt_native_once = 0;
    }
    return RADIO_OK;
}

int radio_bt_read(unsigned offset, void *dst, unsigned length)
{
    (void)offset; (void)dst; (void)length;
    assert(!"unexpected Bluetooth read");
    return RADIO_INVALID;
}

int radio_bt_write(unsigned offset, const void *src, unsigned length)
{
    (void)offset; (void)src; (void)length;
    assert(!"unexpected Bluetooth write");
    return RADIO_INVALID;
}

static unsigned selected(void)
{
    struct radio_edit edit;
    assert(!radio_begin(&radio_stock_storage, &radio_settings[RADIO_DISPLAY_LAYOUT], &edit));
    return radio_value(&edit);
}

static int select_layout(unsigned value)
{
    struct radio_edit edit;
    assert(!radio_begin(&radio_stock_storage, &radio_settings[RADIO_DISPLAY_LAYOUT], &edit));
    int result = radio_set_number(&edit, value);
    return result ? result : radio_save(&radio_stock_storage, &edit);
}

static void reboot(void)
{
    focus_state_ram = 0;
    live_layout = native_layout;
    radio_focus_sync();
}

int main(void)
{
    const struct radio_setting *layout = &radio_settings[RADIO_DISPLAY_LAYOUT];
    assert(layout->offset == RADIO_DISPLAY_LAYOUT_OFFSET && layout->choice_count == 4);
    assert(strcmp(layout->choices[RADIO_DISPLAY_FOCUS].label, "Focus") == 0);
    for (unsigned stock = RADIO_DISPLAY_SINGLE; stock <= RADIO_DISPLAY_CLASSIC; ++stock) {
        native_layout = stock;
        reboot();
        assert(!radio_focus_enabled() && selected() == stock && live_layout == stock);
    }
    assert(!redraws && !vm_writes && !partition_writes);
    native_layout = live_layout = RADIO_DISPLAY_DUAL;
    assert(select_layout(4) == RADIO_INVALID);
    assert(select_layout(RADIO_DISPLAY_FOCUS) == RADIO_OK);
    assert(radio_focus_enabled() && vm_writes == 1 && !partition_writes);
    assert(selected() == RADIO_DISPLAY_FOCUS && native_layout == RADIO_DISPLAY_DUAL);
    reboot();
    assert(radio_focus_enabled() && live_layout == RADIO_DISPLAY_CLASSIC && redraws == 1);
    unsigned reads = vm_reads;
    radio_focus_sync();
    assert(vm_reads == reads && redraws == 1);
    assert(select_layout(RADIO_DISPLAY_FOCUS) == RADIO_OK && vm_writes == 1);

    corrupt_native_once = 1;
    assert(select_layout(RADIO_DISPLAY_SINGLE) == RADIO_IO_ERROR);
    assert(selected() == RADIO_DISPLAY_FOCUS && native_layout == RADIO_DISPLAY_DUAL && vm_writes == 1);
    fail_write = 1;
    assert(select_layout(RADIO_DISPLAY_SINGLE) == RADIO_IO_ERROR);
    assert(selected() == RADIO_DISPLAY_FOCUS && native_layout == RADIO_DISPLAY_DUAL);
    fail_write = 0;
    assert(select_layout(RADIO_DISPLAY_SINGLE) == RADIO_OK);
    radio_focus_sync();
    assert(live_layout == RADIO_DISPLAY_SINGLE && redraws == 2);
    reboot();
    assert(!radio_focus_enabled() && selected() == RADIO_DISPLAY_SINGLE);

    /* An unrelated record with our prefix must never be read or replaced as
     * owned storage. Ordinary native choices are still available. */
    memcpy(vm, "H9F+guest", sizeof vm);
    vm_length = sizeof vm;
    reboot();
    assert(!radio_focus_enabled());
    unsigned writes = vm_writes;
    assert(select_layout(RADIO_DISPLAY_FOCUS) == RADIO_IO_ERROR && vm_writes == writes);
    assert(select_layout(RADIO_DISPLAY_DUAL) == RADIO_OK && vm_writes == writes);
    assert(radio_focus_reset() == RADIO_OK && vm_writes == writes);

    vm_length = 0;
    focus_state_ram = 0;
    fail_verify = 1;
    assert(select_layout(RADIO_DISPLAY_FOCUS) == RADIO_IO_ERROR);
    assert(!radio_focus_enabled()); /* VM currently unreadable. */
    fail_read = fail_verify = 0;
    assert(select_layout(RADIO_DISPLAY_CLASSIC) == RADIO_OK);
    reboot();
    assert(!radio_focus_enabled() && selected() == RADIO_DISPLAY_CLASSIC);

    unsigned redraws_before = redraws;
    assert(select_layout(RADIO_DISPLAY_FOCUS) == RADIO_OK);
    radio_focus_sync();
    assert(live_layout == RADIO_DISPLAY_CLASSIC && redraws == redraws_before + 1);
    assert(select_layout(RADIO_DISPLAY_CLASSIC) == RADIO_OK);
    radio_focus_sync();
    assert(live_layout == RADIO_DISPLAY_CLASSIC && redraws == redraws_before + 2);

    assert(select_layout(RADIO_DISPLAY_FOCUS) == RADIO_OK && radio_focus_enabled());
    fail_write = 1;
    assert(radio_focus_reset() == RADIO_IO_ERROR && radio_focus_enabled());
    fail_write = 0;
    assert(radio_focus_reset() == RADIO_OK);
    reboot();
    assert(!radio_focus_enabled() && selected() == RADIO_DISPLAY_CLASSIC);
    puts("Focus preference, native layout fallback, resets, foreign records and failed saves passed.");
    return 0;
}
