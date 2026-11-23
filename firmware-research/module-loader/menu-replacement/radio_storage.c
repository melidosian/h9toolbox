#include "radio_settings.h"
#include "radio_bluetooth.h"
#include "radio_33cm.h"
#include "radio_focus.h"
#include "radio_theme.h"

/* Session preference in the boot-cleared area, between 900TX and quick VOX. */
#ifndef THEME_STATE
#define THEME_STATE (*(volatile uint8_t *)0x299F4u)
#endif
#ifndef FOCUS_STATE
#define FOCUS_STATE (*(volatile uint8_t *)0x299F8u)
#define DISPLAY_LAYOUT (*(volatile uint8_t *)0x12BADu)
#define DISPLAY_REDRAW ((void (*)(unsigned))0x01e53306u)
#endif

/* The main-image TX hooks use the cached VM setting. */
#ifndef BAND_STATE
#define BAND_STATE (*(volatile uint32_t *)RADIO_33CM_STATE)
#define BAND_REGION (*(const volatile uint8_t *)RADIO_33CM_REGION)
#endif

int radio_partition_read(unsigned partition, unsigned offset, void *dst, unsigned length);
int radio_partition_write(unsigned partition, unsigned offset, const void *src, unsigned length);
int radio_vm_read(unsigned key, void *dst, unsigned length);
int radio_vm_write(unsigned key, const void *src, unsigned length);

enum { FOCUS_VM_KEY = 251, VM_MISSING = -252, FOCUS_RECORD_FOREIGN = -1, FOCUS_RECORD_ERROR = -2 };
enum { FOCUS_UNREAD, FOCUS_OFF, FOCUS_ON, FOCUS_ON_PENDING, FOCUS_STOCK_PENDING };
static const char focus_on[] = "H9F+", focus_off[] = "H9F-";

/* v1.0.38 returns the bytes copied, so ask for one extra to distinguish
 * our four-byte record from a foreign record with the same prefix. */
static int read_focus_record(void)
{
    char record[sizeof focus_on];
    int length = radio_vm_read(FOCUS_VM_KEY, record, sizeof record);
    if (length == VM_MISSING) return VM_MISSING;
    if (length < 0) return FOCUS_RECORD_ERROR;
    if (length != sizeof focus_on - 1 ||
        record[0] != 'H' || record[1] != '9' || record[2] != 'F') return FOCUS_RECORD_FOREIGN;
    return record[3] == '+' ? FOCUS_ON : record[3] == '-' ? FOCUS_OFF : FOCUS_RECORD_FOREIGN;
}

static unsigned focus_state(void)
{
    if (FOCUS_STATE == FOCUS_UNREAD)
        FOCUS_STATE = read_focus_record() == FOCUS_ON ? FOCUS_ON_PENDING : FOCUS_OFF;
    return FOCUS_STATE;
}

int radio_focus_enabled(void)
{
    unsigned state = focus_state();
    return state == FOCUS_ON || state == FOCUS_ON_PENDING;
}

void radio_focus_sync(void)
{
    unsigned state = focus_state();
    if (state == FOCUS_OFF) return;
    unsigned wanted = state < FOCUS_STOCK_PENDING ? RADIO_DISPLAY_CLASSIC : state - FOCUS_STOCK_PENDING;
    if (state == FOCUS_ON_PENDING) FOCUS_STATE = FOCUS_ON;
    if (state >= FOCUS_STOCK_PENDING) FOCUS_STATE = FOCUS_OFF;
    if (state >= FOCUS_ON_PENDING || DISPLAY_LAYOUT != wanted) {
        DISPLAY_LAYOUT = wanted;
        DISPLAY_REDRAW(0);
    }
}

static int write_focus_record(unsigned state)
{
    int old = read_focus_record();
    if (old != FOCUS_ON && old != FOCUS_OFF && old != VM_MISSING) return RADIO_IO_ERROR;
    if (old == (int)state || (old == VM_MISSING && state == FOCUS_OFF)) return RADIO_OK;
    const char *bytes = state == FOCUS_ON ? focus_on : focus_off;
    return radio_vm_write(FOCUS_VM_KEY, bytes, sizeof focus_on - 1) == sizeof focus_on - 1 &&
           read_focus_record() == (int)state ? RADIO_OK : RADIO_IO_ERROR;
}

int radio_focus_reset(void)
{
    int saved = read_focus_record();
    if (saved == FOCUS_RECORD_ERROR || (saved == FOCUS_ON && write_focus_record(FOCUS_OFF)))
        return RADIO_IO_ERROR;
    FOCUS_STATE = FOCUS_OFF;
    return RADIO_OK;
}

