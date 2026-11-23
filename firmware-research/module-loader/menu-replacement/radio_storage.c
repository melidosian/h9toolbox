#include "radio_settings.h"
#include "radio_bluetooth.h"
#include "radio_33cm.h"

/* The main-image TX hooks use the cached VM setting. */
#define BAND_STATE (*(volatile uint32_t *)RADIO_33CM_STATE)
#define BAND_REGION (*(const volatile uint8_t *)RADIO_33CM_REGION)

int radio_partition_read(unsigned partition, unsigned offset, void *dst, unsigned length);
int radio_partition_write(unsigned partition, unsigned offset, const void *src, unsigned length);
int radio_vm_read(unsigned key, void *dst, unsigned length);
int radio_vm_write(unsigned key, const void *src, unsigned length);

static int write_33cm(unsigned enabled)
{
    if (enabled > 1 || (enabled && BAND_REGION > 1)) return RADIO_INVALID;
    uint32_t record;
    uint32_t wanted = enabled ? RADIO_33CM_MAGIC : RADIO_33CM_OFF;
    int length = radio_vm_read(RADIO_33CM_VM_KEY, &record, sizeof record);
    /* The stock VM returns -252 for a missing record. Leave a foreign
     * record intact, including a record with a different length. */
    if (length != -252 &&
        (length != sizeof record || (record != RADIO_33CM_MAGIC && record != RADIO_33CM_OFF)))
        return RADIO_IO_ERROR;
    if (length == -252 || record != wanted) {
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
    if (offset == RADIO_33CM_SETTING) {
        if (length != 1) return RADIO_INVALID;
        *(uint8_t *)dst = BAND_REGION <= 1 && BAND_STATE == RADIO_33CM_MAGIC;
        return RADIO_OK;
    }
    if (offset >= 0x6000) return radio_bt_read(offset, dst, length);
    return radio_partition_read(4, offset, dst, length);
}
static int write_setting(unsigned offset, const void *src, unsigned length)
{
    if (offset == RADIO_33CM_SETTING) {
        if (length != 1) return RADIO_INVALID;
        return write_33cm(*(const uint8_t *)src);
    }
    if (offset >= 0x6000) return radio_bt_write(offset, src, length);
    return radio_partition_write(4, offset, src, length);
}
const struct radio_storage radio_stock_storage = { read_setting, write_setting };
