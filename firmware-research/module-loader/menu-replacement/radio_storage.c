#include "radio_settings.h"
#include "radio_bluetooth.h"
#include "radio_33cm.h"

/* This is a session setting. Use the same word as the main-image TX hooks. */
#define BAND_STATE (*(volatile uint32_t *)RADIO_33CM_STATE)
#define BAND_REGION (*(const volatile uint8_t *)RADIO_33CM_REGION)

int radio_partition_read(unsigned partition, unsigned offset, void *dst, unsigned length);
int radio_partition_write(unsigned partition, unsigned offset, const void *src, unsigned length);
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
        unsigned enabled = *(const uint8_t *)src;
        if (enabled > 1 || (enabled && BAND_REGION > 1)) return RADIO_INVALID;
        BAND_STATE = enabled ? RADIO_33CM_MAGIC : 0;
        return RADIO_OK;
    }
    if (offset >= 0x6000) return radio_bt_write(offset, src, length);
    return radio_partition_write(4, offset, src, length);
}
const struct radio_storage radio_stock_storage = { read_setting, write_setting };
