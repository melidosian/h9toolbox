#include "radio_settings.h"
#include "radio_bluetooth.h"

int radio_partition_read(unsigned partition, unsigned offset, void *dst, unsigned length);
int radio_partition_write(unsigned partition, unsigned offset, const void *src, unsigned length);
static int read_setting(unsigned offset, void *dst, unsigned length)
{
    if (offset >= 0x6000) return radio_bt_read(offset, dst, length);
    return radio_partition_read(4, offset, dst, length);
}
static int write_setting(unsigned offset, const void *src, unsigned length)
{
    if (offset >= 0x6000) return radio_bt_write(offset, src, length);
    return radio_partition_write(4, offset, src, length);
}
const struct radio_storage radio_stock_storage = { read_setting, write_setting };
