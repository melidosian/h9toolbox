#include "h9_main_boot_record.h"

static uint16_t read_u16(const volatile uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t read_u32(const volatile uint8_t *bytes)
{
    return (uint32_t)read_u16(bytes) | ((uint32_t)read_u16(bytes + 2) << 16);
}

uint32_t h9_main_consume_boot_record(
    volatile uint8_t record[H9_MAIN_BOOT_RECORD_BYTES],
    H9MainBootRecordCheck check, void *context)
{
    uint32_t checked = check == NULL ? 0u :
        check(record + 2, H9_MAIN_BOOT_RECORD_CHECK_BYTES, context);
    uint32_t expected = read_u32(record);
    uint32_t low = checked != 0u && checked == expected ?
        (uint32_t)read_u16(record + 4) : 0x5A00u;
    uint32_t value = low | ((uint32_t)read_u16(record + 6) << 16);
    size_t index;

    for (index = 0; index < H9_MAIN_BOOT_RECORD_BYTES; ++index) {
        record[index] = 0;
    }
    return value;
}