static int store_native_layout(uint8_t layout)
{
    uint8_t stored;
    return radio_partition_write(4, RADIO_DISPLAY_LAYOUT_OFFSET, &layout, 1) ||
           radio_partition_read(4, RADIO_DISPLAY_LAYOUT_OFFSET, &stored, 1) ||
           stored != layout;
}

static int write_display_layout(uint8_t requested)
{
    if (requested > RADIO_DISPLAY_FOCUS) return RADIO_INVALID;
    if (requested == RADIO_DISPLAY_FOCUS) {
        int result = write_focus_record(FOCUS_ON);
        FOCUS_STATE = result ? FOCUS_UNREAD : FOCUS_ON_PENDING;
        return result;
    }

    int saved = read_focus_record();
    if (saved == FOCUS_RECORD_ERROR) return RADIO_IO_ERROR;
    uint8_t previous;
    if (radio_partition_read(4, RADIO_DISPLAY_LAYOUT_OFFSET, &previous, 1)) return RADIO_IO_ERROR;
    if ((previous != requested && store_native_layout(requested)) ||
        (saved == FOCUS_ON && write_focus_record(FOCUS_OFF))) {
        /* Keep the prior native mode as the fallback on any save error. */
        if (previous != requested) (void)store_native_layout(previous);
        FOCUS_STATE = saved == FOCUS_ON && read_focus_record() == FOCUS_OFF &&
                      previous <= RADIO_DISPLAY_CLASSIC ? FOCUS_STOCK_PENDING + previous : FOCUS_UNREAD;
        return RADIO_IO_ERROR;
    }
    FOCUS_STATE = FOCUS_STOCK_PENDING + requested;
    return RADIO_OK;
}

static int write_33cm(unsigned enabled)
{
    if (enabled > 1 || (enabled && BAND_REGION > 1)) return RADIO_INVALID;
    uint32_t record;
    uint32_t wanted = enabled ? RADIO_33CM_MAGIC : RADIO_33CM_OFF;
    int length = radio_vm_read(RADIO_33CM_VM_KEY, &record, sizeof record);
    /* The stock VM returns -252 for a missing record. Leave a foreign
     * record intact, including a record with a different length. */
    if (length != VM_MISSING &&
        (length != sizeof record || (record != RADIO_33CM_MAGIC && record != RADIO_33CM_OFF)))
        return RADIO_IO_ERROR;
    if (length == VM_MISSING || record != wanted) {
        if (radio_vm_write(RADIO_33CM_VM_KEY, &wanted, sizeof wanted) != sizeof wanted ||
            radio_vm_read(RADIO_33CM_VM_KEY, &record, sizeof record) != sizeof record ||
            record != wanted)
            return RADIO_IO_ERROR;
    }
    BAND_STATE = wanted;
    return RADIO_OK;
}
static int read_setting(unsigned offset, void *dst, unsigned length)
{
    if (offset == RADIO_DISPLAY_LAYOUT_OFFSET && length == 1 && radio_focus_enabled()) {
        *(uint8_t *)dst = RADIO_DISPLAY_FOCUS;
        return RADIO_OK;
    }
    if (offset == RADIO_33CM_SETTING) {
        if (length != 1) return RADIO_INVALID;
        *(uint8_t *)dst = BAND_REGION <= 1 && BAND_STATE == RADIO_33CM_MAGIC;
        return RADIO_OK;
    }
    if (offset == RADIO_THEME_SETTING) {
        if (length != 1) return RADIO_INVALID;
        *(uint8_t *)dst = THEME_STATE;
        return RADIO_OK;
    }
    if (offset >= 0x6000) return radio_bt_read(offset, dst, length);
    return radio_partition_read(4, offset, dst, length);
}
static int write_setting(unsigned offset, const void *src, unsigned length)
{
    if (offset == RADIO_DISPLAY_LAYOUT_OFFSET && length == 1)
        return write_display_layout(*(const uint8_t *)src);
    if (offset == RADIO_33CM_SETTING) {
        if (length != 1) return RADIO_INVALID;
        return write_33cm(*(const uint8_t *)src);
    }
    if (offset == RADIO_THEME_SETTING) {
        if (length != 1 || *(const uint8_t *)src >= RADIO_THEME_COUNT) return RADIO_INVALID;
        THEME_STATE = *(const uint8_t *)src;
        return RADIO_OK;
    }
    if (offset >= 0x6000) return radio_bt_write(offset, src, length);
    return radio_partition_write(4, offset, src, length);
}
const struct radio_storage radio_stock_storage = { read_setting, write_setting };
