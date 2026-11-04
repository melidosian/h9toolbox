#ifndef H9_MAIN_BOOT_RECORD_H
#define H9_MAIN_BOOT_RECORD_H

#include <stddef.h>
#include <stdint.h>

#define H9_MAIN_BOOT_RECORD_BYTES 80u
#define H9_MAIN_BOOT_RECORD_CHECK_BYTES 78u

/* The stock startup calls a hardware-backed helper on record bytes [2, 80).
 * It compares the helper's 16-bit result with the first record halfword. */
typedef uint32_t (*H9MainBootRecordCheck)(const volatile uint8_t *data,
                                          size_t length, void *context);

/* Port of stock app+0x05E494's decision and record clearing. The returned
 * word belongs at main RAM 0x11C3C after the board check is connected. */
uint32_t h9_main_consume_boot_record(
    volatile uint8_t record[H9_MAIN_BOOT_RECORD_BYTES],
    H9MainBootRecordCheck check, void *context);

#endif
